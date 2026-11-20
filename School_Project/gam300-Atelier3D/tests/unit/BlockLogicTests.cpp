#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/input/Input.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/logic/Library.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/SceneFile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// The block language (Logic::Blocks): the kinds of block, where they fit, saving them with the scene, and running them
// in Play mode (Logic::LogicRunner)

using Engine::Logic::Block;
using Engine::Logic::InputsText;
using Engine::Logic::Library;
using Engine::Logic::MakeCall;
using Engine::Logic::ParseInputs;
using Engine::Logic::BlockShape;
using Engine::Logic::LogicDesc;
using Engine::Logic::LogicRunner;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;
using Engine::Logic::Script;
using Engine::Scene::GameObject;
using Engine::Scene::SceneDescription;
using Engine::Scene::TransformComponent;

namespace
{
    constexpr float kTolerance = 1e-4f;

    // A block with its numbers (and body, slots) set
    Block Make(Op op, std::uint32_t id, std::vector<float> numbers = {}, std::vector<Block> body = {})
    {
        Block block = MakeBlock(op, id);
        for (std::size_t i = 0; i < numbers.size() && i < block.numbers.size(); ++i)
        {
            block.numbers[i] = numbers[i];
        }
        block.body = std::move(body);
        return block;
    }

    Block KeyBlock(Op op, std::uint32_t id, const char* key)
    {
        Block block = MakeBlock(op, id);
        block.text = key;
        return block;
    }

    LogicDesc OneScript(std::vector<Block> blocks)
    {
        return LogicDesc{.scripts = {Script{.x = 10.0f, .y = 20.0f, .blocks = std::move(blocks)}}};
    }

    // Keys that are never pressed, or the ones in down / pressed this frame
    struct FakeKeys
    {
        std::set<int> down;
        std::set<int> pressed;

        LogicRunner::Keys Keys() const
        {
            return LogicRunner::Keys{.down = [this](int code) { return down.contains(code); },
                                     .pressed = [this](int code) { return pressed.contains(code); }};
        }
    };

    // An ECS world with one entity at the origin, and a runner for its logic
    class LogicRunnerTest : public ::testing::Test
    {
    protected:
        void SetUp() override { entity_ = ecs_.CreateEntity(); }

        void Run(std::vector<Block> blocks) { runner_.Set(1, "Cube", entity_.GetId(), OneScript(std::move(blocks))); }

        void Update(double seconds = 1.0 / 60.0) { runner_.Update(ecs_, seconds, keys_.Keys()); }

        TransformComponent Transform() const { return ecs_.GetTransform(entity_).value_or(TransformComponent{}); }

        Engine::Scene::Scene ecs_;
        Engine::Scene::Entity entity_;
        LogicRunner runner_;
        FakeKeys keys_;
    };
}

// ---------------------------------------------------------------- The kinds of block

// Every kind has its own name (for the file), a label with as many defaults as fields, and the right number of slots
TEST(Blocks, EveryKindIsDescribedOnce)
{
    std::set<std::string_view> names;
    for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
    {
        EXPECT_TRUE(names.insert(info.name).second) << info.name;
        EXPECT_EQ(Engine::Logic::OpNamed(info.name), info.op) << info.name;
        EXPECT_EQ(&Engine::Logic::InfoOf(info.op), &info);
        EXPECT_FALSE(info.help.empty()) << info.name;
        EXPECT_LE(Engine::Logic::NumberCount(info.op), 4u) << info.name;
        EXPECT_EQ(MakeBlock(info.op, 7).slots.size(), Engine::Logic::SlotCount(info.op)) << info.name;
        EXPECT_EQ(MakeBlock(info.op, 7).inputs.size(), Engine::Logic::NumberCount(info.op)) << info.name;
        EXPECT_FALSE(Engine::Logic::CategoryHelp(info.category).empty()) << info.name;
        if (info.hasElse)
        {
            EXPECT_TRUE(info.hasBody) << info.name;
        }
    }
    EXPECT_FALSE(Engine::Logic::OpNamed("teleport"));

    // The shapes say where blocks go
    EXPECT_EQ(Engine::Logic::InfoOf(Op::WhenPlayStarts).shape, BlockShape::Hat);
    EXPECT_EQ(Engine::Logic::InfoOf(Op::Forever).shape, BlockShape::Cap);
    EXPECT_EQ(Engine::Logic::InfoOf(Op::KeyDown).shape, BlockShape::Condition);
    EXPECT_EQ(Engine::Logic::SlotCount(Op::And), 2u);
    EXPECT_TRUE(Engine::Logic::HasKeyField(Op::WhenKeyPressed));
    EXPECT_TRUE(Engine::Logic::HasTextField(Op::Say));
    EXPECT_EQ(Engine::Logic::InfoOf(Op::Add).shape, BlockShape::Value);
    EXPECT_TRUE(Engine::Logic::HasVariableField(Op::SetVariable));

    // Every category has blocks, and loops, ifs, math and variables each have their own
    for (const Engine::Logic::Category category : Engine::Logic::kCategories)
    {
        EXPECT_TRUE(std::any_of(Engine::Logic::AllBlocks().begin(), Engine::Logic::AllBlocks().end(),
                                [category](const auto& info) { return info.category == category; }))
            << Engine::Logic::CategoryName(category);
    }
    EXPECT_EQ(Engine::Logic::InfoOf(Op::Repeat).category, Engine::Logic::Category::Loops);
    EXPECT_EQ(Engine::Logic::InfoOf(Op::IfElse).category, Engine::Logic::Category::Conditions);
    EXPECT_EQ(Engine::Logic::InfoOf(Op::Random).category, Engine::Logic::Category::Math);
    EXPECT_EQ(Engine::Logic::InfoOf(Op::ChangeVariable).category, Engine::Logic::Category::Variables);
}

// Blocks read as sentences with their values, for the Logic window and the problem messages
TEST(Blocks, BlocksReadAsSentences)
{
    EXPECT_EQ(Engine::Logic::Describe(Make(Op::MoveForward, 1, {2.0f})), "move forward 2");
    EXPECT_EQ(Engine::Logic::Describe(Make(Op::Wait, 1, {0.5f})), "wait 0.5 seconds");
    EXPECT_EQ(Engine::Logic::Describe(KeyBlock(Op::WhenKeyPressed, 1, "W")), "when W key pressed");
    Block test = MakeBlock(Op::If, 1);
    EXPECT_EQ(Engine::Logic::Describe(test), "if ( ) then");
    test.slots[0] = KeyBlock(Op::KeyDown, 2, "Space");
    EXPECT_EQ(Engine::Logic::Describe(test), "if (key Space pressed?) then");

    // A value block in place of a number
    Block turn = Make(Op::TurnBy, 3);
    turn.inputs[0] = Make(Op::Random, 4, {-30, 30});
    EXPECT_EQ(Engine::Logic::Describe(turn), "turn (pick random -30 to 30) degrees");
    Block set = MakeBlock(Op::SetVariable, 5);
    set.text = "score";
    EXPECT_EQ(Engine::Logic::Describe(set), "set score to 0");
}

// Ids: the next free one counts blocks inside others and in slots; a copy is renumbered so ids stay unique
TEST(Blocks, IdsStayUnique)
{
    Block test = MakeBlock(Op::If, 3);
    test.slots[0] = KeyBlock(Op::KeyDown, 9, "W");
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Repeat, 2, {3}, {test})});
    EXPECT_EQ(Engine::Logic::NextBlockId(logic), 10u);
    EXPECT_EQ(Engine::Logic::NextBlockId(LogicDesc{}), 1u);
    EXPECT_EQ(Engine::Logic::CountBlocks(logic), 4u);
    ASSERT_NE(Engine::Logic::FindBlock(logic, 9), nullptr);
    EXPECT_EQ(Engine::Logic::FindBlock(logic, 9)->op, Op::KeyDown);
    EXPECT_EQ(Engine::Logic::FindBlock(logic, 0), nullptr) << "0 is an empty slot, not a block";

    std::vector<Block> copy = logic.scripts[0].blocks;
    std::uint32_t next = Engine::Logic::NextBlockId(logic);
    Engine::Logic::Renumber(copy, next);
    EXPECT_EQ(next, 14u);
    EXPECT_EQ(copy[1].body[0].slots[0].id, 13u);
    logic.scripts.push_back(Script{.blocks = copy});
    EXPECT_EQ(Engine::Logic::CountBlocks(logic), 8u);
    EXPECT_EQ(Engine::Logic::NextBlockId(logic), 14u);
}

// The Viewport's logic badges show a script as running when any of its blocks is, even one in a C block or a slot
TEST(Blocks, AScriptContainsTheBlocksInsideIt)
{
    Block test = MakeBlock(Op::If, 3);
    test.slots[0] = KeyBlock(Op::KeyDown, 9, "W");
    const std::vector<Block> script{MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Repeat, 2, {3}, {test})};
    EXPECT_TRUE(Engine::Logic::ContainsAny(script, {1}));
    EXPECT_TRUE(Engine::Logic::ContainsAny(script, {3}));
    EXPECT_TRUE(Engine::Logic::ContainsAny(script, {7, 9}));
    EXPECT_FALSE(Engine::Logic::ContainsAny(script, {4, 8}));
    EXPECT_FALSE(Engine::Logic::ContainsAny(script, {}));
    EXPECT_FALSE(Engine::Logic::ContainsAny(script, {0})) << "0 is an empty slot, not a block";
}

// Illegal connections are refused: a hat only starts a script, nothing goes under a cap or above a hat, a cap only
// goes last, and conditions only go into slots
TEST(Blocks, BlocksOnlyFitWhereTheyMakeSense)
{
    using Engine::Logic::CanFillSlot;
    using Engine::Logic::CanInsert;
    const std::vector<Block> script{MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2)};
    const std::vector<Block> move{Make(Op::MoveForward, 3)};
    const std::vector<Block> hat{MakeBlock(Op::WhenKeyPressed, 4)};
    const std::vector<Block> forever{MakeBlock(Op::Forever, 5)};
    const std::vector<Block> condition{MakeBlock(Op::KeyDown, 6)};

    EXPECT_TRUE(CanInsert(script, 1, move, true));
    EXPECT_TRUE(CanInsert(script, 2, move, true));
    EXPECT_FALSE(CanInsert(script, 0, move, true)) << "nothing above a hat";
    EXPECT_FALSE(CanInsert(script, 1, hat, true)) << "a hat only starts a script";
    EXPECT_FALSE(CanInsert({}, 0, hat, false)) << "nor inside a C block";
    EXPECT_TRUE(CanInsert(move, 0, hat, true)) << "on top of a script without one";
    EXPECT_TRUE(CanInsert(script, 2, forever, true)) << "a cap goes last";
    EXPECT_FALSE(CanInsert(script, 1, forever, true)) << "but not with blocks under it";
    EXPECT_FALSE(CanInsert(forever, 1, move, true)) << "nothing under a cap";
    EXPECT_FALSE(CanInsert(script, 1, condition, true)) << "conditions only go into slots";
    EXPECT_TRUE(CanInsert({}, 0, move, false)) << "into an empty C block";
    EXPECT_FALSE(CanInsert(script, 5, move, true));
    EXPECT_FALSE(CanInsert(script, 1, {}, true));

    const std::vector<Block> value{Make(Op::Add, 8)};
    EXPECT_FALSE(CanInsert(script, 1, value, true)) << "values only go into numbers";
    const Block move3 = Make(Op::MoveForward, 9);
    EXPECT_TRUE(Engine::Logic::CanFillInput(move3.inputs[0], value[0]));
    EXPECT_FALSE(Engine::Logic::CanFillInput(move3.inputs[0], condition[0])) << "a condition is not a number";
    EXPECT_FALSE(Engine::Logic::CanFillInput(move3.inputs[0], move[0]));

    const Block test = MakeBlock(Op::If, 7);
    EXPECT_TRUE(CanFillSlot(test.slots[0], condition[0]));
    EXPECT_FALSE(CanFillSlot(test.slots[0], value[0])) << "a value is not a condition";
    EXPECT_FALSE(CanFillSlot(test.slots[0], move[0])) << "a stack block does not fit a slot";
    EXPECT_FALSE(CanFillSlot(condition[0], condition[0])) << "a slot takes one condition";
}

// Picking up a block takes the blocks under it along, as in Scratch
TEST(Blocks, PickingUpABlockTakesTheBlocksUnderIt)
{
    std::vector<Block> list{MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2), Make(Op::TurnBy, 3),
                            Make(Op::Wait, 4)};
    const std::vector<Block> taken = Engine::Logic::TakeStack(list, 2);
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_EQ(taken[0].id, 3u);
    EXPECT_EQ(taken[1].id, 4u);
    EXPECT_EQ(list.size(), 2u);
    EXPECT_THROW(Engine::Logic::TakeStack(list, 2), std::out_of_range);
}

// Variables: names are unique and not blank; renaming one renames it in every block that uses it
TEST(Blocks, VariablesAreNamedAndRenamed)
{
    LogicDesc logic;
    EXPECT_TRUE(Engine::Logic::CanAddVariable(logic, "score"));
    logic.variables.push_back({.name = "score", .value = 0.0f});
    EXPECT_FALSE(Engine::Logic::CanAddVariable(logic, "score")) << "already used";
    EXPECT_FALSE(Engine::Logic::CanAddVariable(logic, ""));
    EXPECT_FALSE(Engine::Logic::CanAddVariable(logic, "   "));
    EXPECT_FALSE(Engine::Logic::CanAddVariable(logic, std::string(33, 'x')));

    Block set = MakeBlock(Op::SetVariable, 2);
    set.text = "score";
    Block value = MakeBlock(Op::VariableValue, 4);
    value.text = "score";
    set.inputs[0] = value;
    logic.scripts.push_back(Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 1), set}});
    EXPECT_EQ(Engine::Logic::CountUses(logic, "score"), 2u);

    EXPECT_TRUE(Engine::Logic::RenameVariable(logic, "score", "points"));
    EXPECT_EQ(logic.variables[0].name, "points");
    EXPECT_EQ(logic.scripts[0].blocks[1].text, "points");
    EXPECT_EQ(logic.scripts[0].blocks[1].inputs[0].text, "points");
    EXPECT_EQ(Engine::Logic::CountUses(logic, "score"), 0u);
    EXPECT_FALSE(Engine::Logic::RenameVariable(logic, "missing", "other"));
    logic.variables.push_back({.name = "lives"});
    EXPECT_FALSE(Engine::Logic::RenameVariable(logic, "points", "lives")) << "the new name is taken";
    ASSERT_NE(Engine::Logic::FindVariable(logic, "lives"), nullptr);
}

// ---------------------------------------------------------------- Saved with the scene

namespace
{
    class BlockFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_block_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::filesystem::path dir_;
    };
}

// An object's scripts, C blocks within C blocks, conditions in slots, an empty slot and a loose condition all come
// back the same from the scene file
TEST_F(BlockFileTest, LogicRoundTripsThroughTheSceneFile)
{
    Block test = MakeBlock(Op::IfElse, 4);
    test.slots[0] = MakeBlock(Op::And, 5);
    test.slots[0].slots[0] = KeyBlock(Op::KeyDown, 6, "W");
    test.body = {Make(Op::MoveForward, 7, {0.5f})};
    test.elseBody = {Make(Op::TurnBy, 8, {-15.0f})};
    Block say = MakeBlock(Op::Say, 9);
    say.text = "Hello, \"world\"!";

    GameObject cube{.id = 1, .name = "Cube"};
    cube.transform.emplace();
    cube.logic = LogicDesc{.scripts = {
                               Script{.x = 12.5f,
                                      .y = -3.0f,
                                      .blocks = {KeyBlock(Op::WhenKeyPressed, 1, "Space"),
                                                 Make(Op::Repeat, 2, {4}, {test}), say, MakeBlock(Op::Forever, 10)}},
                               Script{.x = 200.0f, .y = 40.0f, .blocks = {MakeBlock(Op::Not, 11)}},
                               Script{.x = 0.0f, .y = 300.0f, .blocks = {Make(Op::GoTo, 12, {1, 2, 3})}},
                           }};
    // Variables, a value block in a number, and a value lying loose on the canvas
    cube.logic->variables = {{.name = "speed", .value = 2.5f}, {.name = "score", .value = 0.0f}};
    Block forward = Make(Op::MoveForward, 13);
    forward.inputs[0] = MakeBlock(Op::Multiply, 14);
    forward.inputs[0].inputs[0] = MakeBlock(Op::VariableValue, 15);
    forward.inputs[0].inputs[0].text = "speed";
    forward.inputs[0].numbers[1] = 2.0f;
    cube.logic->scripts[2].blocks.push_back(forward);
    cube.logic->scripts.push_back(Script{.x = 5.0f, .y = 5.0f, .blocks = {Make(Op::Random, 16, {1, 6})}});
    GameObject empty{.id = 2, .name = "No logic"};
    const SceneDescription scene{.objects = {cube, empty}};
    Engine::Scene::SaveSceneFile(dir_ / "main.json", scene);

    SceneDescription loaded;
    const auto report = Engine::Scene::LoadSceneFile(dir_ / "main.json", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_EQ(loaded, scene);
    EXPECT_FALSE(loaded.objects[1].logic) << "an object without blocks has no Logic";
}

// Hand-edited files are read forgivingly: unknown blocks, blocks where they cannot be, and repeated ids are fixed with
// warnings instead of failing the scene
TEST_F(BlockFileTest, BrokenBlocksAreLeftOutWithWarnings)
{
    std::ofstream(dir_ / "main.json") << std::format(R"({{ "version": {}, "objects": [ {{ "id": 1, "name": "Cube",
        "logic": {{ "scripts": [
            {{ "x": 0, "y": 0, "blocks": [
                {{ "id": 1, "block": "when_play_starts" }},
                {{ "id": 2, "block": "teleport" }},
                {{ "id": 3, "block": "when_key_pressed", "text": "W" }},
                {{ "id": 4, "block": "key_down", "text": "W" }},
                {{ "id": 2, "block": "if", "slots": [ {{ "id": 5, "block": "move_forward", "numbers": [1] }} ],
                  "body": [ {{ "id": 1, "block": "turn_by", "numbers": ["far"] }} ] }} ] }},
            {{ "x": 0, "y": 0, "blocks": [ {{ "id": 9, "block": "nonsense" }} ] }} ] }} }} ] }})",
                                                     Engine::Scene::kSceneFormatVersion);
    SceneDescription loaded;
    const auto report = Engine::Scene::LoadSceneFile(dir_ / "main.json", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    ASSERT_EQ(loaded.objects.size(), 1u);
    ASSERT_TRUE(loaded.objects[0].logic);
    const LogicDesc& logic = *loaded.objects[0].logic;
    ASSERT_EQ(logic.scripts.size(), 1u) << "a script with nothing usable is dropped";
    const std::vector<Block>& blocks = logic.scripts[0].blocks;
    ASSERT_EQ(blocks.size(), 2u) << "the unknown block, the second hat and the loose condition are left out";
    EXPECT_EQ(blocks[0].op, Op::WhenPlayStarts);
    EXPECT_EQ(blocks[1].op, Op::If);
    ASSERT_EQ(blocks[1].slots.size(), 1u);
    EXPECT_EQ(blocks[1].slots[0].id, 0u) << "a stack block in a slot is left out; the slot stays, empty";
    ASSERT_EQ(blocks[1].body.size(), 1u);
    EXPECT_FLOAT_EQ(blocks[1].body[0].numbers[0], 15.0f) << "the default for a number that is not one";
    EXPECT_NE(blocks[1].body[0].id, 1u) << "id 1 was used twice: it gets a new one";
    EXPECT_GE(report.warnings.size(), 7u);
}

// ---------------------------------------------------------------- Running

// "when Play starts" runs once, on the first update; a block moves the object the way it faces
TEST_F(LogicRunnerTest, WhenPlayStartsRunsOnce)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2, {2}), Make(Op::TurnBy, 3, {90}),
         Make(Op::MoveForward, 4, {1})});
    Update();
    // Forward is -Z at rotation 0; after a quarter turn it is -X
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance);
    EXPECT_NEAR(Transform().x, -1.0f, kTolerance);
    EXPECT_NEAR(Transform().rotation, 90.0f, kTolerance);
    EXPECT_EQ(runner_.RunningScripts(1), 0u);
    Update();
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance) << "not again";
}

// A loop goes round once per frame, so learners see each step (the last one too); "repeat" stops after its count
TEST_F(LogicRunnerTest, RepeatGoesRoundOncePerFrame)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Repeat, 2, {3}, {Make(Op::MoveBy, 3, {1, 0, 0})}),
         Make(Op::GoTo, 4, {0, 5, 0})});
    Update();
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance);
    EXPECT_EQ(runner_.RunningScripts(1), 1u);
    Update();
    EXPECT_NEAR(Transform().x, 2.0f, kTolerance);
    Update();
    EXPECT_NEAR(Transform().x, 3.0f, kTolerance) << "the third round is shown for a frame";
    EXPECT_EQ(runner_.RunningScripts(1), 1u);
    Update();
    EXPECT_NEAR(Transform().y, 5.0f, kTolerance) << "after the third round, on to the next block";
    EXPECT_EQ(runner_.RunningScripts(1), 0u);
}

// "wait" holds the script across frames, and the block waited on counts as running (for highlighting)
TEST_F(LogicRunnerTest, WaitHoldsTheScript)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {1}), Make(Op::MoveBy, 3, {0, 1, 0})});
    Update(0.5); // the wait starts: its second is counted from the next frame on
    EXPECT_NEAR(Transform().y, 0.0f, kTolerance);
    const auto active = runner_.ActiveBlocks(1);
    EXPECT_TRUE(std::find(active.begin(), active.end(), 2u) != active.end());
    Update(0.6);
    EXPECT_NEAR(Transform().y, 0.0f, kTolerance);
    Update(0.5);
    EXPECT_NEAR(Transform().y, 1.0f, kTolerance);

    // A while later nothing has run: nothing glows
    Update(1.0);
    EXPECT_TRUE(runner_.ActiveBlocks(1).empty());
}

// "forever" never ends by itself; "stop this script" does
TEST_F(LogicRunnerTest, ForeverRunsUntilStop)
{
    Block test = MakeBlock(Op::If, 4);
    test.slots[0] = KeyBlock(Op::KeyDown, 5, "Space");
    test.body = {MakeBlock(Op::Stop, 6)};
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {Make(Op::TurnBy, 3, {10}), test})});
    for (int i = 0; i < 5; ++i)
    {
        Update();
    }
    EXPECT_NEAR(Transform().rotation, 50.0f, kTolerance);
    EXPECT_EQ(runner_.RunningScripts(1), 1u);

    keys_.down.insert(Engine::Input::Key_Space);
    Update();
    EXPECT_EQ(runner_.RunningScripts(1), 0u);
    Update();
    EXPECT_NEAR(Transform().rotation, 60.0f, kTolerance);
}

// "when key pressed" runs each time the key goes down, not while it is held
TEST_F(LogicRunnerTest, WhenKeyPressedRunsOnEachPress)
{
    Run({KeyBlock(Op::WhenKeyPressed, 1, "W"), Make(Op::MoveForward, 2, {1})});
    Update();
    EXPECT_NEAR(Transform().z, 0.0f, kTolerance);
    keys_.pressed.insert(Engine::Input::Key_W);
    Update();
    EXPECT_NEAR(Transform().z, -1.0f, kTolerance);
    keys_.pressed.clear();
    keys_.down.insert(Engine::Input::Key_W); // held
    Update();
    EXPECT_NEAR(Transform().z, -1.0f, kTolerance);
    keys_.pressed.insert(Engine::Input::Key_W);
    Update();
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance);
}

// A key tapped on a frame without an update (blocks run in fixed steps) still starts its script, once
TEST_F(LogicRunnerTest, AKeyTappedBetweenUpdatesStillStartsItsScript)
{
    Run({KeyBlock(Op::WhenKeyPressed, 1, "W"), Make(Op::MoveForward, 2, {1})});
    Update();
    keys_.pressed.insert(Engine::Input::Key_W);
    runner_.NoteKeyPresses(keys_.Keys());
    keys_.pressed.clear();
    EXPECT_NEAR(Transform().z, 0.0f, kTolerance);
    Update();
    EXPECT_NEAR(Transform().z, -1.0f, kTolerance);
    Update();
    EXPECT_NEAR(Transform().z, -1.0f, kTolerance) << "once for each press";
}

// Conditions: if-else takes one arm or the other; and / or / not combine; an empty slot is false
TEST_F(LogicRunnerTest, ConditionsChooseWhatRuns)
{
    Block choose = MakeBlock(Op::IfElse, 2);
    choose.slots[0] = MakeBlock(Op::And, 3);
    choose.slots[0].slots[0] = KeyBlock(Op::KeyDown, 4, "A");
    choose.slots[0].slots[1] = MakeBlock(Op::Not, 5);
    choose.slots[0].slots[1].slots[0] = KeyBlock(Op::KeyDown, 6, "B");
    choose.body = {Make(Op::MoveBy, 7, {1, 0, 0})};
    choose.elseBody = {Make(Op::MoveBy, 8, {0, 0, 1})};
    Block empty = MakeBlock(Op::If, 9);
    empty.body = {Make(Op::GoTo, 10, {9, 9, 9})};
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 11, {}, {choose, empty})});

    Update();
    EXPECT_NEAR(Transform().z, 1.0f, kTolerance) << "A is not held: else";
    keys_.down.insert(Engine::Input::Key_A);
    Update();
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance) << "A held and not B: then";
    keys_.down.insert(Engine::Input::Key_B);
    Update();
    EXPECT_NEAR(Transform().z, 2.0f, kTolerance) << "B held too: else";
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance) << "the empty If never ran its blocks";
}

// A block that needs a component the object lacks does nothing, and is reported once in plain words
// Looks and Health act on the object's components; "say" is kept for the Logic window
// A child moves relative to its parent, as the Inspector shows it (HierarchySystem places it in the world)
TEST_F(LogicRunnerTest, AChildMovesRelativeToItsParent)
{
    const Engine::Scene::Entity parent = ecs_.CreateEntity();
    ecs_.AddComponent(entity_, Engine::Scene::HierarchyComponent{.parent = parent.GetId(), .local = {.x = 1.0f}});
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {2, 0, 0})});
    Update();
    EXPECT_FLOAT_EQ(ecs_.TryGetComponent<Engine::Scene::HierarchyComponent>(entity_)->local.x, 3.0f);
}

// Looks blocks on a group change everything in it (members of members too); the group needs nothing to draw itself
TEST_F(LogicRunnerTest, LooksBlocksOnAGroupChangeItsMembers)
{
    const Engine::Scene::Entity child = ecs_.CreateEntity();
    const Engine::Scene::Entity grandchild = ecs_.CreateEntity();
    ecs_.AddComponent(child, Engine::Scene::HierarchyComponent{.parent = entity_.GetId()});
    ecs_.AddComponent(grandchild, Engine::Scene::HierarchyComponent{.parent = child.GetId()});
    for (const Engine::Scene::Entity member : {child, grandchild})
    {
        ecs_.AddComponent(member, Engine::Scene::MeshRendererComponent{});
    }
    Run({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::Hide, 2), Make(Op::SetColor, 3, {0, 1, 0})});
    Update();
    for (const Engine::Scene::Entity member : {child, grandchild})
    {
        const auto* mesh = ecs_.TryGetComponent<Engine::Scene::MeshRendererComponent>(member);
        ASSERT_NE(mesh, nullptr);
        EXPECT_FALSE(mesh->visible);
        EXPECT_FLOAT_EQ(mesh->albedoG, 1.0f);
        EXPECT_FLOAT_EQ(mesh->albedoR, 0.0f);
    }
    EXPECT_TRUE(runner_.Problems(1).empty()) << "a group has nothing to draw itself, and that is fine";
}

// Only scripts under a hat run; setting an object's logic again starts it from the top; removing it stops it
TEST_F(LogicRunnerTest, LooseBlocksDoNotRunAndEditsRestart)
{
    LogicDesc logic = OneScript({Make(Op::MoveBy, 1, {1, 0, 0})});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_NEAR(Transform().x, 0.0f, kTolerance);
    EXPECT_TRUE(runner_.Has(1));

    logic.scripts[0].blocks.insert(logic.scripts[0].blocks.begin(), MakeBlock(Op::WhenPlayStarts, 2));
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance);
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_NEAR(Transform().x, 2.0f, kTolerance) << "restarted";

    runner_.Remove(1);
    EXPECT_FALSE(runner_.Has(1));
    EXPECT_TRUE(runner_.ActiveBlocks(1).empty());
}

// A long script without a wait or a loop cannot freeze the game: it stops at the limit, and the learner is told
TEST_F(LogicRunnerTest, LongScriptsPauseAtTheStepLimit)
{
    std::vector<Block> blocks{MakeBlock(Op::WhenPlayStarts, 1)};
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(LogicRunner::kMaxSteps) + 10; ++i)
    {
        blocks.push_back(Make(Op::MoveBy, i + 2, {1, 0, 0}));
    }
    Run(std::move(blocks));
    Update();
    EXPECT_NEAR(Transform().x, static_cast<float>(LogicRunner::kMaxSteps), 0.5f);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().reason, Engine::Logic::PauseReason::InfiniteLoop);
    Update();
    EXPECT_NEAR(Transform().x, static_cast<float>(LogicRunner::kMaxSteps), 0.5f) << "nothing runs while paused";
}

// ---------------------------------------------------------------- In the game

// The runtime runs each object's logic on its entity, and live edits start an edited object's scripts again
TEST(BlockRuntime, TheRuntimeRunsEachObjectsLogic)
{
    const auto dir = std::filesystem::temp_directory_path() / "atelier_block_tests" / "runtime";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "configs");
    std::ofstream(dir / "configs" / "game.cfg") << "name=block_test\n";

    Engine::Game::GameRuntime runtime;
    runtime.SetSimulating(false);
    runtime.LoadProject(Engine::Game::GameProject(dir));
    GameObject cube{.id = 4, .name = "Cube"};
    cube.transform.emplace();
    cube.logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0})});
    GameObject plain{.id = 5, .name = "Plain"};
    ASSERT_TRUE(runtime.Start(SceneDescription{.objects = {cube, plain}}));
    EXPECT_TRUE(runtime.Blocks().Has(4));
    EXPECT_FALSE(runtime.Blocks().Has(5));

    // Blocks added to the other object, and the cube's taken away
    plain.logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1)});
    cube.logic.reset();
    runtime.UpdateScene(SceneDescription{.objects = {cube, plain}});
    EXPECT_FALSE(runtime.Blocks().Has(4));
    EXPECT_TRUE(runtime.Blocks().Has(5));

    runtime.UpdateScene(SceneDescription{.objects = {cube}});
    EXPECT_FALSE(runtime.Blocks().Has(5)) << "a removed object's logic stops";
    runtime.Stop();
    EXPECT_FALSE(runtime.Blocks().Has(4));
    std::filesystem::remove_all(dir);
}

// ---------------------------------------------------------------- Variables, values and more

namespace
{
    Block Variable(Op op, std::uint32_t id, const char* name, std::vector<float> numbers = {})
    {
        Block block = Make(op, id, std::move(numbers));
        block.text = name;
        return block;
    }

    // The block with input i replaced by value
    Block With(Block block, std::size_t i, Block value)
    {
        block.inputs.at(i) = std::move(value);
        return block;
    }
}

// Variables start from their values, change, and can be used in any number
TEST_F(LogicRunnerTest, VariablesAreSetChangedAndUsed)
{
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), Variable(Op::ChangeVariable, 2, "speed", {3}),
                                 With(Make(Op::MoveBy, 3, {0, 0, 0}), 0, Variable(Op::VariableValue, 4, "speed")),
                                 With(Variable(Op::SetVariable, 5, "score"), 0,
                                      With(With(MakeBlock(Op::Multiply, 6), 0, Variable(Op::VariableValue, 7, "speed")),
                                           1, Make(Op::Add, 8, {1, 1})))});
    logic.variables = {{.name = "speed", .value = 2.0f}, {.name = "score", .value = 0.0f}};
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_NEAR(Transform().x, 5.0f, kTolerance) << "speed 2, changed by 3";
    const auto variables = runner_.Variables(1);
    ASSERT_EQ(variables.size(), 2u);
    EXPECT_EQ(variables[0].first, "speed");
    EXPECT_DOUBLE_EQ(variables[0].second, 5.0);
    EXPECT_DOUBLE_EQ(variables[1].second, 10.0) << "5 * (1 + 1)";
    EXPECT_TRUE(runner_.Problems(1).empty());

    // Set again (an edit, or Play pressed again): back to the starting values
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    EXPECT_DOUBLE_EQ(runner_.Variables(1)[0].second, 2.0);
}

// Math blocks: arithmetic, mod that stays positive, rounding, random numbers in their range; dividing by zero is a
// problem that gives 0
TEST_F(LogicRunnerTest, MathBlocksWorkOutNumbers)
{
    LogicDesc logic = OneScript(
        {MakeBlock(Op::WhenPlayStarts, 1), With(Variable(Op::SetVariable, 2, "a"), 0, Make(Op::Mod, 3, {-1, 3})),
         With(Variable(Op::SetVariable, 4, "b"), 0, With(Make(Op::Round, 5), 0, Make(Op::Divide, 6, {7, 2}))),
         With(Variable(Op::SetVariable, 7, "c"), 0, Make(Op::Abs, 8, {-4})),
         With(Variable(Op::SetVariable, 9, "d"), 0, Make(Op::Subtract, 10, {1, 3})),
         With(Variable(Op::SetVariable, 11, "e"), 0, Make(Op::Divide, 12, {1, 0}))});
    for (const char* name : {"a", "b", "c", "d", "e"})
    {
        logic.variables.push_back({.name = name, .value = 99.0f});
    }
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    const auto variables = runner_.Variables(1);
    EXPECT_DOUBLE_EQ(variables[0].second, 2.0) << "-1 mod 3";
    EXPECT_DOUBLE_EQ(variables[1].second, 4.0) << "round 3.5";
    EXPECT_DOUBLE_EQ(variables[2].second, 4.0);
    EXPECT_DOUBLE_EQ(variables[3].second, -2.0);
    EXPECT_DOUBLE_EQ(variables[4].second, 0.0);
    const auto problems = runner_.Problems(1);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems[0].block, 12u);
    EXPECT_NE(problems[0].message.find("zero"), std::string::npos);

    // pick random 1 to 6: whole numbers, every one of them in time, never outside
    LogicDesc dice = OneScript(
        {MakeBlock(Op::WhenPlayStarts, 1),
         Make(Op::Forever, 2, {}, {With(Variable(Op::SetVariable, 3, "roll"), 0, Make(Op::Random, 4, {1, 6}))})});
    dice.variables.push_back({.name = "roll"});
    runner_.Set(1, "Cube", entity_.GetId(), dice);
    std::set<double> seen;
    for (int i = 0; i < 300; ++i)
    {
        Update();
        const double roll = runner_.Variables(1)[0].second;
        EXPECT_TRUE(roll >= 1.0 && roll <= 6.0 && roll == std::round(roll)) << roll;
        seen.insert(roll);
    }
    EXPECT_EQ(seen.size(), 6u);
}

// "move right" moves sideways, to the object's right whichever way it faces
TEST_F(LogicRunnerTest, MoveRightGoesSideways)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveRight, 2, {1}), Make(Op::TurnBy, 3, {90}),
         Make(Op::MoveRight, 4, {2})});
    Update();
    // Facing -Z its right is +X; turned a quarter to the left (facing -X), its right is -Z
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance);
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance);
}

// "repeat until" goes round once per frame until its condition (here a comparison of a sensed value) is true
TEST_F(LogicRunnerTest, RepeatUntilStopsWhenTheConditionIsTrue)
{
    Block until = MakeBlock(Op::RepeatUntil, 2);
    until.slots[0] = With(Make(Op::Greater, 3, {0, 3}), 0, MakeBlock(Op::PositionX, 4));
    until.body = {Make(Op::MoveBy, 5, {1, 0, 0})};
    Run({MakeBlock(Op::WhenPlayStarts, 1), until, Make(Op::GoTo, 6, {0, 7, 0})});
    for (int i = 0; i < 3; ++i)
    {
        Update();
    }
    EXPECT_NEAR(Transform().x, 3.0f, kTolerance);
    EXPECT_EQ(runner_.RunningScripts(1), 1u) << "3 is not > 3 yet";
    Update();
    EXPECT_NEAR(Transform().x, 4.0f, kTolerance);
    Update();
    EXPECT_NEAR(Transform().y, 7.0f, kTolerance) << "4 > 3: on to the next block";
    EXPECT_EQ(runner_.RunningScripts(1), 0u);

    // = and < too
    Block test = MakeBlock(Op::IfElse, 7);
    test.slots[0] = With(Make(Op::Equal, 8, {0, 7}), 0, MakeBlock(Op::PositionY, 9));
    test.body = {Make(Op::MoveBy, 10, {0, 0, 1})};
    test.elseBody = {Make(Op::MoveBy, 11, {0, 0, -1})};
    Run({MakeBlock(Op::WhenPlayStarts, 1), test});
    Update();
    EXPECT_NEAR(Transform().z, 1.0f, kTolerance) << "y is 7";
}

// "glide" moves the object smoothly over its seconds, holding the script until it gets there
TEST_F(LogicRunnerTest, GlideMovesOverTime)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::GlideTo, 2, {1, 10, 0, 0}), Make(Op::MoveBy, 3, {0, 1, 0})});
    Update(0.5); // starts: it moves from the next frame on
    EXPECT_NEAR(Transform().x, 0.0f, kTolerance);
    Update(0.5);
    EXPECT_NEAR(Transform().x, 5.0f, kTolerance);
    EXPECT_NEAR(Transform().y, 0.0f, kTolerance);
    const auto active = runner_.ActiveBlocks(1);
    EXPECT_TRUE(std::find(active.begin(), active.end(), 2u) != active.end()) << "the glide glows while it moves";
    Update(0.5);
    EXPECT_NEAR(Transform().x, 10.0f, kTolerance);
    EXPECT_NEAR(Transform().y, 1.0f, kTolerance) << "there: on to the next block in the same frame";
}

// A broadcast starts every "when I receive" script for the message on the next frame, in any object; a receiver
// that is still running starts again from the top
TEST_F(LogicRunnerTest, BroadcastsStartReceivers)
{
    const Engine::Scene::Entity other = ecs_.CreateEntity();
    Block send = MakeBlock(Op::Broadcast, 2);
    send.text = "go";
    runner_.Set(1, "Sender", entity_.GetId(), OneScript({KeyBlock(Op::WhenKeyPressed, 1, "Space"), send}));
    Block hear = MakeBlock(Op::WhenIReceive, 1);
    hear.text = "GO";
    runner_.Set(2, "Receiver", other.GetId(), OneScript({hear, Make(Op::MoveBy, 2, {1, 0, 0})}));

    Update();
    EXPECT_NEAR(ecs_.GetTransform(other)->x, 0.0f, kTolerance) << "nothing sent yet";
    keys_.pressed.insert(Engine::Input::Key_Space);
    Update();
    keys_.pressed.clear();
    EXPECT_NEAR(ecs_.GetTransform(other)->x, 0.0f, kTolerance) << "heard on the next frame";
    Update();
    EXPECT_NEAR(ecs_.GetTransform(other)->x, 1.0f, kTolerance) << "messages ignore case";
    Update();
    EXPECT_NEAR(ecs_.GetTransform(other)->x, 1.0f, kTolerance) << "once per broadcast";
}

// Blocks find other objects by name: point towards one, go to it, measure the distance to it; a name no object has
// is a problem
TEST_F(LogicRunnerTest, BlocksFindOtherObjectsByName)
{
    const Engine::Scene::Entity target = ecs_.CreateEntity();
    ecs_.SetTransform(target, TransformComponent{.x = 10.0f});
    runner_.SetObjects({{.name = "Cube", .entity = entity_.GetId()}, {.name = "Target", .entity = target.GetId()}});

    Block point = MakeBlock(Op::PointTowards, 2);
    point.text = "target"; // names ignore case when nothing matches exactly
    Block distance = MakeBlock(Op::DistanceTo, 5);
    distance.text = "Target";
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), point, Make(Op::MoveForward, 3, {1}),
                                 With(Variable(Op::SetVariable, 4, "far"), 0, distance)});
    logic.variables = {{.name = "far"}};
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_NEAR(Transform().rotation, -90.0f, kTolerance);
    EXPECT_NEAR(Transform().x, 1.0f, kTolerance) << "it faces the target now";
    EXPECT_NEAR(runner_.Variables(1)[0].second, 9.0, 1e-4);

    Block go = MakeBlock(Op::GoToObject, 2);
    go.text = "Target";
    Block lost = MakeBlock(Op::GoToObject, 3);
    lost.text = "Nobody";
    Run({MakeBlock(Op::WhenPlayStarts, 1), go, lost});
    Update();
    EXPECT_NEAR(Transform().x, 10.0f, kTolerance);
    const auto problems = runner_.Problems(1);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].message.find("no object called \"Nobody\""), std::string::npos) << problems[0].message;
}

// The timer counts seconds of Play and can be reset; a variable that does not exist is a problem
TEST_F(LogicRunnerTest, TimerAndMissingVariables)
{
    LogicDesc logic =
        OneScript({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {0.5}),
                   With(Variable(Op::SetVariable, 3, "t"), 0, MakeBlock(Op::Timer, 4)), MakeBlock(Op::ResetTimer, 5),
                   With(Variable(Op::SetVariable, 6, "u"), 0, MakeBlock(Op::Timer, 7)),
                   Variable(Op::ChangeVariable, 8, "missing", {1})});
    logic.variables = {{.name = "t"}, {.name = "u", .value = 5.0f}};
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update(0.25);
    Update(0.25);
    Update(0.25);
    const auto variables = runner_.Variables(1);
    EXPECT_NEAR(variables[0].second, 0.75, 1e-9);
    EXPECT_NEAR(variables[1].second, 0.0, 1e-9) << "just reset";
    const auto problems = runner_.Problems(1);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].message.find("no variable called \"missing\""), std::string::npos);
}

// Hand-edited variables: a repeated or missing name is left out, and only a value block fits in a number
TEST_F(BlockFileTest, BrokenVariablesAndValuesAreLeftOut)
{
    std::ofstream(dir_ / "main.json") << std::format(R"({{ "version": {}, "objects": [ {{ "id": 1, "name": "Cube",
        "logic": {{ "variables": [ {{ "name": "score", "value": 3 }}, {{ "name": "score" }}, {{ "value": 1 }} ],
          "scripts": [ {{ "x": 0, "y": 0, "blocks": [
            {{ "id": 1, "block": "when_play_starts" }},
            {{ "id": 2, "block": "move_forward", "numbers": [2],
              "inputs": [ {{ "id": 3, "block": "turn_by" }} ] }},
            {{ "id": 4, "block": "turn_by", "inputs": [ {{ "id": 5, "block": "variable", "text": "score" }} ] }} ] }}
          ] }} }} ] }})",
                                                     Engine::Scene::kSceneFormatVersion);
    SceneDescription loaded;
    const auto report = Engine::Scene::LoadSceneFile(dir_ / "main.json", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    const LogicDesc& logic = *loaded.objects[0].logic;
    ASSERT_EQ(logic.variables.size(), 1u);
    EXPECT_EQ(logic.variables[0], (Engine::Logic::Variable{.name = "score", .value = 3.0f}));
    const std::vector<Block>& blocks = logic.scripts[0].blocks;
    ASSERT_EQ(blocks.size(), 3u);
    EXPECT_EQ(blocks[1].inputs[0].id, 0u) << "a stack block is not a number";
    EXPECT_FLOAT_EQ(blocks[1].numbers[0], 2.0f) << "the number stays";
    EXPECT_EQ(blocks[2].inputs[0].op, Op::VariableValue);
    EXPECT_EQ(blocks[2].inputs[0].text, "score");
    EXPECT_EQ(report.warnings.size(), 4u)
        << "the repeated name; the missing name (missing, and left out); the turn block";
}

// ---------------------------------------------------------------- The block library

namespace
{
    Engine::Logic::Library MoveLibrary()
    {
        return Engine::Logic::Library{
            .blocks = {{.name = "Move", .blocks = {Make(Op::MoveForward, 1, {2}), Make(Op::TurnBy, 2, {90})}}}};
    }
}

// Save to Library: a long stack becomes one block, "when Play starts > Movement Logic", and the saved blocks get ids of
// their own; saving under a name already used replaces that library block
TEST(BlockLibrary, SavingAStackLeavesOneBlockInItsPlace)
{
    Engine::Logic::Library library;
    std::vector<Block> script{MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 5), Make(Op::TurnBy, 7)};
    ASSERT_TRUE(Engine::Logic::SaveToLibrary(library, script, 0, "Movement Logic", 9)) << "from the hat: what is under";
    ASSERT_EQ(script.size(), 2u);
    EXPECT_EQ(script[0].op, Op::WhenPlayStarts);
    EXPECT_EQ(script[1].op, Op::Call);
    EXPECT_EQ(script[1].id, 9u);
    EXPECT_EQ(script[1].text, "Movement Logic");
    EXPECT_EQ(Engine::Logic::Describe(script[1]), "Movement Logic");
    const Engine::Logic::LibraryBlock* saved = library.Find("Movement Logic");
    ASSERT_NE(saved, nullptr);
    ASSERT_EQ(saved->blocks.size(), 2u);
    EXPECT_EQ(saved->blocks[0].id, 1u);
    EXPECT_EQ(saved->blocks[1].op, Op::TurnBy);

    // Again under the same name, from the call: it replaces the saved blocks
    script.push_back(Make(Op::Say, 10));
    ASSERT_TRUE(Engine::Logic::SaveToLibrary(library, script, 1, "Movement Logic", 11));
    ASSERT_EQ(library.blocks.size(), 1u);
    EXPECT_EQ(library.blocks[0].blocks.size(), 2u) << "the old call and the say";

    // Nothing to save, or no name
    std::vector<Block> hat{MakeBlock(Op::WhenPlayStarts, 1)};
    EXPECT_FALSE(Engine::Logic::SaveToLibrary(library, hat, 0, "Empty", 2));
    EXPECT_FALSE(Engine::Logic::SaveToLibrary(library, script, 1, "   ", 12));
    EXPECT_FALSE(Engine::Logic::SaveToLibrary(library, script, 5, "Far", 12));
}

// Unpack puts a library block's blocks back in its place, with new ids, to change them
TEST(BlockLibrary, UnpackPutsTheBlocksBack)
{
    const Engine::Logic::Library library = MoveLibrary();
    std::vector<Block> script{MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Move", 2), Make(Op::Say, 3)};
    std::uint32_t next = 10;
    ASSERT_TRUE(Engine::Logic::UnpackCall(library, script, 1, next));
    ASSERT_EQ(script.size(), 4u);
    EXPECT_EQ(script[1].op, Op::MoveForward);
    EXPECT_EQ(script[1].id, 10u);
    EXPECT_EQ(script[2].id, 11u);
    EXPECT_EQ(script[3].op, Op::Say);
    EXPECT_FALSE(Engine::Logic::UnpackCall(library, script, 1, next)) << "not a library block";
    script.push_back(Engine::Logic::MakeCall("Missing", 20));
    EXPECT_FALSE(Engine::Logic::UnpackCall(library, script, 4, next));
}

// Renaming a library block renames the library blocks (and scripts) that run it
TEST(BlockLibrary, RenamingFollowsEverywhere)
{
    Engine::Logic::Library library = MoveLibrary();
    library.blocks.push_back(
        {.name = "Twice", .blocks = {Engine::Logic::MakeCall("Move", 1), Engine::Logic::MakeCall("Move", 2)}});
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Move", 2)});
    EXPECT_EQ(Engine::Logic::CountCalls(logic, "Move"), 1u);

    ASSERT_TRUE(Engine::Logic::RenameLibraryBlock(library, "Move", "Step"));
    Engine::Logic::RenameCalls(logic, "Move", "Step");
    EXPECT_NE(library.Find("Step"), nullptr);
    EXPECT_EQ(library.Find("Twice")->blocks[0].text, "Step");
    EXPECT_EQ(logic.scripts[0].blocks[1].text, "Step");
    EXPECT_EQ(Engine::Logic::CountCalls(logic, "Move"), 0u);
    EXPECT_FALSE(Engine::Logic::RenameLibraryBlock(library, "Step", "Twice")) << "taken";
    EXPECT_FALSE(Engine::Logic::RenameLibraryBlock(library, "Gone", "Other"));
}

// The library file round-trips; no file is an empty library; bad entries are left out with warnings
TEST_F(BlockFileTest, LibraryFilesRoundTrip)
{
    Engine::Logic::Library library = MoveLibrary();
    Block test = MakeBlock(Op::If, 1);
    test.slots[0] = KeyBlock(Op::KeyDown, 2, "W");
    test.body = {Engine::Logic::MakeCall("Move", 3)};
    library.blocks.push_back({.name = "Walk", .blocks = {test}});
    Engine::Logic::SaveLibraryFile(dir_ / "data" / "library.blocks.json", library);

    Engine::Logic::Library loaded;
    const auto report = Engine::Logic::LoadLibraryFile(dir_ / "data" / "library.blocks.json", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_EQ(loaded, library);

    Engine::Logic::Library none = MoveLibrary();
    EXPECT_TRUE(Engine::Logic::LoadLibraryFile(dir_ / "missing.json", none).ok);
    EXPECT_TRUE(none.blocks.empty());

    std::ofstream(dir_ / "bad.json") << R"({ "version": 1, "blocks": [ { "name": "Move", "blocks": [] },
        { "name": "Move" }, { "blocks": [] }, 5,
        { "name": "Odd", "blocks": [ { "id": 1, "block": "when_play_starts" }, { "id": 2, "block": "show" } ] } ] })";
    Engine::Logic::Library bad;
    const auto badReport = Engine::Logic::LoadLibraryFile(dir_ / "bad.json", bad);
    ASSERT_TRUE(badReport.ok);
    ASSERT_EQ(bad.blocks.size(), 2u) << "a repeated name, no name and not an object are left out";
    ASSERT_EQ(bad.blocks[1].blocks.size(), 1u) << "a hat cannot be in a library block";
    EXPECT_EQ(bad.blocks[1].blocks[0].op, Op::Show);
    EXPECT_GE(badReport.warnings.size(), 4u);
}

// A library block runs its blocks as if they were in the script; while they run, the library block in the script
// glows (their own ids are not the script's)
TEST_F(LogicRunnerTest, LibraryBlocksRunTheirBlocks)
{
    runner_.SetLibrary(MoveLibrary());
    Run({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Move", 2), Engine::Logic::MakeCall("Move", 3),
         Make(Op::Say, 4)});
    Update();
    // Forward 2 (to z -2), a quarter turn, forward 2 again (now along -X), another quarter turn
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance);
    EXPECT_NEAR(Transform().x, -2.0f, kTolerance);
    EXPECT_NEAR(Transform().rotation, 180.0f, kTolerance);
    EXPECT_EQ(runner_.LastSaid(1), "Hello!") << "and on to the blocks after it";
    const auto active = runner_.ActiveBlocks(1);
    EXPECT_TRUE(std::find(active.begin(), active.end(), 2u) != active.end());
    EXPECT_TRUE(std::find(active.begin(), active.end(), 3u) != active.end());
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// Library blocks can run library blocks; one that is missing, or that runs itself without end, is a problem
TEST_F(LogicRunnerTest, LibraryBlocksInsideLibraryBlocks)
{
    Engine::Logic::Library library = MoveLibrary();
    library.blocks.push_back(
        {.name = "Twice", .blocks = {Engine::Logic::MakeCall("Move", 1), Engine::Logic::MakeCall("Move", 2)}});
    library.blocks.push_back({.name = "Again", .blocks = {Engine::Logic::MakeCall("Again", 1)}});
    runner_.SetLibrary(library);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Twice", 2), Engine::Logic::MakeCall("Gone", 3),
         Engine::Logic::MakeCall("Again", 4), Make(Op::MoveBy, 5, {0, 9, 0})});
    Update();
    EXPECT_NEAR(Transform().rotation, 180.0f, kTolerance) << "Twice turned twice";
    const auto problems = runner_.Problems(1);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_EQ(problems[0].block, 3u);
    EXPECT_NE(problems[0].message.find("no block called \"Gone\""), std::string::npos) << problems[0].message;
    EXPECT_EQ(problems[1].block, 4u) << "reported on the library block in the script";
    EXPECT_NE(problems[1].message.find("infinite loop"), std::string::npos) << problems[1].message;
    EXPECT_TRUE(runner_.IsPaused()) << "stopped where it happened";
    EXPECT_EQ(runner_.Debug().reason, Engine::Logic::PauseReason::InfiniteLoop);
    EXPECT_NEAR(Transform().y, 0.0f, kTolerance) << "nothing after it ran";
}

// ---------------------------------------------------------------- Physics blocks

namespace
{
    // Records what the blocks ask of the physics: a stand-in for Game::BlockPhysics
    struct FakePhysics final : LogicRunner::PhysicsAccess
    {
        bool dynamic = true; // the object has a Dynamic Rigid Body
        bool ground = true;  // ...standing on something
        glm::vec3 walked{0.0f};
        glm::vec3 pushed{0.0f};
        float jumped = 0.0f;

        bool Walk(Engine::Scene::Entity, const glm::vec3& offset) override
        {
            walked += dynamic ? offset : glm::vec3(0.0f);
            return dynamic;
        }
        bool Push(Engine::Scene::Entity, const glm::vec3& velocityChange) override
        {
            pushed += dynamic ? velocityChange : glm::vec3(0.0f);
            return dynamic;
        }
        bool Jump(Engine::Scene::Entity, float height) override
        {
            jumped = dynamic && ground ? height : jumped;
            return dynamic;
        }
        std::optional<bool> OnGround(Engine::Scene::Entity) const override
        {
            return dynamic ? std::optional<bool>(ground) : std::nullopt;
        }
        bool Raycast(Engine::Scene::Entity, const glm::vec3&, float) override { return false; }
        std::string surface = "stone"; // what it stands on
        std::optional<std::string> SurfaceUnder(Engine::Scene::Entity) const override
        {
            return dynamic && ground ? std::optional<std::string>(surface) : std::nullopt;
        }
        std::optional<RayResult> ray; // what a ray reaches
        std::optional<RayResult> CastRay(Engine::Scene::Entity, Engine::Scene::Entity, float, float, float,
                                         float) const override
        {
            return ray;
        }
    };
}

// "shoot" takes Health from what it hits and runs that object's "when I am shot"; "my shot hit?" says whether it hit
// The Game blocks keep the player's settings (to be read back) and ask the game's window for the rest
TEST_F(LogicRunnerTest, GameBlocksKeepSettingsAndAskTheWindow)
{
    Block volume = Make(Op::SetVolume, 2, {40});
    volume.text = "Music";
    Block fullscreen = MakeBlock(Op::SetFullscreen, 4);
    fullscreen.text = "on";
    Block read = MakeBlock(Op::SetVariable, 6);
    read.text = "music";
    Block setting = MakeBlock(Op::SettingOf, 7);
    setting.text = "music volume";
    read.inputs = {setting};
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), volume, Make(Op::SetSensitivity, 3, {0.5f}),
                                 fullscreen, MakeBlock(Op::QuitGame, 5), read});
    logic.variables.push_back({.name = "music", .value = 0.0f});
    runner_.Set(1, "Menu", entity_.GetId(), logic);
    Update();
    EXPECT_DOUBLE_EQ(runner_.Settings().at("music volume"), 40.0);
    EXPECT_DOUBLE_EQ(runner_.Settings().at("sensitivity"), 0.5);
    EXPECT_EQ(runner_.VariableOf("Menu", "music"), std::optional<double>(40.0));
    const auto requests = runner_.TakeGameRequests();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[0].kind, LogicRunner::GameRequest::Kind::Fullscreen);
    EXPECT_FLOAT_EQ(requests[0].a, 1.0f);
    EXPECT_EQ(requests[1].kind, LogicRunner::GameRequest::Kind::Quit);
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// "set view angle" zooms the Main camera in (aiming down the sights) and back out
TEST_F(LogicRunnerTest, TheViewAngleIsTheMainCamerasFieldOfView)
{
    const auto camera = ecs_.CreateEntity();
    ecs_.AddComponent(camera, Engine::Scene::CameraComponent{.fieldOfView = 70.0f, .primary = true});
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetViewAngle, 2, {45})});
    Update();
    EXPECT_FLOAT_EQ(ecs_.TryGetComponent<Engine::Scene::CameraComponent>(camera)->fieldOfView, 45.0f);
}

// "set setting" keeps a number of the game's own, and "setting" reads it back (0 until it is set)
TEST_F(LogicRunnerTest, ASettingOfYourOwnIsKept)
{
    Block set = Make(Op::SetSetting, 2, {3});
    set.text = "Difficulty";
    Run({MakeBlock(Op::WhenPlayStarts, 1), set});
    Update();
    EXPECT_DOUBLE_EQ(runner_.Settings().at("difficulty"), 3.0);
}

// Tilting a player the camera follows moves its camera, which is where it aims; anything else leans
TEST_F(LogicRunnerTest, TiltingAFollowedPlayerMovesItsCamera)
{
    ecs_.AddComponent(entity_, Engine::Scene::PlayerControllerComponent{.followCamera = true});
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetTilt, 2, {12})});
    Update();
    EXPECT_FLOAT_EQ(ecs_.TryGetComponent<Engine::Scene::PlayerControllerComponent>(entity_)->cameraPitch, 12.0f);
    EXPECT_FLOAT_EQ(ecs_.GetTransform(entity_)->rotationX, 0.0f);
}

// "can see" asks the physics; "shoot at" hurts what is hit, and says so
// "key pressed?" knows the mouse buttons by name too
TEST_F(LogicRunnerTest, KeyDownKnowsMouseButtons)
{
    Block held = Make(Op::If, 2);
    Block right = MakeBlock(Op::KeyDown, 3);
    right.text = "MouseRight";
    held.slots = {right};
    Block mark = MakeBlock(Op::SetVariable, 4);
    mark.text = "aiming";
    mark.numbers = {1.0f};
    held.body = {mark};
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), held});
    logic.variables.push_back({.name = "aiming", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    auto keys = keys_.Keys();
    keys.mouseButton = [](int button) { return button == Engine::Input::Mouse_Right; };
    runner_.Update(ecs_, 1.0 / 60.0, keys);
    EXPECT_EQ(runner_.VariableOf("Object", "aiming"), std::optional<double>(1.0));
}

// A button hidden and shown again is still a button, with the text it was shown with
TEST_F(LogicRunnerTest, AButtonShownAgainStaysAButton)
{
    ecs_.AddComponent(entity_, Engine::Scene::ScreenElementComponent{.kind = Engine::Scene::ScreenElementKind::Button,
                                                                     .visible = false});
    Block show = MakeBlock(Op::ShowScreenText, 2);
    show.text = "BACK";
    Run({MakeBlock(Op::WhenPlayStarts, 1), show});
    Update();
    const auto* element = ecs_.TryGetComponent<Engine::Scene::ScreenElementComponent>(entity_);
    EXPECT_TRUE(element->visible);
    EXPECT_EQ(element->kind, Engine::Scene::ScreenElementKind::Button);
    EXPECT_EQ(element->text, "BACK");
}

// A ray says what it reached and how far off, "ray hit" asks about it, and "hit.variable" reaches into what it hit
TEST_F(LogicRunnerTest, RaysFindThingsAndReachIntoThem)
{
    const auto target = ecs_.CreateEntity();
    runner_.SetObjects({{.name = "Target", .entity = target.GetId()}, {.name = "Object", .entity = entity_.GetId()}});
    LogicDesc hurt;
    Block heard = MakeBlock(Op::SetVariable, 11);
    heard.text = "told";
    heard.numbers = {1.0f};
    Block hat = MakeBlock(Op::WhenIReceive, 10);
    hat.text = "shot";
    hurt.variables.push_back({.name = "health", .value = 100.0f});
    hurt.variables.push_back({.name = "told", .value = 0.0f});
    hurt.scripts.push_back(Script{.x = 0.0f, .y = 0.0f, .blocks = {hat, heard}});
    runner_.Set(2, "Target", target.GetId(), hurt);

    FakePhysics physics;
    physics.ray = LogicRunner::PhysicsAccess::RayResult{.entity = target.GetId(), .distance = 12.0f};
    runner_.SetPhysics(&physics);
    Block found = Make(Op::If, 3);
    Block hit = MakeBlock(Op::RayHit, 4);
    hit.text = "Target";
    found.slots = {hit};
    Block damage = Make(Op::ChangeValueOf, 5, {-25});
    damage.text = "hit.health";
    Block tell = MakeBlock(Op::TellRayHit, 6);
    tell.text = "shot";
    Block far = MakeBlock(Op::SetVariable, 7);
    far.text = "far";
    far.inputs[0] = MakeBlock(Op::RayDistance, 8);
    found.body = {damage, tell, far};
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::RayAhead, 2), found});
    logic.variables.push_back({.name = "far", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    Update();
    EXPECT_EQ(runner_.VariableOf("Target", "health"), std::optional<double>(75.0));
    EXPECT_EQ(runner_.VariableOf("Object", "far"), std::optional<double>(12.0));
    Update();
    EXPECT_EQ(runner_.VariableOf("Target", "told"), std::optional<double>(1.0)) << "only what it hit is told";
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// Hitting something with no such variable (a wall has no health) is no mistake, and "exists?" can ask first
TEST_F(LogicRunnerTest, ARayThatHitsAWallHurtsNothing)
{
    const auto wall = ecs_.CreateEntity();
    FakePhysics physics;
    physics.ray = LogicRunner::PhysicsAccess::RayResult{.entity = wall.GetId(), .distance = 3.0f};
    runner_.SetPhysics(&physics);
    Block asked = Make(Op::If, 3);
    Block exists = MakeBlock(Op::ValueExists, 4);
    exists.text = "hit.health";
    asked.slots = {exists};
    Block mark = MakeBlock(Op::SetVariable, 5);
    mark.text = "could";
    mark.numbers = {1.0f};
    asked.body = {mark};
    Block damage = Make(Op::ChangeValueOf, 6, {-25});
    damage.text = "hit.health";
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::RayAhead, 2), asked, damage});
    logic.variables.push_back({.name = "could", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    Update();
    EXPECT_EQ(runner_.VariableOf("Object", "could"), std::optional<double>(0.0));
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// Any object's variable can be read and written as Object.variable; one that is not there is reported
TEST_F(LogicRunnerTest, ValuesOfOtherObjects)
{
    const auto other = ecs_.CreateEntity();
    LogicDesc theirs;
    theirs.variables.push_back({.name = "score", .value = 5.0f});
    runner_.Set(2, "Keeper", other.GetId(), theirs);
    Block copy = MakeBlock(Op::SetVariable, 2);
    copy.text = "mine";
    copy.inputs[0] = MakeBlock(Op::ValueOf, 3);
    copy.inputs[0].text = "Keeper.score";
    Block set = Make(Op::SetValueOf, 4, {9});
    set.text = "Keeper.score";
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), copy, set});
    logic.variables.push_back({.name = "mine", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    Update();
    EXPECT_EQ(runner_.VariableOf("Object", "mine"), std::optional<double>(5.0));
    EXPECT_EQ(runner_.VariableOf("Keeper", "score"), std::optional<double>(9.0));

    Block missing = Make(Op::SetValueOf, 2, {1});
    missing.text = "Keeper.lives";
    runner_.Set(1, "Object", entity_.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), missing}));
    Update();
    EXPECT_FALSE(runner_.Problems(1).empty());
}

// A My Block takes numbers: its blocks read them as variables, each time with what that use gave it
TEST_F(LogicRunnerTest, MyBlocksTakeInputs)
{
    Library library;
    Block read = Make(Op::ChangeVariable, 1);
    read.text = "total";
    read.inputs[0] = MakeBlock(Op::VariableValue, 2);
    read.inputs[0].text = "amount";
    library.blocks.push_back({.name = "Add Up", .blocks = {read}, .inputs = {"amount"}});
    runner_.SetLibrary(library);
    Block five = MakeCall("Add Up", 2);
    five.numbers[0] = 5.0f;
    Block two = MakeCall("Add Up", 3);
    two.numbers[0] = 2.0f;
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), five, two});
    logic.variables.push_back({.name = "total", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    Update();
    EXPECT_EQ(runner_.VariableOf("Object", "total"), std::optional<double>(7.0));
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// The palette's list of My Blocks: in name order whatever order they were saved in, narrowed by what is typed
TEST(MyBlockList, IsInNameOrderAndFoundByPartOfTheName)
{
    Library library;
    for (const char* name : {"Shoot", "bandit Hurt", "Enemy Hurt", "Apply Settings"})
    {
        library.blocks.push_back({.name = name});
    }
    const auto names = [&library](std::string_view search)
    {
        std::vector<std::string> found;
        for (const auto* saved : Engine::Logic::LibraryBlocksMatching(library, search))
        {
            found.push_back(saved->name);
        }
        return found;
    };
    EXPECT_EQ(names(""), (std::vector<std::string>{"Apply Settings", "bandit Hurt", "Enemy Hurt", "Shoot"}));
    EXPECT_EQ(names("HURT"), (std::vector<std::string>{"bandit Hurt", "Enemy Hurt"}));
    EXPECT_TRUE(names("nothing like it").empty());
}

TEST(MyBlockInputs, AreReadFromTheirText)
{
    EXPECT_EQ(ParseInputs(" damage , range,, "), (std::vector<std::string>{"damage", "range"}));
    EXPECT_EQ(InputsText({"damage", "range"}), "damage, range");
    EXPECT_EQ(ParseInputs("a,b,c,d,e").size(), 4u) << "as many as a block has numbers";
}

// Show, hide and colour are the same blocks for what an object shows on the screen
TEST_F(LogicRunnerTest, ShowHideAndColourCoverScreenElements)
{
    ecs_.AddComponent(entity_, Engine::Scene::ScreenElementComponent{.text = "Hi", .visible = true});
    Run({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::Hide, 2), Make(Op::SetColor, 3, {1, 0, 0})});
    Update();
    const auto* element = ecs_.TryGetComponent<Engine::Scene::ScreenElementComponent>(entity_);
    EXPECT_FALSE(element->visible);
    EXPECT_FLOAT_EQ(element->colorG, 0.0f);
    Run({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::Show, 2)});
    Update();
    EXPECT_TRUE(ecs_.TryGetComponent<Engine::Scene::ScreenElementComponent>(entity_)->visible);
}

// "when key pressed" starts on a mouse button too
TEST_F(LogicRunnerTest, WhenKeyPressedKnowsMouseButtons)
{
    Block hat = MakeBlock(Op::WhenKeyPressed, 1);
    hat.text = "MouseRight";
    Block mark = MakeBlock(Op::SetVariable, 2);
    mark.text = "clicked";
    mark.numbers = {1.0f};
    LogicDesc logic = OneScript({hat, mark});
    logic.variables.push_back({.name = "clicked", .value = 0.0f});
    runner_.Set(1, "Object", entity_.GetId(), logic);
    auto keys = keys_.Keys();
    keys.mousePressed = [](int button) { return button == Engine::Input::Mouse_Right; };
    runner_.Update(ecs_, 1.0 / 60.0, keys);
    EXPECT_EQ(runner_.VariableOf("Object", "clicked"), std::optional<double>(1.0));
}

// "play footstep" plays the steps of what the object stands on, and nothing while it is in the air
TEST_F(LogicRunnerTest, FootstepsSoundOfTheGroundUnderfoot)
{
    FakePhysics physics;
    runner_.SetPhysics(&physics);
    const auto step = [this]
    {
        Run({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::PlayFootstep, 2)});
        Update();
        return runner_.TakeSoundRequests();
    };

    auto sounds = step();
    ASSERT_EQ(sounds.size(), 1u);
    EXPECT_EQ(sounds[0].name, "step_stone");
    physics.surface = "";
    sounds = step();
    ASSERT_EQ(sounds.size(), 1u);
    EXPECT_EQ(sounds[0].name, "step_default") << "ground with no Surface";
    physics.ground = false;
    EXPECT_TRUE(step().empty()) << "in the air";
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// On a Dynamic body, Motion blocks walk it through the physics (so walls stop it) instead of moving its Transform
TEST_F(LogicRunnerTest, MotionBlocksWalkDynamicBodies)
{
    FakePhysics physics;
    runner_.SetPhysics(&physics);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2, {2}), Make(Op::MoveBy, 3, {0, 1, 0})});
    Update();
    EXPECT_NEAR(physics.walked.z, -2.0f, kTolerance);
    EXPECT_NEAR(physics.walked.y, 1.0f, kTolerance);
    EXPECT_NEAR(Transform().z, 0.0f, kTolerance) << "the physics moves it, not the block";

    physics.dynamic = false; // without a Dynamic body the Transform moves, as always
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2, {2})});
    Update();
    EXPECT_NEAR(Transform().z, -2.0f, kTolerance);
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// Push forward kicks it the way it faces, push by along the axes, and jump only when it stands on something
TEST_F(LogicRunnerTest, PhysicsBlocksPushAndJump)
{
    FakePhysics physics;
    runner_.SetPhysics(&physics);
    Block ifGround = Make(Op::If, 4, {}, {Make(Op::Jump, 5, {2})});
    ifGround.slots[0] = MakeBlock(Op::OnGround, 6);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::PushForward, 2, {3}), Make(Op::PushBy, 3, {0, 4, 0}), ifGround});
    Update();
    EXPECT_NEAR(physics.pushed.z, -3.0f, kTolerance);
    EXPECT_NEAR(physics.pushed.y, 4.0f, kTolerance);
    EXPECT_FLOAT_EQ(physics.jumped, 2.0f);

    physics.jumped = 0.0f;
    physics.ground = false;
    Run({MakeBlock(Op::WhenPlayStarts, 1), ifGround});
    Update();
    EXPECT_FLOAT_EQ(physics.jumped, 0.0f) << "in the air: no jump";
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// Without a Rigid Body (or without the game's physics) the Physics blocks say what is missing
TEST_F(LogicRunnerTest, PhysicsBlocksNeedARigidBody)
{
    Block touching = MakeBlock(Op::Touching, 4);
    touching.text = "Cube";
    Block test = Make(Op::If, 3, {}, {Make(Op::PushBy, 5, {0, 1, 0})});
    test.slots[0] = touching;
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Jump, 2, {1}), test});
    Update();
    const auto problems = runner_.Problems(1);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].message.find("Dynamic Rigid Body"), std::string::npos) << problems[0].message;
    EXPECT_NE(problems[1].message.find("no Rigid Body"), std::string::npos) << problems[1].message;
}

// "touching" asks the body's contacts, by the other object's name
TEST_F(LogicRunnerTest, TouchingAsksWhatTheBodyTouches)
{
    const Engine::Scene::Entity ball = ecs_.CreateEntity();
    const Engine::Scene::Entity wall = ecs_.CreateEntity();
    runner_.SetObjects({{.name = "Cube", .entity = entity_.GetId()},
                        {.name = "Ball", .entity = ball.GetId()},
                        {.name = "Wall", .entity = wall.GetId()}});
    ecs_.AddComponent(entity_, Engine::Scene::RigidBodyComponent{});
    ecs_.AddComponent(entity_, Engine::Scene::PhysicsBodyComponent{.touching = {ball}});
    Block touchingBall = MakeBlock(Op::Touching, 3);
    touchingBall.text = "Ball";
    Block touchingWall = MakeBlock(Op::Touching, 5);
    touchingWall.text = "Wall";
    Block ifBall = Make(Op::If, 2, {}, {Make(Op::SetVariable, 4, {1})});
    ifBall.slots[0] = touchingBall;
    ifBall.body[0].text = "ball";
    Block ifWall = Make(Op::If, 6, {}, {Make(Op::SetVariable, 7, {1})});
    ifWall.slots[0] = touchingWall;
    ifWall.body[0].text = "wall";
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), ifBall, ifWall});
    logic.variables = {{.name = "ball", .value = 0.0f}, {.name = "wall", .value = 0.0f}};
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    const auto variables = runner_.Variables(1);
    ASSERT_EQ(variables.size(), 2u);
    EXPECT_DOUBLE_EQ(variables[0].second, 1.0) << "it touches the ball";
    EXPECT_DOUBLE_EQ(variables[1].second, 0.0) << "but not the wall";
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// ---------------------------------------------------------------- What the game's screen shows

// "show variable" puts a variable (and its value as it changes) on the game's screen, "hide variable" takes it off
// What was said is a speech bubble for a few seconds, newest first
TEST_F(LogicRunnerTest, SpeechLastsAFewSeconds)
{
    Block hello = MakeBlock(Op::Say, 2);
    hello.text = "Hello!";
    Run({MakeBlock(Op::WhenPlayStarts, 1), hello});
    Update();
    auto speeches = runner_.Speeches(4.0);
    ASSERT_EQ(speeches.size(), 1u);
    EXPECT_EQ(speeches[0].text, "Hello!");
    EXPECT_EQ(speeches[0].objectName, "Cube");
    EXPECT_EQ(speeches[0].entity, entity_.GetId());
    Update(5.0);
    EXPECT_TRUE(runner_.Speeches(4.0).empty()) << "5 seconds later it has gone";
    EXPECT_EQ(runner_.LastSaid(1), "Hello!") << "the Logic window still has it";
}

// The Screen blocks act on the object's Screen Element: "show text / picture / bar on screen" give it one if it has
// none and show it, the others place, size and colour it, and "hide from screen" hides it
TEST_F(LogicRunnerTest, AnimationBlocksChangeTheObjectsAnimator)
{
    using Engine::Scene::AnimatorComponent;
    Block walk = MakeBlock(Op::PlayAnimation, 2);
    walk.text = "Walk";
    Run({MakeBlock(Op::WhenPlayStarts, 1), walk, Make(Op::SetAnimationSpeed, 3, {2})});
    Update();
    const auto* animator = ecs_.TryGetComponent<AnimatorComponent>(entity_);
    ASSERT_NE(animator, nullptr) << "made for it";
    EXPECT_EQ(animator->clip, "Walk");
    EXPECT_TRUE(animator->playing);
    EXPECT_TRUE(animator->loop);
    EXPECT_FLOAT_EQ(animator->speed, 2.0f);
    EXPECT_FLOAT_EQ(animator->time, 0.0f) << "a new clip starts at its beginning";

    Block jump = MakeBlock(Op::PlayAnimationOnce, 2);
    jump.text = "Jump";
    Run({MakeBlock(Op::WhenPlayStarts, 1), jump, MakeBlock(Op::StopAnimation, 3)});
    Update();
    animator = ecs_.TryGetComponent<AnimatorComponent>(entity_);
    ASSERT_NE(animator, nullptr);
    EXPECT_EQ(animator->clip, "Jump");
    EXPECT_FALSE(animator->loop) << "once";
    EXPECT_FALSE(animator->playing) << "stopped after it was started";
}

// The Sky blocks change the scene's sky: an object's with a Sky (the first), or a sky of their own when there is none
// "tilt of" reads where a player's camera looks (or another object's tilt), "set tilt to" tips the object, and "fly
// forward" follows that tilt while "move forward" stays level
TEST_F(LogicRunnerTest, TiltAimsWhatFliesForward)
{
    const Engine::Scene::Entity player = ecs_.CreateEntity();
    ecs_.SetTransform(player, TransformComponent{.rotationX = 10.0f});
    ecs_.AddComponent(player, Engine::Scene::PlayerControllerComponent{.cameraPitch = 30.0f});
    runner_.SetObjects({{.name = "Cube", .entity = entity_.GetId()}, {.name = "Player", .entity = player.GetId()}});
    Block tilt = Make(Op::SetTilt, 2, {0});
    Block of = MakeBlock(Op::TiltOf, 3);
    of.text = "Player";
    tilt.inputs[0] = of;
    Run({MakeBlock(Op::WhenPlayStarts, 1), tilt, Make(Op::FlyForward, 4, {2}), Make(Op::MoveForward, 5, {1})});
    Update();
    EXPECT_NEAR(Transform().rotationX, 30.0f, kTolerance) << "a player aims where its camera looks";
    EXPECT_NEAR(Transform().y, 1.0f, kTolerance) << "2 along a 30 degree slope rises 1";
    EXPECT_NEAR(Transform().z, -std::sqrt(3.0f) - 1.0f, kTolerance) << "then 1 more, level";

    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetTilt, 2, {-120})});
    Update();
    EXPECT_NEAR(Transform().rotationX, -90.0f, kTolerance) << "straight down at most";
}

// A click on the object's Screen Element starts its "when clicked on screen" scripts
TEST_F(LogicRunnerTest, ClickingTheObjectOnTheScreenRunsItsScript)
{
    Block clicked = MakeBlock(Op::WhenScreenClicked, 3);
    Block go = MakeBlock(Op::GoToScene, 4);
    go.text = "level1";
    LogicDesc logic;
    logic.scripts.push_back(Script{.x = 0.0f, .y = 0.0f, .blocks = {clicked, go}});
    runner_.Set(1, "Play Button", entity_.GetId(), logic);
    EXPECT_TRUE(runner_.HasScreenClickScript(1));
    Update();
    EXPECT_FALSE(runner_.TakeSceneRequest()) << "not clicked yet";

    runner_.ScreenClicked(1);
    Update();
    EXPECT_EQ(runner_.TakeSceneRequest(), std::optional<std::string>("level1"));
    EXPECT_FALSE(runner_.TakeSceneRequest()) << "taken once";
}

// "play sound" asks the game for the sound, from where the object is; each request is taken once
TEST_F(LogicRunnerTest, PlaySoundAsksForTheSoundWhereTheObjectIs)
{
    auto at = *ecs_.GetTransform(entity_);
    at.x = 3.0f;
    at.z = -2.0f;
    ecs_.SetTransform(entity_, at);
    Block swing = MakeBlock(Op::PlaySound, 2);
    swing.text = "sword_swing";
    Run({MakeBlock(Op::WhenPlayStarts, 1), swing});
    Update();
    const auto sounds = runner_.TakeSoundRequests();
    ASSERT_EQ(sounds.size(), 1u);
    EXPECT_EQ(sounds[0].name, "sword_swing");
    ASSERT_TRUE(sounds[0].position);
    EXPECT_FLOAT_EQ(sounds[0].position->x, 3.0f);
    EXPECT_FLOAT_EQ(sounds[0].position->z, -2.0f);
    EXPECT_TRUE(runner_.TakeSoundRequests().empty()) << "taken once";
    EXPECT_TRUE(runner_.Problems(1).empty());
}

// The HUD's own parts, and the mouse, are the blocks' to change too
// "point in direction" turns the object, "direction of" reads another's, and "mouse down?" asks the left button

// Several blocks at once (the canvas's selection): a script with a loop, and one more
namespace
{
    Engine::Logic::LogicDesc TwoScripts()
    {
        Engine::Logic::LogicDesc logic;
        logic.scripts.push_back({.blocks = {Make(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2, {1.0f}),
                                            Make(Op::Repeat, 3, {2.0f}, {Make(Op::MoveForward, 4, {1.0f})}),
                                            Make(Op::Show, 5)}});
        logic.scripts.push_back({.blocks = {Make(Op::Hide, 6)}});
        return logic;
    }
}

TEST(BlockBatch, RemovingSeveralLeavesTheRestAndDropsEmptyScripts)
{
    Engine::Logic::LogicDesc logic = TwoScripts();
    EXPECT_EQ(Engine::Logic::RemoveBlocks(logic, {2, 3, 6}), 3u);

    ASSERT_EQ(logic.scripts.size(), 1u) << "the script left with nothing goes";
    ASSERT_EQ(logic.scripts[0].blocks.size(), 2u);
    EXPECT_EQ(logic.scripts[0].blocks[0].id, 1u);
    EXPECT_EQ(logic.scripts[0].blocks[1].id, 5u) << "what was under a removed block stays";
    EXPECT_EQ(Engine::Logic::FindBlock(logic, 4), nullptr) << "what was inside a removed block goes with it";
}

TEST(BlockBatch, ABlockInsideAnotherCanBeRemovedOnItsOwn)
{
    Engine::Logic::LogicDesc logic = TwoScripts();
    EXPECT_EQ(Engine::Logic::RemoveBlocks(logic, {4}), 1u);
    ASSERT_NE(Engine::Logic::FindBlock(logic, 3), nullptr);
    EXPECT_TRUE(Engine::Logic::FindBlock(logic, 3)->body.empty());
}

TEST(BlockBatch, CopiesKeepNeighboursTogether)
{
    const auto runs = Engine::Logic::CopyBlocks(TwoScripts(), {2, 3, 4, 6});
    ASSERT_EQ(runs.size(), 2u);
    ASSERT_EQ(runs[0].size(), 2u) << "2 and 3 are neighbours, and 4 comes inside 3";
    EXPECT_EQ(runs[0][0].id, 2u);
    EXPECT_EQ(runs[0][1].id, 3u);
    ASSERT_EQ(runs[1].size(), 1u);
    EXPECT_EQ(runs[1][0].id, 6u);
}

TEST(BlockBatch, ABlockApartFromItsNeighboursIsARunOfItsOwn)
{
    const auto runs = Engine::Logic::CopyBlocks(TwoScripts(), {1, 5});
    ASSERT_EQ(runs.size(), 2u);
    EXPECT_EQ(runs[0].front().id, 1u);
    EXPECT_EQ(runs[1].front().id, 5u);
}
