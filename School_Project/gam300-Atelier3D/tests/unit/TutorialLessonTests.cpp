// The lessons shipped with the editor (data/tutorials.editor.modes of the sample game, copied to the templates):
// the Home page shows them when a game has no lesson notes of its own, or before a game is open

#include "editor/Tour.hpp"
#include "engine/editor/LessonGoals.hpp"
#include "engine/editor/TutorialNotes.hpp"
#include "engine/logic/Blocks.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using Engine::Editor::TutorialMode;
using Engine::Editor::TutorialNotes;

namespace
{
    const std::filesystem::path kTemplatesDir = ATELIER_SAMPLE_GAME_DIR;
    const std::filesystem::path kLessonsFile = "data/tutorials.editor.modes";
}

// The bug this guards: a game's saved file of the empty modes hid the shipped lessons. Only real notes replace them.
TEST(TutorialLessons, EmptySavedNotesAreNotAGamesOwn)
{
    std::vector<TutorialMode> modes = TutorialNotes::Defaults();
    EXPECT_TRUE(TutorialNotes::IsUntouched(modes));

    // As it comes back from the file every game opened so far has
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "atelier_untouched.editor.modes";
    std::string error;
    ASSERT_TRUE(TutorialNotes::Save(modes, file, error)) << error;
    std::vector<TutorialMode> saved;
    ASSERT_TRUE(TutorialNotes::Load(saved, file, error)) << error;
    std::filesystem::remove(file);
    EXPECT_TRUE(TutorialNotes::IsUntouched(saved));

    std::vector<TutorialMode> renamed = modes;
    renamed[0].name = "My lessons";
    EXPECT_FALSE(TutorialNotes::IsUntouched(renamed));

    std::vector<TutorialMode> written = modes;
    written[1].tutorials.push_back({"Lesson 1", "My notes"});
    EXPECT_FALSE(TutorialNotes::IsUntouched(written));

    std::vector<TutorialMode> track;
    ASSERT_TRUE(TutorialNotes::Load(track, kTemplatesDir / kLessonsFile, error)) << error;
    EXPECT_FALSE(TutorialNotes::IsUntouched(track));
}

// The tracks go from the world to the logic, and every step can be checked
TEST(TutorialLessons, TracksGoInOrder)
{
    std::vector<TutorialMode> modes;
    std::string error;
    ASSERT_TRUE(TutorialNotes::Load(modes, kTemplatesDir / kLessonsFile, error)) << error;

    const std::vector<std::string> order{"Environment", "Physics", "Parts", "Movement", "Logic", "Your game"};
    ASSERT_EQ(modes.size(), order.size());
    for (std::size_t i = 0; i < modes.size(); ++i)
    {
        const TutorialMode& track = modes[i];
        EXPECT_EQ(track.name, "Track " + std::to_string(i + 1) + ": " + order[i])
            << "shipped lessons are told apart from a game's notes by \"Track \"";
        EXPECT_EQ(track.done, 0);
        EXPECT_EQ(track.nextIndex, static_cast<int>(track.tutorials.size()) + 1);
        EXPECT_LE(track.tutorials.size(), 6u) << "more than fit on the path without scrolling";
        for (const auto& lesson : track.tutorials)
        {
            EXPECT_FALSE(lesson.code.empty()) << lesson.name;
            EXPECT_FALSE(lesson.steps.empty()) << lesson.name;
            for (const auto& step : lesson.steps)
            {
                EXPECT_FALSE(step.text.empty()) << lesson.name;
                EXPECT_TRUE(Engine::Editor::IsValidGoal(step.check)) << lesson.name << ": " << step.check;
            }
        }
    }
}

// The bug this guards: "Press Run" was ticked by pressing Run. Saying what to press is not checking what happened.
TEST(TutorialLessons, EveryRunStepChecksWhatHappened)
{
    std::vector<TutorialMode> modes;
    std::string error;
    ASSERT_TRUE(TutorialNotes::Load(modes, kTemplatesDir / kLessonsFile, error)) << error;
    const std::vector<std::string> results{"ran ", "finished ", "near ", "travelled ", "dropped ", "landed ", "reaches "};
    for (const TutorialMode& track : modes)
    {
        for (const auto& lesson : track.tutorials)
        {
            for (const auto& step : lesson.steps)
            {
                if (step.text.starts_with("Press Run"))
                {
                    EXPECT_TRUE(std::any_of(results.begin(), results.end(), [&](const std::string& each)
                                            { return step.check.starts_with(each); }))
                        << lesson.name << ": " << step.check;
                }
            }
        }
    }
}

// Lessons open in order, across the tracks too. What is done stays open, to go over it.
TEST(TutorialLessons, LockedLessonsStayLocked)
{
    const std::vector<Engine::Editor::Tutorial> three(3);
    std::vector<TutorialMode> modes{{.name = "Track 1", .tutorials = three, .done = 1},
                                    {.name = "Track 2", .tutorials = three, .done = 0}};
    const auto open = [&](int mode, int lesson) { return TutorialNotes::IsUnlocked(modes, mode, lesson); };

    EXPECT_TRUE(open(0, 0)) << "done: can be gone over";
    EXPECT_TRUE(open(0, 1)) << "the next one";
    EXPECT_FALSE(open(0, 2)) << "not there yet";
    EXPECT_FALSE(open(1, 0)) << "the track before is not finished";

    modes[0].done = 3;
    EXPECT_TRUE(open(0, 2));
    EXPECT_TRUE(open(1, 0)) << "finishing a track opens the next";
    EXPECT_FALSE(open(1, 1));

    EXPECT_FALSE(open(2, 0)) << "no such track";
    EXPECT_FALSE(open(0, -1));
}

// Steps survive being saved, which is how the learner's progress is kept
TEST(TutorialLessons, StepsAreSaved)
{
    std::vector<TutorialMode> modes{{.name = "Track", .tutorials = {{.name = "One", .code = "c", .steps = {{"t", "object A"}}}}}};
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "atelier_steps.editor.modes";
    std::string error;
    ASSERT_TRUE(TutorialNotes::Save(modes, file, error)) << error;
    std::vector<TutorialMode> loaded;
    ASSERT_TRUE(TutorialNotes::Load(loaded, file, error)) << error;
    std::filesystem::remove(file);
    EXPECT_EQ(loaded, modes);
}

namespace
{
    using Engine::Editor::CheckGoal;
    using Engine::Editor::GoalWorld;
    using Engine::Logic::Block;
    using Engine::Logic::MakeBlock;
    using Engine::Logic::Op;

    Block Holding(Op op, std::vector<Block> body)
    {
        Block block = MakeBlock(op, 5);
        block.body = std::move(body);
        return block;
    }

    Engine::Scene::SceneDescription SceneOf(std::vector<Block> script)
    {
        Engine::Scene::SceneDescription scene;
        Engine::Scene::GameObject player{.id = 1, .name = "Player"};
        player.logic.emplace().scripts.push_back({.blocks = std::move(script)});
        player.logic->variables.push_back({.name = "score"});
        scene.objects = {player, {.id = 2, .name = "Flag"}};
        return scene;
    }
}

TEST(LessonGoals, ObjectsAndVariablesByName)
{
    const auto scene = SceneOf({});
    EXPECT_TRUE(CheckGoal(scene, "object Flag", {}));
    EXPECT_TRUE(CheckGoal(scene, "object flag", {})) << "names ignore case";
    EXPECT_FALSE(CheckGoal(scene, "object Ground", {}));
    EXPECT_TRUE(CheckGoal(scene, "variable Player score", {}));
    EXPECT_FALSE(CheckGoal(scene, "variable Player lives", {}));
    EXPECT_FALSE(CheckGoal(scene, "variable Flag score", {}));
}

// Blocks count in order, and only under a "when" block: a loose stack never runs
TEST(LessonGoals, ScriptsAreInOrderAndMustRun)
{
    const auto scene = SceneOf({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::Say, 2), MakeBlock(Op::MoveForward, 3)});
    EXPECT_TRUE(CheckGoal(scene, "script Player when_play_starts say move_forward", {}));
    EXPECT_FALSE(CheckGoal(scene, "script Player when_play_starts move_forward say", {})) << "the order is the logic";
    EXPECT_TRUE(CheckGoal(scene, "uses Player say", {}));

    const auto loose = SceneOf({MakeBlock(Op::Say, 2)});
    EXPECT_FALSE(CheckGoal(loose, "uses Player say", {}));
    EXPECT_FALSE(CheckGoal(loose, "gone Player say", {})) << "still there, even if it does not run";
}

TEST(LessonGoals, InsideAndSlots)
{
    Block check = MakeBlock(Op::IfElse, 6);
    check.slots = {MakeBlock(Op::Less, 7)};
    check.slots[0].inputs[0] = MakeBlock(Op::DistanceTo, 8);
    check.elseBody = {MakeBlock(Op::MoveForward, 9)};
    const auto scene = SceneOf({MakeBlock(Op::WhenPlayStarts, 1), Holding(Op::Forever, {check})});

    EXPECT_TRUE(CheckGoal(scene, "inside Player forever if", {})) << "\"if\" also means if / else";
    EXPECT_TRUE(CheckGoal(scene, "inside Player forever move_forward", {})) << "however deep";
    EXPECT_TRUE(CheckGoal(scene, "inside Player if move_forward", {})) << "either arm";
    EXPECT_FALSE(CheckGoal(scene, "inside Player repeat move_forward", {}));
    EXPECT_TRUE(CheckGoal(scene, "slot Player if less", {}));
    EXPECT_TRUE(CheckGoal(scene, "slot Player if distance_to", {})) << "a value in the condition";
    EXPECT_FALSE(CheckGoal(scene, "slot Player if key_down", {}));
}

// The scene: what things are, and what was done to them after they were made
TEST(LessonGoals, WhatIsInTheScene)
{
    Engine::Scene::SceneDescription scene;
    Engine::Scene::GameObject ball{.id = 1, .name = "Ball"};
    ball.transform.emplace();
    ball.renderable.emplace().shape = Engine::Game::Shape::Sphere;
    Engine::Scene::GameObject ground{.id = 2, .name = "Ground"};
    ground.transform.emplace();
    scene.objects = {ball, ground};

    EXPECT_TRUE(CheckGoal(scene, "shape Ball sphere", {}));
    EXPECT_FALSE(CheckGoal(scene, "shape Ball cube", {}));
    EXPECT_FALSE(CheckGoal(scene, "has Ball body", {}));
    EXPECT_FALSE(CheckGoal(scene, "moved Ball", {})) << "as it was made";
    EXPECT_FALSE(CheckGoal(scene, "sized Ball", {}));
    EXPECT_FALSE(CheckGoal(scene, "colored Ball", {}));
    EXPECT_FALSE(CheckGoal(scene, "above Ball Ground 3", {}));

    Engine::Scene::GameObject& changed = scene.objects[0];
    changed.transform->y = 4.0f;
    changed.transform->scaleX = 2.0f;
    changed.renderable->albedoG = 0.5f;
    changed.rigidBody.emplace();
    EXPECT_TRUE(CheckGoal(scene, "moved Ball", {}));
    EXPECT_TRUE(CheckGoal(scene, "sized Ball", {}));
    EXPECT_TRUE(CheckGoal(scene, "colored Ball", {}));
    EXPECT_TRUE(CheckGoal(scene, "above Ball Ground 3", {}));
    EXPECT_FALSE(CheckGoal(scene, "apart Ball Ground 1", {})) << "straight above: not apart along the ground";
    EXPECT_TRUE(CheckGoal(scene, "has Ball body", {}));
    EXPECT_TRUE(CheckGoal(scene, "body Ball dynamic", {}));
    EXPECT_FALSE(CheckGoal(scene, "body Ball static", {}));
    EXPECT_FALSE(CheckGoal(scene, "bouncy Ball", {}));
    changed.rigidBody->bounciness = 0.6f;
    EXPECT_TRUE(CheckGoal(scene, "bouncy Ball", {}));
}

// What a block is set to, not only that it is there
TEST(LessonGoals, WhatBlocksSay)
{
    Block turn = MakeBlock(Op::TurnBy, 2);
    turn.numbers[0] = 90.0f;
    Block point = MakeBlock(Op::PointTowards, 3);
    point.text = "Flag";
    const auto scene = SceneOf({MakeBlock(Op::WhenPlayStarts, 1), turn, point});
    EXPECT_TRUE(CheckGoal(scene, "number Player turn_by 0 90", {}));
    EXPECT_FALSE(CheckGoal(scene, "number Player turn_by 0 45", {}));
    EXPECT_TRUE(CheckGoal(scene, "text Player point_towards flag", {}));
    EXPECT_FALSE(CheckGoal(scene, "text Player point_towards Enemy", {}));
    EXPECT_FALSE(CheckGoal(SceneOf({turn}), "number Player turn_by 0 90", {})) << "under no \"when\" block it never runs";

    // A value block lives in a slot, not in the stack: a lesson can still ask about it
    Block check = MakeBlock(Op::If, 4);
    check.slots = {MakeBlock(Op::Less, 5)};
    check.slots[0].inputs[0] = MakeBlock(Op::DistanceTo, 6);
    check.slots[0].inputs[0].text = "Enemy";
    const auto nested = SceneOf({MakeBlock(Op::WhenPlayStarts, 1), Holding(Op::Forever, {check})});
    EXPECT_TRUE(CheckGoal(nested, "text Player distance_to Enemy", {}));
    EXPECT_FALSE(CheckGoal(nested, "text Player distance_to Flag", {}));
}

// Nothing about the running game holds until it says so: pressing Run is not enough
TEST(LessonGoals, TheRunningGame)
{
    auto scene = SceneOf({});
    scene.objects[0].transform.emplace().y = 5.0f;
    const std::vector<std::string> checks{"ran Player say",     "finished Player",      "near Player Flag 2",
                                          "travelled Player 1", "dropped Player 1",     "landed Player Flag",
                                          "reaches Player score 1"};
    for (const std::string& check : checks)
    {
        EXPECT_FALSE(CheckGoal(scene, check, {})) << check << ": the game is not running";
    }

    // Running, with nothing having happened yet: it is where it started
    GoalWorld world;
    world.seconds = 0.1;
    std::array<float, 3> player{0.0f, 5.0f, 0.0f};
    world.place = [&](std::string_view name) -> std::optional<std::array<float, 3>>
    {
        if (name == "Player")
        {
            return player;
        }
        return std::array{8.0f, 0.0f, 0.0f};
    };
    bool ended = false;
    bool said = false;
    double score = 0.0;
    world.finished = [&](std::string_view) { return ended; };
    world.ran = [&](std::string_view, Op) { return said; };
    world.variable = [&](std::string_view, std::string_view) { return std::optional(score); };
    for (const std::string& check : {"ran Player say", "finished Player", "near Player Flag 2", "travelled Player 1",
                                     "dropped Player 1", "landed Player Flag", "reaches Player score 1"})
    {
        EXPECT_FALSE(CheckGoal(scene, check, world)) << check << ": just pressed Run";
    }

    said = true;
    EXPECT_TRUE(CheckGoal(scene, "ran Player say", world));

    ended = true;
    EXPECT_FALSE(CheckGoal(scene, "finished Player", world)) << "over, but too soon to have been seen";
    world.seconds = 3.0;
    EXPECT_TRUE(CheckGoal(scene, "finished Player", world));

    EXPECT_TRUE(CheckGoal(scene, "landed Player Flag", world)) << "still above after falling for a while";
    player = {7.0f, 3.0f, 0.0f};
    EXPECT_TRUE(CheckGoal(scene, "travelled Player 5", world));
    EXPECT_TRUE(CheckGoal(scene, "dropped Player 1", world));
    EXPECT_TRUE(CheckGoal(scene, "near Player Flag 2", world));
    player[1] = -1.0f;
    EXPECT_FALSE(CheckGoal(scene, "landed Player Flag", world)) << "fell through";

    score = 2.0;
    EXPECT_TRUE(CheckGoal(scene, "reaches Player score 1", world));
}

// A typing mistake in a lesson shows up here, not as a step that can never be ticked
TEST(LessonGoals, BadChecksAreCaught)
{
    using Engine::Editor::IsValidGoal;
    EXPECT_FALSE(IsValidGoal(""));
    EXPECT_FALSE(IsValidGoal("object"));
    EXPECT_FALSE(IsValidGoal("dance Player"));
    EXPECT_FALSE(IsValidGoal("script Player when_play_starts mvoe_forward"));
    EXPECT_FALSE(IsValidGoal("near Player Flag far"));
    EXPECT_FALSE(IsValidGoal("played")) << "pressing Run proves nothing";
    EXPECT_FALSE(IsValidGoal("shape Ball blob"));
    EXPECT_FALSE(IsValidGoal("has Ball wings"));
    EXPECT_FALSE(IsValidGoal("number Wall turn_by x 90"));
    EXPECT_FALSE(CheckGoal(SceneOf({}), "uses Player nonsense", {}));
}

// The tour's words stay on the screen, beside what they point at and not over it
TEST(Tour, WordsGoBesideTheirTarget)
{
    using Editor::Tour::Box;
    using Editor::Tour::PlaceBeside;
    constexpr float kWidth = 1000.0f;
    constexpr float kHeight = 700.0f;
    const auto overlaps = [](const Box& a, const Box& b)
    { return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height; };

    for (const Box& target : {Box{10, 10, 100, 40}, Box{850, 10, 140, 40}, Box{400, 640, 200, 50}, Box{0, 0, 640, 690}})
    {
        const Box words = PlaceBeside(target, 300, 180, kWidth, kHeight, 16);
        EXPECT_GE(words.x, 0.0f);
        EXPECT_GE(words.y, 0.0f);
        EXPECT_LE(words.x + words.width, kWidth);
        EXPECT_LE(words.y + words.height, kHeight);
        EXPECT_FALSE(overlaps(words, target)) << target.x;
    }

    // Nowhere beside it: still on the screen
    const Box words = PlaceBeside({0, 0, 1000, 700}, 300, 180, kWidth, kHeight, 16);
    EXPECT_LE(words.y + words.height, kHeight);
}

// Every step has words, and what it lights up is marked somewhere (SceneEditor::TourMark)
TEST(Tour, EveryStepSaysSomething)
{
    for (const Editor::Tour::Step& step : Editor::Tour::kSteps)
    {
        EXPECT_NE(std::string(step.title), "");
        EXPECT_NE(std::string(step.text), "");
    }
    EXPECT_EQ(Editor::Tour::kSteps.front().target, nullptr) << "it starts with a welcome";
    EXPECT_FALSE(Editor::Tour::kSteps.front().workspace);
}

// The Advanced interface's tour stays in the workspace, and starts with a welcome too
TEST(Tour, TheAdvancedTourStaysInTheWorkspace)
{
    for (const Editor::Tour::Step& step : Editor::Tour::kAdvancedSteps)
    {
        EXPECT_NE(std::string(step.title), "");
        EXPECT_NE(std::string(step.text), "");
        EXPECT_TRUE(step.workspace) << step.title;
    }
    EXPECT_EQ(Editor::Tour::kAdvancedSteps.front().target, nullptr) << "it starts with a welcome";
}
