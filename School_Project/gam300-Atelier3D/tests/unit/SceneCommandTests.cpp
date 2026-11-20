#include "engine/editor/EditorScene.hpp"
#include "engine/history/SceneCommands.hpp"
#include "engine/history/UndoRedoManager.hpp"

#include "engine/logic/Blocks.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using Engine::Editor::EditorScene;
using Engine::Editor::GameObject;
using Engine::History::BlockEditCommand;
using Engine::History::CreateObjectCommand;
using Engine::History::DeleteObjectCommand;
using Engine::History::ICommand;
using Engine::History::MakeEditCommand;
using Engine::History::PropertyChangeCommand;
using Engine::History::SceneStateCommand;
using Engine::History::TransformCommand;
using Engine::History::UndoRedoManager;
using Engine::Logic::Block;
using Engine::Logic::LogicDesc;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;
using Engine::Logic::Script;
using Engine::Scene::TransformComponent;

// The undo commands with objects that have children: delete, duplicate and undo must bring back (or take away) the
// whole subtree, in the same places in the list

namespace
{
    // Lamp (top), Table (top) with Cup and Plate under it, Chair (top)
    EditorScene Room()
    {
        EditorScene scene;
        scene.SetDescription({.objects = {GameObject{.id = 1, .name = "Lamp"}, GameObject{.id = 2, .name = "Table"},
                                          GameObject{.id = 3, .name = "Cup", .parent = 2},
                                          GameObject{.id = 4, .name = "Plate", .parent = 2},
                                          GameObject{.id = 5, .name = "Chair"}}});
        return scene;
    }

    std::vector<std::uint32_t> Ids(const EditorScene& scene)
    {
        std::vector<std::uint32_t> ids;
        for (const GameObject& object : scene.Objects())
        {
            ids.push_back(object.id);
        }
        return ids;
    }
}

TEST(SceneCommands, UndoingADeleteBringsBackTheChildrenToo)
{
    EditorScene scene = Room();
    const auto before = scene.CaptureState().scene;
    UndoRedoManager history;

    history.Execute(std::make_unique<DeleteObjectCommand>(scene, 2));
    EXPECT_EQ(Ids(scene), (std::vector<std::uint32_t>{1, 5}));

    history.Undo();
    EXPECT_EQ(scene.CaptureState().scene, before);
    EXPECT_EQ(scene.Selected(), 2u);

    history.Redo();
    EXPECT_EQ(Ids(scene), (std::vector<std::uint32_t>{1, 5}));
}

TEST(SceneCommands, RedoingADuplicateBringsBackItsChildren)
{
    EditorScene scene = Room();
    const std::uint32_t copy = scene.Duplicate(2).id;
    const auto withCopy = scene.CaptureState().scene;
    UndoRedoManager history;
    history.Push(std::make_unique<CreateObjectCommand>(scene, copy));

    history.Undo();
    EXPECT_EQ(Ids(scene), (std::vector<std::uint32_t>{1, 2, 3, 4, 5}));

    history.Redo();
    EXPECT_EQ(scene.CaptureState().scene, withCopy);
    EXPECT_EQ(scene.Selected(), copy);
}

TEST(SceneCommands, ADeleteOfAnObjectThatIsGoneDoesNothing)
{
    EditorScene scene = Room();
    UndoRedoManager history;
    history.Execute(std::make_unique<DeleteObjectCommand>(scene, 42));
    history.Undo();
    EXPECT_EQ(Ids(scene), (std::vector<std::uint32_t>{1, 2, 3, 4, 5}));
}

TEST(SceneCommands, ASceneSnapshotUndoesObjectsAddedAndReordered)
{
    EditorScene scene = Room();
    const EditorScene::State before = scene.CaptureState();
    scene.CreateObject("Rug");
    ASSERT_TRUE(scene.SetParent(1, 5)); // the Lamp goes under the Chair
    const EditorScene::State after = scene.CaptureState();
    UndoRedoManager history;
    history.Push(std::make_unique<SceneStateCommand>(scene, before, after, "Edit scene"));

    history.Undo();
    EXPECT_EQ(scene.CaptureState(), before);
    history.Redo();
    EXPECT_EQ(scene.CaptureState(), after);
}

// ---------------------------------------------------------------- The history's limit

namespace
{
    // Adds its number to a total, so what was undone can be read off the total
    class AddCommand : public ICommand
    {
    public:
        AddCommand(int& total, int amount) : total_(total), amount_(amount) {}
        void Execute() override { total_ += amount_; }
        void Undo() override { total_ -= amount_; }
        std::string Name() const override { return "Add " + std::to_string(amount_); }

    private:
        int& total_;
        int amount_;
    };
}

TEST(UndoHistory, KeepsTheLast100Operations)
{
    EXPECT_EQ(UndoRedoManager{}.Capacity(), 100u);
    int total = 0;
    UndoRedoManager history;
    for (int i = 0; i < 100; ++i)
    {
        history.Execute(std::make_unique<AddCommand>(total, 1));
    }
    EXPECT_EQ(history.UndoCount(), 100u) << "exactly at the limit, none is lost";

    history.Execute(std::make_unique<AddCommand>(total, 1));
    EXPECT_EQ(history.UndoCount(), 100u) << "one more pushes the oldest out";
    int undone = 0;
    while (history.Undo())
    {
        ++undone;
    }
    EXPECT_EQ(undone, 100);
    EXPECT_EQ(total, 1) << "the oldest stays done: it can no longer be undone";
    EXPECT_FALSE(history.CanUndo());
    EXPECT_EQ(history.RedoCount(), 100u);
}

TEST(UndoHistory, TheOldestIsTheOneDropped)
{
    int total = 0;
    UndoRedoManager history(3);
    for (const int amount : {1, 10, 100, 1000})
    {
        history.Execute(std::make_unique<AddCommand>(total, amount));
    }
    ASSERT_EQ(history.UndoCount(), 3u);
    ASSERT_NE(history.NextUndo(), nullptr);
    EXPECT_EQ(history.NextUndo()->Name(), "Add 1000");
    while (history.Undo())
    {
    }
    EXPECT_EQ(total, 1) << "the 1 is what fell off the end";
    while (history.Redo())
    {
    }
    EXPECT_EQ(total, 1111) << "and the three kept all come back";
}

TEST(UndoHistory, ANewOperationClearsRedoAndNothingLeftIsNoProblem)
{
    int total = 0;
    UndoRedoManager history;
    EXPECT_FALSE(history.Undo());
    EXPECT_FALSE(history.Redo());
    history.Execute(std::make_unique<AddCommand>(total, 1));
    history.Execute(std::make_unique<AddCommand>(total, 2));
    history.Undo();
    EXPECT_TRUE(history.CanRedo());
    history.Execute(std::make_unique<AddCommand>(total, 5));
    EXPECT_FALSE(history.CanRedo());
    EXPECT_FALSE(history.Redo());
    EXPECT_EQ(total, 6);
}

// ---------------------------------------------------------------- Each kind of operation

namespace
{
    // A script: when Play starts, move, forever { turn }
    LogicDesc SomeBlocks()
    {
        Block forever = MakeBlock(Op::Forever, 3);
        forever.body = {MakeBlock(Op::TurnBy, 4)};
        return LogicDesc{.scripts = {Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::MoveForward, 2),
                                                       forever}}}};
    }

    // Commits what changed, as the editor does when a drag ends
    void Commit(EditorScene& scene, UndoRedoManager& history, EditorScene::State& committed)
    {
        EditorScene::State now = scene.CaptureState();
        if (auto command = MakeEditCommand(scene, committed, now))
        {
            history.Push(std::move(command));
        }
        committed = std::move(now);
    }

    // The Lamp with some blocks, and a history ready to record
    struct Blocks
    {
        EditorScene scene = Room();
        UndoRedoManager history;
        EditorScene::State committed;

        Blocks()
        {
            scene.Find(1)->logic = SomeBlocks();
            committed = scene.CaptureState();
        }

        std::vector<Block>& Stack() { return scene.Find(1)->logic->scripts.front().blocks; }
        void Commit() { ::Commit(scene, history, committed); }
    };
}

TEST(BlockUndo, PlacingABlockIsUndoneAndRedone)
{
    Blocks game;
    const LogicDesc before = *game.scene.Find(1)->logic;
    game.Stack().insert(game.Stack().begin() + 2, MakeBlock(Op::Say, 5));
    const LogicDesc after = *game.scene.Find(1)->logic;
    game.Commit();

    ASSERT_EQ(game.history.UndoCount(), 1u);
    EXPECT_EQ(game.history.NextUndo()->Name(), "Place block on Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, before);
    game.history.Redo();
    EXPECT_EQ(*game.scene.Find(1)->logic, after);
}

TEST(BlockUndo, DeletingABlockIsUndoneAndRedone)
{
    Blocks game;
    const LogicDesc before = *game.scene.Find(1)->logic;
    game.Stack().erase(game.Stack().begin() + 1);
    const LogicDesc after = *game.scene.Find(1)->logic;
    game.Commit();

    EXPECT_EQ(game.history.NextUndo()->Name(), "Delete block on Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, before);
    game.history.Redo();
    EXPECT_EQ(*game.scene.Find(1)->logic, after);
}

TEST(BlockUndo, DeletingABlockTakesWhatIsInsideItAndUndoBringsItBack)
{
    Blocks game;
    const LogicDesc before = *game.scene.Find(1)->logic;
    game.Stack().pop_back(); // the forever, and the turn inside it
    game.Commit();
    EXPECT_EQ(game.history.NextUndo()->Name(), "Delete 2 blocks on Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, before);
}

TEST(BlockUndo, ABlockPlacedInsideAnotherIsUndone)
{
    Blocks game;
    const LogicDesc before = *game.scene.Find(1)->logic;
    game.Stack().back().body.push_back(MakeBlock(Op::Say, 5));
    game.Commit();
    EXPECT_EQ(game.history.NextUndo()->Name(), "Place block on Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, before);
}

TEST(BlockUndo, TheFirstBlocksOfAnObjectAreUndoneToNoBlocksAtAll)
{
    EditorScene scene = Room();
    UndoRedoManager history;
    EditorScene::State committed = scene.CaptureState();
    ASSERT_FALSE(scene.Find(2)->logic.has_value());
    scene.Find(2)->logic = SomeBlocks();
    Commit(scene, history, committed);
    EXPECT_EQ(history.NextUndo()->Name(), "Place 4 blocks on Table");
    history.Undo();
    EXPECT_FALSE(scene.Find(2)->logic.has_value());
    history.Redo();
    EXPECT_EQ(*scene.Find(2)->logic, SomeBlocks());
}

TEST(BlockUndo, AWholeScriptDeletedComesBackInItsPlace)
{
    Blocks game;
    game.scene.Find(1)->logic->scripts.push_back(Script{.x = 500.0f, .blocks = {MakeBlock(Op::WhenPlayStarts, 9)}});
    game.Commit();
    const LogicDesc two = *game.scene.Find(1)->logic;
    game.scene.Find(1)->logic->scripts.erase(game.scene.Find(1)->logic->scripts.begin());
    game.Commit();
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, two);
}

TEST(BlockUndo, MovingABlockOrChangingAFieldIsOneEditStep)
{
    Blocks game;
    const LogicDesc before = *game.scene.Find(1)->logic;
    game.Stack()[1].numbers[0] = 7.0f;
    game.Commit();
    EXPECT_EQ(game.history.NextUndo()->Name(), "Edit blocks on Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1)->logic, before);
}

TEST(BlockUndo, NothingChangedRecordsNothing)
{
    Blocks game;
    game.Commit();
    EXPECT_FALSE(game.history.CanUndo());
}

// Blocks and something else changed together are one property step, undone together
TEST(BlockUndo, BlocksChangedWithSomethingElseAreOneStep)
{
    Blocks game;
    const GameObject before = *game.scene.Find(1);
    game.Stack().pop_back();
    game.scene.Find(1)->name = "Big Lamp";
    game.Commit();
    EXPECT_EQ(game.history.NextUndo()->Name(), "Edit Big Lamp");
    game.history.Undo();
    EXPECT_EQ(*game.scene.Find(1), before);
}

// The My Block window's blocks are not in the scene: its own history, on a stand-in
TEST(BlockUndo, BlocksOfAMyBlockWindowAreUndoneInItsOwnHistory)
{
    GameObject standIn;
    standIn.logic = SomeBlocks();
    const auto before = standIn.logic;
    standIn.logic->scripts.front().blocks.push_back(MakeBlock(Op::Say, 5));
    const auto after = standIn.logic;
    UndoRedoManager history;
    history.Push(std::make_unique<BlockEditCommand>([&standIn] { return &standIn.logic; }, before, after));
    EXPECT_EQ(history.NextUndo()->Name(), "Place block");
    history.Undo();
    EXPECT_EQ(standIn.logic, before);
    history.Redo();
    EXPECT_EQ(standIn.logic, after);
}

TEST(GizmoUndo, MoveRotateAndScaleAreEachUndoneAndRedone)
{
    EditorScene scene = Room();
    const TransformComponent start{.x = 1.0f, .y = 2.0f, .z = 3.0f};
    scene.Find(1)->transform = start;

    TransformComponent moved = start;
    moved.x = 9.0f;
    TransformComponent turned = moved;
    turned.rotation = 45.0f;
    turned.rotationX = 10.0f;
    TransformComponent scaled = turned;
    scaled.scale = 2.0f;
    scaled.scaleY = 3.0f;

    UndoRedoManager history;
    const auto drag = [&](const TransformComponent& from, const TransformComponent& to, const char* name)
    {
        scene.Find(1)->transform = to;
        history.Push(std::make_unique<TransformCommand>(scene, 1, from, to, name));
    };
    drag(start, moved, "Move Lamp");
    drag(moved, turned, "Rotate Lamp");
    drag(turned, scaled, "Scale Lamp");

    EXPECT_EQ(history.NextUndo()->Name(), "Scale Lamp");
    history.Undo();
    EXPECT_EQ(*scene.Find(1)->transform, turned);
    EXPECT_EQ(history.NextUndo()->Name(), "Rotate Lamp");
    history.Undo();
    EXPECT_EQ(*scene.Find(1)->transform, moved);
    EXPECT_EQ(history.NextUndo()->Name(), "Move Lamp");
    history.Undo();
    EXPECT_EQ(*scene.Find(1)->transform, start);
    history.Redo();
    history.Redo();
    history.Redo();
    EXPECT_EQ(*scene.Find(1)->transform, scaled);
}

TEST(PropertyUndo, ChangesToOneObjectOrSeveralAreOneStep)
{
    EditorScene scene = Room();
    UndoRedoManager history;
    EditorScene::State committed = scene.CaptureState();
    scene.Find(1)->name = "Floor Lamp";
    Commit(scene, history, committed);
    EXPECT_EQ(history.NextUndo()->Name(), "Edit Floor Lamp");

    scene.Find(2)->name = "Desk";
    scene.Find(5)->name = "Stool";
    Commit(scene, history, committed);
    EXPECT_EQ(history.NextUndo()->Name(), "Edit 2 objects");

    history.Undo();
    EXPECT_EQ(scene.Find(2)->name, "Table");
    EXPECT_EQ(scene.Find(5)->name, "Chair");
    EXPECT_EQ(scene.Find(1)->name, "Floor Lamp") << "the step before it is not touched";
    history.Undo();
    EXPECT_EQ(scene.Find(1)->name, "Lamp");
    history.Redo();
    history.Redo();
    EXPECT_EQ(scene.Find(2)->name, "Desk");
}

// ---------------------------------------------------------------- All of them together

// A session of every kind of edit: undoing it all passes back through each state exactly, and redoing it all forward
TEST(UndoHistory, EveryKindOfOperationUndoesAndRedoesInOrder)
{
    EditorScene scene = Room();
    scene.Find(1)->transform = TransformComponent{};
    scene.Find(1)->logic = SomeBlocks();
    UndoRedoManager history;
    EditorScene::State committed = scene.CaptureState();
    std::vector<EditorScene::State> states{committed};
    const auto record = [&] { states.push_back(committed = scene.CaptureState()); };

    // Create an object, the way the editor records one: the command keeps it, and running it puts it back
    const std::uint32_t rug = scene.CreateObject("Rug").id;
    auto create = std::make_unique<CreateObjectCommand>(scene, rug);
    scene.Remove(rug);
    history.Execute(std::move(create));
    record();
    // A gizmo drag
    TransformComponent moved;
    moved.x = 4.0f;
    const TransformComponent from = *scene.Find(1)->transform;
    scene.Find(1)->transform = moved;
    history.Push(std::make_unique<TransformCommand>(scene, 1, from, moved, "Move Lamp"));
    record();
    // A property
    scene.Find(5)->name = "Stool";
    Commit(scene, history, committed);
    states.push_back(committed);
    // Place a block
    scene.Find(1)->logic->scripts.front().blocks.push_back(MakeBlock(Op::Say, 5));
    Commit(scene, history, committed);
    states.push_back(committed);
    // Delete a block
    scene.Find(1)->logic->scripts.front().blocks.erase(scene.Find(1)->logic->scripts.front().blocks.begin() + 1);
    Commit(scene, history, committed);
    states.push_back(committed);
    // Delete an object with children
    history.Execute(std::make_unique<DeleteObjectCommand>(scene, 2));
    record();
    // Reparent (a snapshot)
    ASSERT_TRUE(scene.SetParent(1, 5));
    Commit(scene, history, committed);
    states.push_back(committed);

    ASSERT_EQ(history.UndoCount(), 7u);
    const auto same = [](const EditorScene::State& a, const EditorScene::State& b)
    { return a.scene.objects == b.scene.objects; };
    for (std::size_t i = states.size() - 1; i > 0; --i)
    {
        ASSERT_TRUE(history.Undo());
        EXPECT_TRUE(same(scene.CaptureState(), states[i - 1])) << "undo back to state " << i - 1;
    }
    EXPECT_FALSE(history.CanUndo());
    for (std::size_t i = 1; i < states.size(); ++i)
    {
        ASSERT_TRUE(history.Redo());
        EXPECT_TRUE(same(scene.CaptureState(), states[i])) << "redo on to state " << i;
    }
    EXPECT_FALSE(history.CanRedo());
}

// 150 operations: the oldest 50 are gone, and the last 100 undo and redo as ever
TEST(UndoHistory, AfterMoreThanTheLimitTheLast100StillWork)
{
    int total = 0;
    UndoRedoManager history;
    for (int i = 0; i < 150; ++i)
    {
        history.Execute(std::make_unique<AddCommand>(total, 1));
    }
    ASSERT_EQ(total, 150);
    EXPECT_EQ(history.UndoCount(), 100u);

    int undone = 0;
    while (history.Undo())
    {
        ++undone;
    }
    EXPECT_EQ(undone, 100);
    EXPECT_EQ(total, 50) << "the oldest 50 can no longer be undone";

    int redone = 0;
    while (history.Redo())
    {
        ++redone;
    }
    EXPECT_EQ(redone, 100);
    EXPECT_EQ(total, 150);
}
