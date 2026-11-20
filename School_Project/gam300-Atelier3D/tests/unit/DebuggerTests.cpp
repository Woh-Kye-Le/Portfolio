#include "editor/panels/blocks/ExecutionHighlight.hpp"
#include "engine/assets/ResourceManager.hpp"
#include "engine/game/BlockPhysics.hpp"
#include "engine/graphics/DebugLines.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/logic/Library.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include "engine/logic/BreakpointFile.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

// The block debugger (T16): Pause, Step one block and breakpoints in Logic::LogicRunner. Execution Highlighting (F02):
// which blocks ran, ran before and run next, and so which keep their colour

using Editor::Panels::ExecutionHighlight::Collect;
using Editor::Panels::ExecutionHighlight::Highlight;
using Editor::Panels::ExecutionHighlight::HighlightOf;
using Editor::Panels::ExecutionHighlight::ScriptHighlights;
using Engine::Logic::Block;
using Engine::Logic::LogicDesc;
using Engine::Logic::LogicRunner;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;
using Engine::Logic::PauseReason;
using Engine::Logic::RunMode;
using Engine::Logic::Script;
using Engine::Scene::TransformComponent;
using State = LogicRunner::ScriptState;

namespace
{
    constexpr float kTolerance = 1e-4f;

    // A block with its numbers (and body) set
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

    LogicDesc OneScript(std::vector<Block> blocks)
    {
        return LogicDesc{.scripts = {Script{.x = 10.0f, .y = 20.0f, .blocks = std::move(blocks)}}};
    }

    // "when Play starts", then blocks 2 to 5 each move 1 along X: X counts how many ran
    std::vector<Block> FourMoves()
    {
        return {MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0}), Make(Op::MoveBy, 3, {1, 0, 0}),
                Make(Op::MoveBy, 4, {1, 0, 0}), Make(Op::MoveBy, 5, {1, 0, 0})};
    }

    // An ECS world with one entity at the origin, and a runner for its logic (object 1)
    class LogicDebugTest : public ::testing::Test
    {
    protected:
        void SetUp() override { entity_ = ecs_.CreateEntity(); }

        void Run(std::vector<Block> blocks) { runner_.Set(1, "Cube", entity_.GetId(), OneScript(std::move(blocks))); }

        void Update(double seconds = 1.0 / 60.0) { runner_.Update(ecs_, seconds, LogicRunner::Keys{}); }

        // StepOnce, then the frame that runs it
        void Step()
        {
            runner_.StepOnce();
            Update();
        }

        float X(Engine::Scene::Entity entity) const
        {
            return ecs_.GetTransform(entity).value_or(TransformComponent{}).x;
        }
        float X() const { return X(entity_); }

        // Where object 1's script at index is, for highlighting
        State At(std::size_t index = 0) const
        {
            const auto scripts = runner_.Scripts(1);
            return index < scripts.size() ? scripts[index] : State{};
        }

        Engine::Scene::Scene ecs_;
        Engine::Scene::Entity entity_;
        LogicRunner runner_;
    };
}

TEST_F(LogicDebugTest, BreakpointStopsBeforeItsBlock)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 4);
    Update();

    EXPECT_NEAR(X(), 2.0f, kTolerance); // blocks 2 and 3 ran, 4 did not
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedObject, 1u);
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Breakpoint);
}

// A My Block's blocks have ids of their own, counted from 1: a breakpoint on block 3 of the script is not one on
// block 3 of the My Block
TEST_F(LogicDebugTest, ABreakpointIsNotSetOffByABlockInAMyBlockWithTheSameId)
{
    Engine::Logic::Library library;
    library.blocks.push_back({.name = "Three Moves",
                              .blocks = {Make(Op::MoveBy, 1, {1, 0, 0}), Make(Op::MoveBy, 2, {1, 0, 0}),
                                         Make(Op::MoveBy, 3, {1, 0, 0})}});
    runner_.SetLibrary(library);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Three Moves", 2), Make(Op::MoveBy, 3, {10, 0, 0})});
    runner_.ToggleBreakpoint(1, 3);
    Update();
    EXPECT_NEAR(X(), 3.0f, kTolerance) << "all of the My Block ran first";
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    runner_.Resume();
    Update();
    EXPECT_NEAR(X(), 13.0f, kTolerance);
}

// A breakpoint on a My Block stops before it; stepping goes through its blocks, showing the My Block in the script as
// where it is (its blocks are not in the script, and their ids mean other blocks there)
TEST_F(LogicDebugTest, SteppingThroughAMyBlockShowsTheMyBlock)
{
    Engine::Logic::Library library;
    library.blocks.push_back(
        {.name = "Two Moves", .blocks = {Make(Op::MoveBy, 1, {1, 0, 0}), Make(Op::MoveBy, 2, {1, 0, 0})}});
    runner_.SetLibrary(library);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Two Moves", 7), Make(Op::MoveBy, 8, {10, 0, 0})});
    runner_.ToggleBreakpoint(1, 7);
    Update();
    ASSERT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 7u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);

    Step(); // into it
    EXPECT_EQ(runner_.Debug().pausedBlock, 7u);
    EXPECT_EQ(At().next, 7u) << "the script is still on the My Block";
    Step(); // its first move
    EXPECT_NEAR(X(), 1.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 7u);
    Step(); // its second
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 8u) << "done with it: on to the block after it";
    EXPECT_EQ(At().next, 8u);
}

TEST_F(LogicDebugTest, NothingRunsWhilePaused)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0})})});
    Update();
    runner_.Pause(1);
    for (int i = 0; i < 100; ++i)
    {
        Update();
    }
    EXPECT_NEAR(X(), 1.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
}

TEST_F(LogicDebugTest, ResumeRunsTheBreakpointBlockOnce)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 4);
    Update();
    runner_.Resume();
    Update();

    EXPECT_NEAR(X(), 4.0f, kTolerance);
    EXPECT_FALSE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().mode, RunMode::Running);
}

TEST_F(LogicDebugTest, StepRunsOneBlock)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_NEAR(X(), 1.0f, kTolerance);

    Step();
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Step);

    Step();
    EXPECT_NEAR(X(), 3.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 5u);

    Step();
    EXPECT_NEAR(X(), 4.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u); // its last block ran
    EXPECT_EQ(runner_.Debug().reason, PauseReason::StepLast);

    Step(); // the script is shown as ended, and stays there
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Step);
    EXPECT_NEAR(X(), 4.0f, kTolerance);
}

TEST_F(LogicDebugTest, StepFinishesAWait)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {2}), Make(Op::MoveBy, 3, {1, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    Step(); // one step for the wait: it starts and finishes at once
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);

    Step();
    EXPECT_NEAR(X(), 1.0f, kTolerance);
}

TEST_F(LogicDebugTest, StepFinishesAWaitPausedPartWay)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {2}), Make(Op::MoveBy, 3, {1, 0, 0})});
    Update(); // on the wait
    runner_.Pause(1);
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    Step();
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);
}

TEST_F(LogicDebugTest, StepFinishesAGlide)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::GlideTo, 2, {2, 5, 0, 0}), Make(Op::MoveBy, 3, {1, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    Update();

    Step(); // the glide reaches its end in the one step
    EXPECT_NEAR(X(), 5.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
}

// Two scripts: when the one being stepped ends, Step goes on with the other
TEST_F(LogicDebugTest, StepMovesOnWhenItsScriptEnds)
{
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0})});
    logic.scripts.push_back(
        Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 3), Make(Op::Wait, 4, {1}), Make(Op::MoveBy, 5, {10, 0, 0})}});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    runner_.ToggleBreakpoint(1, 2);
    Update(); // stops before block 2; the second script starts and waits

    Step(); // block 2 runs and its script ends: it stays there, with no next block
    EXPECT_NEAR(X(), 1.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedThread, 0);
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::StepLast);

    Step(); // the script shows as ended
    EXPECT_EQ(runner_.Debug().pausedThread, 0);
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Step);

    Step(); // on to the other script
    EXPECT_EQ(runner_.Debug().pausedThread, 1);
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u);

    Step(); // the wait
    Step();
    EXPECT_NEAR(X(), 11.0f, kTolerance);
}

// A click on a block picks the script Step runs (option D); Run then lets the first script go past its breakpoint
TEST_F(LogicDebugTest, FocusScriptPicksTheScriptStepRuns)
{
    LogicDesc logic = OneScript(FourMoves());
    logic.scripts.push_back(
        Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 6), Make(Op::Wait, 7, {1}), Make(Op::MoveBy, 8, {10, 0, 0})}});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    EXPECT_FALSE(runner_.FocusScript(1, 99)); // no such block
    EXPECT_TRUE(runner_.FocusScript(1, 8));
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Focus);
    EXPECT_EQ(runner_.Debug().pausedThread, 1);
    EXPECT_EQ(runner_.Debug().pausedBlock, 7u);

    Step();
    Step();
    EXPECT_NEAR(X(), 10.0f, kTolerance); // only the picked script ran

    runner_.Resume();
    Update();
    EXPECT_NEAR(X(), 14.0f, kTolerance); // the first script's breakpoint did not stop it again
}

TEST_F(LogicDebugTest, FocusScriptNeedsAPause)
{
    Run(FourMoves());
    EXPECT_FALSE(runner_.FocusScript(1, 2));
}

// ---- Step frame: one frame of every script, like Scratch's single step

namespace
{
    // StepFrame, then the frame that runs it
    void Frame(LogicRunner& runner, Engine::Scene::Scene& ecs)
    {
        runner.StepFrame();
        runner.Update(ecs, 1.0 / 60.0, LogicRunner::Keys{});
    }
}

TEST_F(LogicDebugTest, StepFrameRunsEveryScriptOnce)
{
    LogicDesc logic =
        OneScript({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0})})});
    logic.scripts.push_back(Script{
        .blocks = {MakeBlock(Op::WhenPlayStarts, 4), Make(Op::Forever, 5, {}, {Make(Op::MoveBy, 6, {10, 0, 0})})}});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    ASSERT_NEAR(X(), 11.0f, kTolerance);
    runner_.Pause(1);

    Frame(runner_, ecs_);
    EXPECT_NEAR(X(), 22.0f, kTolerance); // both scripts went round once
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_TRUE(runner_.FrameStepped());
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Frame);

    Update(); // paused: nothing runs, and the world stands still again
    EXPECT_FALSE(runner_.FrameStepped());
    EXPECT_NEAR(X(), 22.0f, kTolerance);
}

TEST_F(LogicDebugTest, StepFrameCountsAWaitDown)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {0.04f}), Make(Op::MoveBy, 3, {1, 0, 0})});
    Update(); // on the wait
    runner_.Pause(1);

    Frame(runner_, ecs_);
    Frame(runner_, ecs_);
    EXPECT_NEAR(X(), 0.0f, kTolerance); // two frames of 1/60 s: still waiting
    Frame(runner_, ecs_);
    EXPECT_NEAR(X(), 1.0f, kTolerance); // the wait is over
}

TEST_F(LogicDebugTest, StepFrameRunsThePausedBlockAndTheRest)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_NEAR(X(), 1.0f, kTolerance);

    Frame(runner_, ecs_); // the breakpoint block runs; the script runs to its end
    EXPECT_NEAR(X(), 4.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u); // no script is left to step
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Frame);
}

TEST_F(LogicDebugTest, StepFrameStopsAtALaterBreakpoint)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    runner_.ToggleBreakpoint(1, 5);
    Update();

    Frame(runner_, ecs_);
    EXPECT_NEAR(X(), 3.0f, kTolerance); // 3 and 4 ran, 5 did not
    EXPECT_EQ(runner_.Debug().pausedBlock, 5u);
    EXPECT_EQ(runner_.Debug().reason, PauseReason::Breakpoint);
}

TEST_F(LogicDebugTest, StepFrameKeepsTheScriptAfterTheFrame)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0})})});
    Update();
    runner_.Pause(1);

    Frame(runner_, ecs_);
    EXPECT_EQ(runner_.Debug().pausedObject, 1u);
    EXPECT_EQ(runner_.Debug().pausedThread, 0);
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u); // a block step can go on from here: the loop block comes round

    Step();
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step();
    EXPECT_NEAR(X(), 3.0f, kTolerance);
}

// How long the player moves in a frame: the debugger holds the world still, and a frame step is one step long
TEST_F(LogicDebugTest, ThePlayerMovesThroughTheFrameWhileRunning)
{
    Run(FourMoves());
    EXPECT_EQ(runner_.PlayerSeconds(1, 0.025), 0.025);
    EXPECT_EQ(runner_.PlayerSeconds(0, 0.004), 0.004); // a short frame with no fixed step still moves it
}

TEST_F(LogicDebugTest, ThePlayerStandsStillWhilePaused)
{
    Run(FourMoves());
    runner_.Pause(1);
    EXPECT_FALSE(runner_.PlayerSeconds(1, 0.025).has_value());
    EXPECT_FALSE(runner_.PlayerSeconds(3, 0.05).has_value());
}

TEST_F(LogicDebugTest, TheStepsOfAFrameStepMoveThePlayerOneFixedStepNoMatterTheFrameTime)
{
    Run(FourMoves());
    runner_.Pause(1);
    runner_.StepFrame();
    EXPECT_EQ(runner_.PlayerSeconds(1, 0.004), LogicRunner::kFrameSeconds); // a fast frame
    EXPECT_EQ(runner_.PlayerSeconds(1, 0.1), LogicRunner::kFrameSeconds);   // a slow one
    EXPECT_EQ(runner_.PlayerSeconds(3, 0.05), LogicRunner::kFrameSeconds);  // more steps: still one is the frame step
}

TEST_F(LogicDebugTest, AFrameWithNoStepToRunDoesNotMoveThePlayerInAFrameStep)
{
    Run(FourMoves());
    runner_.Pause(1);
    runner_.StepFrame();
    EXPECT_FALSE(runner_.PlayerSeconds(0, 0.004).has_value()); // the step runs in a later frame, where it moves
    runner_.Update(ecs_, 1.0 / 60.0, LogicRunner::Keys{});
    EXPECT_FALSE(runner_.PlayerSeconds(1, 0.025).has_value()); // the frame step is done: paused again
}

TEST_F(LogicDebugTest, ABlockStepDoesNotMoveThePlayer)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    Update();
    runner_.StepOnce();
    EXPECT_FALSE(runner_.PlayerSeconds(1, 0.025).has_value());
}

TEST_F(LogicDebugTest, StepFrameNeedsAPause)
{
    Run(FourMoves());
    runner_.StepFrame(); // not paused: nothing
    Update();
    EXPECT_EQ(runner_.Debug().mode, RunMode::Running);
    EXPECT_NEAR(X(), 4.0f, kTolerance);
}

// ---- Step through the control blocks and across objects

namespace
{
    // "left > right", for a condition slot
    Block GreaterThan(std::uint32_t id, Block left, float right)
    {
        Block block = MakeBlock(Op::Greater, id);
        block.inputs[0] = std::move(left);
        block.numbers[1] = right;
        return block;
    }

    // A condition that is never true (0 > 1)
    Block Never(std::uint32_t id)
    {
        Block block = MakeBlock(Op::Greater, id);
        block.numbers[0] = 0.0f;
        block.numbers[1] = 1.0f;
        return block;
    }
}

// A "wait until" whose condition is false stays where it is, one Step at a time, until the condition becomes true
TEST_F(LogicDebugTest, StepStaysOnAWaitUntilUntilItsConditionIsTrue)
{
    Block wait = MakeBlock(Op::WaitUntil, 2);
    wait.slots[0] = GreaterThan(6, MakeBlock(Op::PositionX, 7), 0.5f); // x > 0.5
    LogicDesc logic = OneScript({MakeBlock(Op::WhenPlayStarts, 1), wait, Make(Op::MoveBy, 3, {10, 0, 0})});
    logic.scripts.push_back(Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 4), Make(Op::MoveBy, 5, {1, 0, 0})}});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    Step(); // false: it waits, and Step says it is still on it
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);

    ASSERT_TRUE(runner_.FocusScript(1, 5)); // the other script makes it true
    Step();
    EXPECT_NEAR(X(), 1.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u); // that script ended: it stays there, with no next block
    EXPECT_EQ(runner_.Debug().pausedObject, 1u);

    Step(); // that script shows as ended
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);

    Step(); // on to the waiting one
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u);

    Step(); // true now: it goes on
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step();
    EXPECT_NEAR(X(), 11.0f, kTolerance);
}

TEST_F(LogicDebugTest, StepRunsARepeatUntilRoundByRound)
{
    Block loop = Make(Op::RepeatUntil, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0})});
    loop.slots[0] = GreaterThan(4, MakeBlock(Op::PositionX, 5), 2.5f); // until x > 2.5
    Run({MakeBlock(Op::WhenPlayStarts, 1), loop});
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    int steps = 0;
    while (runner_.Debug().pausedBlock != 0 && steps < 20)
    {
        const std::uint32_t before = runner_.Debug().pausedBlock;
        Step();
        ++steps;
        // The condition is checked, then the body runs, over and over: the loop block and its inside alternate
        EXPECT_TRUE(before == 2u || before == 3u);
    }
    EXPECT_NEAR(X(), 3.0f, kTolerance);
    EXPECT_EQ(steps, 7); // check, move, check, move, check, move, check (true: the script ends)
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
}

TEST_F(LogicDebugTest, StepEntersOnlyTheBranchThatRuns)
{
    Block branch = MakeBlock(Op::IfElse, 2);
    branch.slots[0] = Never(6);
    branch.body = {Make(Op::MoveBy, 3, {1, 0, 0})};
    branch.elseBody = {Make(Op::MoveBy, 4, {10, 0, 0})};
    Run({MakeBlock(Op::WhenPlayStarts, 1), branch, Make(Op::MoveBy, 5, {100, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    Update();

    Step(); // the condition is false: it goes into the else
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);

    Step();
    EXPECT_NEAR(X(), 10.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 5u); // the else is done: on to the block after the If

    Step();
    EXPECT_NEAR(X(), 110.0f, kTolerance); // the "then" blocks never ran
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
}

TEST_F(LogicDebugTest, StepOnStopEndsTheScript)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0}), MakeBlock(Op::Stop, 3),
         Make(Op::MoveBy, 4, {100, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    Update();

    Step();
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step(); // "stop this script"
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);
    EXPECT_NEAR(X(), 1.0f, kTolerance); // the block after it never runs
}

TEST_F(LogicDebugTest, StepRunsARepeatOneRoundAtATime)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Repeat, 2, {2}, {Make(Op::MoveBy, 3, {1, 0, 0})}),
         Make(Op::MoveBy, 4, {10, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    Update();

    Step(); // the repeat starts: next is its first block
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step();
    EXPECT_NEAR(X(), 1.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u); // the repeat comes round again, like a loop check
    Step(); // round two
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step();
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u); // two rounds only
    Step();
    EXPECT_NEAR(X(), 12.0f, kTolerance);
}

// A script that ends stays where it is, with no next block. The next Step goes on with the next object's script
TEST_F(LogicDebugTest, StepMovesOnToTheNextObject)
{
    const Engine::Scene::Entity other = ecs_.CreateEntity();
    const auto twoMoves = [](std::uint32_t first)
    {
        return std::vector<Block>{MakeBlock(Op::WhenPlayStarts, first), Make(Op::MoveBy, first + 1, {1, 0, 0}),
                                  Make(Op::MoveBy, first + 2, {1, 0, 0})};
    };
    Run(twoMoves(1));
    runner_.Set(2, "Other", other.GetId(), OneScript(twoMoves(1)));
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_EQ(runner_.Debug().pausedObject, 1u);

    Step();
    Step(); // the first object's script ends: it stays there
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedObject, 1u);
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);

    Step(); // it shows as ended
    EXPECT_EQ(runner_.Debug().pausedObject, 1u);
    EXPECT_EQ(runner_.Debug().pausedBlock, 0u);

    Step(); // on to the next object
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_EQ(runner_.Debug().pausedObject, 2u);
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u);
    EXPECT_NEAR(X(other), 0.0f, kTolerance); // nothing of it ran yet

    Step();
    EXPECT_NEAR(X(other), 1.0f, kTolerance);
    EXPECT_NEAR(X(), 2.0f, kTolerance); // the first object stood still
}

TEST_F(LogicDebugTest, BreakpointStopsEveryObject)
{
    const Engine::Scene::Entity other = ecs_.CreateEntity();
    Run(FourMoves());
    runner_.Set(2, "Other", other.GetId(), OneScript(FourMoves()));
    runner_.ToggleBreakpoint(1, 2);
    Update();

    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_NEAR(X(), 0.0f, kTolerance);
    EXPECT_NEAR(X(other), 0.0f, kTolerance);
}

TEST_F(LogicDebugTest, HatBreakpointStopsAsTheScriptStarts)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 1);
    Update();
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 1u);
    EXPECT_NEAR(X(), 0.0f, kTolerance);

    runner_.Resume();
    Update();
    EXPECT_NEAR(X(), 4.0f, kTolerance);
}

TEST_F(LogicDebugTest, PauseInALoopThenStep)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0})})});
    Update();
    ASSERT_NEAR(X(), 1.0f, kTolerance);

    runner_.Pause(1);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().reason, PauseReason::User);
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u); // the loop block comes round first

    Step();
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
    Step();
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 2u); // round again
}

TEST_F(LogicDebugTest, EditWhilePausedStaysPaused)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_NEAR(X(), 1.0f, kTolerance);

    Run(FourMoves()); // an edit: the scripts start again
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedObject, 0u);

    Step(); // stopped in no script: runs until one block runs
    EXPECT_NEAR(X(), 2.0f, kTolerance);
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 3u);
}

TEST_F(LogicDebugTest, BreakpointsSurviveSetAndClear)
{
    runner_.ToggleBreakpoint(1, 3);
    Run(FourMoves());
    runner_.Clear();
    EXPECT_TRUE(runner_.HasBreakpoint(1, 3));

    runner_.ToggleBreakpoint(1, 3);
    EXPECT_FALSE(runner_.HasBreakpoint(1, 3));
    EXPECT_TRUE(runner_.Debug().breakpoints.empty());

    runner_.SetBreakpoints(1, {2, 4});
    EXPECT_TRUE(runner_.HasBreakpoint(1, 4));
    runner_.ClearBreakpoints();
    EXPECT_FALSE(runner_.HasBreakpoint(1, 2));
}

TEST_F(LogicDebugTest, ClearRunsFreely)
{
    Run(FourMoves());
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_TRUE(runner_.IsPaused());

    runner_.Clear();
    EXPECT_EQ(runner_.Debug().mode, RunMode::Running);
    EXPECT_EQ(runner_.Debug().pausedObject, 0u);
}

TEST_F(LogicDebugTest, StepAndResumeNeedAPause)
{
    Run(FourMoves());
    runner_.StepOnce(); // not paused: nothing
    Update();
    EXPECT_EQ(runner_.Debug().mode, RunMode::Running);
    EXPECT_NEAR(X(), 4.0f, kTolerance);
}

// A breakpoint in one object must not stop a later object's script from hearing a message sent last frame
TEST_F(LogicDebugTest, MessagesStillStartReceiversWhilePaused)
{
    const Engine::Scene::Entity other = ecs_.CreateEntity();
    Block send = MakeBlock(Op::Broadcast, 2);
    send.text = "go";
    Run({MakeBlock(Op::WhenPlayStarts, 1), send, Make(Op::Wait, 3, {0.01f}), Make(Op::MoveBy, 4, {1, 0, 0})});
    Block hear = MakeBlock(Op::WhenIReceive, 1);
    hear.text = "go";
    runner_.Set(2, "Receiver", other.GetId(), OneScript({hear, Make(Op::MoveBy, 2, {1, 0, 0})}));
    runner_.ToggleBreakpoint(1, 4);

    Update(); // broadcasts, then waits
    Update(); // the wait ends at the breakpoint; the receiver hears "go" the same frame
    ASSERT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.RunningScripts(2), 1u);
    EXPECT_NEAR(X(other), 0.0f, kTolerance); // started, but no block ran

    runner_.Resume();
    Update();
    EXPECT_NEAR(X(other), 1.0f, kTolerance);
    EXPECT_NEAR(X(), 1.0f, kTolerance);
}

// The debugger's lines (T16): Graphics::DebugLines' shapes and Systems::DebugLineSystem's boxes around objects

using Engine::Graphics::DebugLines;
using Engine::Systems::DebugLineSettings;
using Engine::Systems::DebugLineSystem;

namespace
{
    const glm::vec4 kWhite(1.0f);

    // Every point of the lines is within the box from low to high
    void ExpectWithin(const DebugLines& lines, const glm::vec3& low, const glm::vec3& high)
    {
        for (const auto& vertex : lines.Vertices())
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                EXPECT_GE(vertex.position[axis], low[axis] - kTolerance);
                EXPECT_LE(vertex.position[axis], high[axis] + kTolerance);
            }
        }
    }
}

TEST(DebugLines, BoxHasTwelveEdgesOnItsCorners)
{
    DebugLines lines;
    lines.Box(glm::mat4(1.0f), glm::vec3(0.0f), glm::vec3(0.5f), kWhite);
    ASSERT_EQ(lines.Vertices().size(), 24u);
    for (const auto& vertex : lines.Vertices())
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            EXPECT_NEAR(std::abs(vertex.position[axis]), 0.5f, kTolerance);
        }
    }

    lines.Clear();
    EXPECT_TRUE(lines.Empty());
    lines.Box(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)), glm::vec3(0.0f), glm::vec3(0.5f), kWhite);
    EXPECT_NEAR(std::abs(lines.Vertices().front().position.x), 1.0f, kTolerance);
}

TEST(DebugLines, SphereIsThreeCirclesOfItsRadius)
{
    DebugLines lines;
    const glm::vec3 center(1.0f, 2.0f, 3.0f);
    lines.Sphere(center, 2.0f, kWhite);
    EXPECT_EQ(lines.Vertices().size(), 3u * 32u * 2u);
    for (const auto& vertex : lines.Vertices())
    {
        EXPECT_NEAR(glm::length(vertex.position - center), 2.0f, kTolerance);
    }
}

TEST(DebugLines, CapsuleFitsItsHeightAndRadius)
{
    DebugLines lines;
    lines.Capsule(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.5f, 2.0f, kWhite);
    EXPECT_FALSE(lines.Empty());
    ExpectWithin(lines, glm::vec3(-0.5f, -1.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
    const auto top = std::max_element(lines.Vertices().begin(), lines.Vertices().end(),
                                      [](const auto& a, const auto& b) { return a.position.y < b.position.y; });
    EXPECT_NEAR(top->position.y, 1.0f, kTolerance); // the rounded end reaches the full height
}

TEST(DebugLines, RayGoesToItsHitOrItsLength)
{
    DebugLines lines;
    lines.Ray(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -2.0f), 5.0f, std::nullopt, kWhite, kWhite);
    ASSERT_EQ(lines.Vertices().size(), 2u);
    EXPECT_NEAR(lines.Vertices()[1].position.z, -5.0f, kTolerance);

    lines.Clear();
    const glm::vec3 hit(0.0f, 0.0f, -3.0f);
    lines.Ray(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 5.0f, hit, kWhite, kWhite);
    ASSERT_EQ(lines.Vertices().size(), 10u); // up to the hit, the rest of its length, and a cross of three lines
    EXPECT_NEAR(glm::length(lines.Vertices()[1].position - hit), 0.0f, kTolerance);
    EXPECT_NEAR(lines.Vertices()[3].position.z, -5.0f, kTolerance);
}

TEST(DebugLines, ShapeBoundsMatchTheBuiltInMeshes)
{
    const auto [cubeLow, cubeHigh] = DebugLineSystem::ShapeBounds(Engine::Game::Shape::Cube);
    EXPECT_NEAR(cubeLow.x, -0.5f, kTolerance);
    EXPECT_NEAR(cubeHigh.y, 0.5f, kTolerance);

    const auto [sphereLow, sphereHigh] = DebugLineSystem::ShapeBounds(Engine::Game::Shape::Sphere);
    EXPECT_NEAR(sphereLow.y, -1.0f, kTolerance);
    EXPECT_NEAR(sphereHigh.x, 1.0f, kTolerance);

    const auto [planeLow, planeHigh] = DebugLineSystem::ShapeBounds(Engine::Game::Shape::Plane);
    EXPECT_NEAR(planeLow.x, -0.5f, kTolerance);
    EXPECT_NEAR(planeHigh.z, 0.5f, kTolerance);
    EXPECT_NEAR(planeHigh.y - planeLow.y, 0.0f, kTolerance); // flat
}

TEST(DebugLines, BoundsBoxScalesWithTheObject)
{
    Engine::Scene::Scene ecs;
    const Engine::Scene::Entity entity = ecs.CreateEntity();
    ecs.TryGetComponent<Engine::Scene::TransformComponent>(entity)->scale = 2.0f;
    ASSERT_TRUE(ecs.AddComponent(entity, Engine::Scene::MeshRendererComponent{}));
    Engine::Assets::ResourceManager resources;
    DebugLines lines;

    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{}, lines);
    EXPECT_TRUE(lines.Empty()); // nothing switched on

    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.bounds = true}, lines);
    ASSERT_EQ(lines.Vertices().size(), 24u);
    ExpectWithin(lines, glm::vec3(-1.0f), glm::vec3(1.0f));
    EXPECT_NEAR(std::abs(lines.Vertices().front().position.x), 1.0f, kTolerance);

    lines.Clear();
    ecs.TryGetComponent<Engine::Scene::MeshRendererComponent>(entity)->visible = false;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.bounds = true}, lines);
    EXPECT_TRUE(lines.Empty());
}

TEST(DebugLines, BoundsBoxShowsUpOnLightAndDarkObjects)
{
    // Light objects get the dark box
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(1.0f, 1.0f, 1.0f), DebugLineSystem::kBoundsDarkColor);
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(1.0f, 1.0f, 0.6f), DebugLineSystem::kBoundsDarkColor);
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(0.8f, 0.8f, 0.8f), DebugLineSystem::kBoundsDarkColor);
    // Dark objects get the white one
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(0.0f, 0.0f, 0.0f), DebugLineSystem::kBoundsLightColor);
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(1.0f, 0.0f, 0.0f), DebugLineSystem::kBoundsLightColor);
    EXPECT_EQ(DebugLineSystem::BoundsColorFor(0.2f, 0.2f, 0.2f), DebugLineSystem::kBoundsLightColor);
}

TEST(DebugLines, CollectColoursTheBoxByTheObjectsTint)
{
    Engine::Scene::Scene ecs;
    const Engine::Scene::Entity entity = ecs.CreateEntity();
    Engine::Scene::MeshRendererComponent renderer;
    renderer.albedoR = renderer.albedoG = renderer.albedoB = 0.0f;
    ASSERT_TRUE(ecs.AddComponent(entity, renderer));
    Engine::Assets::ResourceManager resources;

    DebugLines dark;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.bounds = true}, dark);
    ASSERT_FALSE(dark.Empty());
    for (const auto& vertex : dark.Vertices())
    {
        EXPECT_EQ(vertex.color, DebugLineSystem::kBoundsLightColor);
    }

    renderer.albedoR = renderer.albedoG = renderer.albedoB = 1.0f;
    *ecs.TryGetComponent<Engine::Scene::MeshRendererComponent>(entity) = renderer;
    DebugLines light;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.bounds = true}, light);
    ASSERT_FALSE(light.Empty());
    for (const auto& vertex : light.Vertices())
    {
        EXPECT_EQ(vertex.color, DebugLineSystem::kBoundsDarkColor);
    }
}

namespace
{
    // A collider outline of a box's 12 edges, as PhysicsSystem::Outlines makes them
    Engine::Systems::ColliderOutline BoxOutline(Engine::Scene::BodyType type, bool asleep = false, bool touching = true)
    {
        Engine::Systems::ColliderOutline outline;
        outline.type = type;
        outline.asleep = asleep;
        outline.touching = touching;
        for (int edge = 0; edge < 12; ++edge)
        {
            const float x = static_cast<float>(edge);
            outline.lines.push_back({{x, 0.0f, 0.0f}, {x, 1.0f, 0.0f}});
        }
        return outline;
    }
}

TEST(DebugLines, CollectDrawsEveryColliderLine)
{
    Engine::Scene::Scene ecs;
    Engine::Assets::ResourceManager resources;
    const std::vector<Engine::Systems::ColliderOutline> outlines{BoxOutline(Engine::Scene::BodyType::Static),
                                                                 BoxOutline(Engine::Scene::BodyType::Dynamic)};

    DebugLines on;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.colliders = true}, outlines, {}, on);
    EXPECT_EQ(on.Vertices().size(), 2u * 12u * 2u); // two boxes, 12 lines each, two vertices per line
    EXPECT_EQ(on.Vertices().front().position, glm::vec3(0.0f, 0.0f, 0.0f));

    DebugLines off;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.bounds = true}, outlines, {}, off);
    EXPECT_TRUE(off.Empty()); // colliders are off, and there is nothing to put a bounds box round
}

TEST(DebugLines, CollectDrawsRaysToTheirHitOrTheirLength)
{
    Engine::Scene::Scene ecs;
    Engine::Assets::ResourceManager resources;
    const Engine::Systems::CastRay hit{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f, glm::vec3(4.0f, 0.0f, 0.0f)};
    const Engine::Systems::CastRay miss{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 3.0f, std::nullopt};

    DebugLines lines;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.rays = true}, {}, {hit, miss}, lines);
    // The hit ray: green to the hit and on for the rest of its length, a three-line green cross there; the miss: one line,
    // its whole length, all red
    ASSERT_EQ(lines.Vertices().size(), 12u);
    EXPECT_EQ(lines.Vertices()[0].position, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[1].position, glm::vec3(4.0f, 0.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[0].color, DebugLineSystem::kRayHitColor);
    EXPECT_EQ(lines.Vertices()[2].position, glm::vec3(4.0f, 0.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[3].position, glm::vec3(10.0f, 0.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[2].color, DebugLineSystem::kRayHitColor);
    EXPECT_EQ(lines.Vertices().back().position, glm::vec3(0.0f, 4.0f, 0.0f));
    EXPECT_EQ(lines.Vertices().back().color, DebugLineSystem::kRayColor);

    DebugLines off;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.colliders = true}, {}, {hit, miss}, off);
    EXPECT_TRUE(off.Empty());
}

// A ground check draws straight down to the ground's height, wherever on the object it touches, with nothing past it.
TEST(DebugLines, AGroundCheckIsStraightDown)
{
    const glm::vec3 origin(2.0f, 5.0f, -1.0f);
    const glm::vec3 gravity(0.0f, -9.81f, 0.0f);

    const Engine::Systems::CastRay found = DebugLineSystem::GroundRay(origin, gravity, glm::vec3(2.4f, 4.5f, -0.6f));
    EXPECT_EQ(found.origin, origin);
    EXPECT_EQ(found.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_FLOAT_EQ(found.length, 0.5f);
    ASSERT_TRUE(found.hit.has_value());
    EXPECT_EQ(*found.hit, glm::vec3(2.0f, 4.5f, -1.0f)); // under the centre, not off at the touching point

    DebugLines lines;
    lines.Ray(found.origin, found.direction, found.length, found.hit, DebugLineSystem::kRayColor,
              DebugLineSystem::kRayHitColor);
    EXPECT_EQ(lines.Vertices().size(), 8u); // the ray and its cross, no part past the hit

    // Nothing found: a short ray; no gravity: still down
    const Engine::Systems::CastRay none = DebugLineSystem::GroundRay(origin, glm::vec3(0.0f), std::nullopt);
    EXPECT_EQ(none.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_FLOAT_EQ(none.length, DebugLineSystem::kGroundProbeLength);
    EXPECT_FALSE(none.hit.has_value());
}

namespace
{
    // Real blocks in a real physics world, with the rays they cast kept as the Raycasts debug lines would get them. A
    // floor under a box that starts on it, and a wall 5 units in front of the box (it faces -Z).
    class BlockRaysTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            using Engine::Scene::BodyType;
            using Engine::Scene::RigidBodyComponent;
            ASSERT_TRUE(world_.Init());
            const auto floor = ecs_.CreateEntity();
            ecs_.SetTransform(floor, TransformComponent{.y = -0.5f, .scaleX = 40.0f, .scaleZ = 40.0f});
            ecs_.AddComponent(floor, RigidBodyComponent{.type = BodyType::Static});
            const auto wall = ecs_.CreateEntity();
            ecs_.SetTransform(wall, TransformComponent{.y = 1.0f, .z = -5.5f, .scaleX = 10.0f, .scaleY = 4.0f});
            ecs_.AddComponent(wall, RigidBodyComponent{.type = BodyType::Static});
            box_ = ecs_.CreateEntity();
            ecs_.SetTransform(box_, TransformComponent{.y = 0.5f});
            ecs_.AddComponent(box_, RigidBodyComponent{});
            // As GameRuntime::CastRay does, which needs a running game (its own tests are in GameRuntimeTests.cpp)
            physics_.SetRayCaster(
                [this](const glm::vec3& origin, const glm::vec3& direction, float length, Engine::Scene::Entity ignore)
                {
                    auto hit = Engine::Systems::PhysicsSystem::Raycast(ecs_, world_, origin, direction, length, ignore);
                    rays_.push_back({origin, direction, length,
                                     hit ? std::optional<glm::vec3>(hit->point) : std::nullopt});
                    return hit;
                });
            physics_.SetRayRecorder([this](const Engine::Systems::CastRay& ray) { rays_.push_back(ray); });
            runner_.SetPhysics(&physics_);
            runner_.SetObjects({{.name = "Box", .entity = box_.GetId()}});
        }

        void TearDown() override { world_.Shutdown(); }

        // A frame as GameRuntime::Tick runs one
        void Frames(int count)
        {
            for (int i = 0; i < count; ++i)
            {
                physics_.Bind(ecs_, world_);
                runner_.Update(ecs_, 1.0 / 60.0, LogicRunner::Keys{});
                physics_.BeforeStep(1.0 / 60.0);
                Engine::Systems::PhysicsSystem::Update(ecs_, world_, 1.0 / 60.0);
                physics_.AfterStep();
            }
        }

        // Runs a block every frame, and forgets the rays of the frames the box takes to settle
        void RunForever(Block block, int settle = 30)
        {
            Block loop = Make(Op::Forever, 2, {}, {std::move(block)});
            runner_.Set(1, "Box", box_.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), std::move(loop)}));
            Frames(settle);
            rays_.clear();
        }

        Engine::Scene::Scene ecs_;
        Engine::Physics::PhysicsWorld world_;
        Engine::Game::BlockPhysics physics_;
        LogicRunner runner_;
        Engine::Scene::Entity box_;
        std::vector<Engine::Systems::CastRay> rays_;
    };

    // An "if" that only asks
    Block IfBlock(Block condition)
    {
        Block test = Make(Op::If, 3);
        test.slots[0] = std::move(condition);
        return test;
    }
}

// The ray a block casts through the physics reaches the debug lines: from the box, up to the wall in front of it, not
// stopped by the box's own collider. One too short for the wall is a miss. (No block casts one yet: the Raycast block of
// F12 will, through PhysicsAccess::Raycast.)
TEST_F(BlockRaysTest, ARayFromTheBlockPhysicsIsRecorded)
{
    Frames(1);
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    EXPECT_TRUE(physics_.Raycast(box_, forward, 10.0f));
    ASSERT_EQ(rays_.size(), 1u);
    EXPECT_NEAR(rays_[0].origin.z, 0.0f, 0.01f);
    EXPECT_NEAR(rays_[0].direction.z, -1.0f, kTolerance);
    EXPECT_FLOAT_EQ(rays_[0].length, 10.0f);
    ASSERT_TRUE(rays_[0].hit.has_value());
    EXPECT_NEAR(rays_[0].hit->z, -5.0f, 0.01f);

    rays_.clear();
    EXPECT_FALSE(physics_.Raycast(box_, forward, 2.0f));
    ASSERT_EQ(rays_.size(), 1u);
    EXPECT_FALSE(rays_[0].hit.has_value());
}

// "on the ground?" draws down to the floor while it stands on it
TEST_F(BlockRaysTest, OnGroundRecordsItsCheck)
{
    RunForever(IfBlock(MakeBlock(Op::OnGround, 4)));
    Frames(1);
    ASSERT_EQ(rays_.size(), 1u);
    EXPECT_EQ(rays_[0].direction, glm::vec3(0.0f, -1.0f, 0.0f));
    ASSERT_TRUE(rays_[0].hit.has_value());
    EXPECT_NEAR(rays_[0].hit->y, 0.0f, 0.05f);
}

// Jump checks the ground too: it finds it when it jumps, and nothing while it is in the air
TEST_F(BlockRaysTest, JumpRecordsItsCheck)
{
    RunForever(Make(Op::Jump, 4, {3}), 0);
    Frames(240); // up and back down
    ASSERT_FALSE(rays_.empty());
    const auto found = [](const Engine::Systems::CastRay& ray) { return ray.hit.has_value(); };
    EXPECT_TRUE(std::any_of(rays_.begin(), rays_.end(), found));
    EXPECT_FALSE(std::all_of(rays_.begin(), rays_.end(), found));
    EXPECT_FLOAT_EQ(rays_[0].direction.y, -1.0f);
}

// Jump of height 0 asks nothing, and blocks with no caster or recorder set still answer
TEST_F(BlockRaysTest, NoCheckNoRay)
{
    RunForever(Make(Op::Jump, 4, {0}));
    Frames(5);
    EXPECT_TRUE(rays_.empty());

    physics_.SetRayCaster(nullptr);
    EXPECT_FALSE(physics_.Raycast(box_, glm::vec3(0.0f, 0.0f, -1.0f), 10.0f));
    EXPECT_TRUE(rays_.empty());

    physics_.SetRayRecorder(nullptr);
    RunForever(IfBlock(MakeBlock(Op::OnGround, 4)));
    Frames(1);
    EXPECT_TRUE(rays_.empty());
    EXPECT_TRUE(runner_.Problems(1).empty());
}

TEST(DebugLines, CollidersAreColouredByBodyType)
{
    using Engine::Scene::BodyType;
    EXPECT_EQ(DebugLineSystem::ColliderColorFor(BoxOutline(BodyType::Static)), DebugLineSystem::kStaticColor);
    EXPECT_EQ(DebugLineSystem::ColliderColorFor(BoxOutline(BodyType::Dynamic)), DebugLineSystem::kDynamicColor);
    EXPECT_EQ(DebugLineSystem::ColliderColorFor(BoxOutline(BodyType::Kinematic)), DebugLineSystem::kKinematicColor);

    // A resting body is faded, and one that touches nothing is a little dimmer than one that touches
    const glm::vec4 asleep = DebugLineSystem::ColliderColorFor(BoxOutline(BodyType::Dynamic, true));
    EXPECT_FLOAT_EQ(asleep.a, DebugLineSystem::kAsleepAlpha);
    const glm::vec4 apart = DebugLineSystem::ColliderColorFor(BoxOutline(BodyType::Dynamic, false, false));
    EXPECT_LT(apart.r, DebugLineSystem::kDynamicColor.r);
    EXPECT_FLOAT_EQ(apart.a, 1.0f);
}

// ---------------------------------------------------------------- Execution Highlighting (F02)

namespace
{
    // A condition that is always true (1 > 0)
    Block Always(std::uint32_t id)
    {
        Block block = MakeBlock(Op::Greater, id);
        block.numbers[0] = 1.0f;
        block.numbers[1] = 0.0f;
        return block;
    }
}

// The three rules for every kind of block that runs, stepped from the hat to the end: the block a Step ran glows (the
// only one), every block that ran before stays faded, and the block the next Step runs is the "ready" one. The end
// block glows for the Step that ran it and is faded after the next one
TEST_F(LogicDebugTest, EveryKindOfBlockFollowsTheThreeRules)
{
    Block ifTrue = Make(Op::If, 5, {}, {Make(Op::MoveBy, 6, {1, 0, 0})});
    ifTrue.slots[0] = Always(30);
    Block ifElse = MakeBlock(Op::IfElse, 7);
    ifElse.slots[0] = Never(31);
    ifElse.body = {Make(Op::MoveBy, 8, {1, 0, 0})};
    ifElse.elseBody = {Make(Op::MoveBy, 9, {1, 0, 0})};
    Block until = Make(Op::RepeatUntil, 12, {}, {Make(Op::MoveBy, 13, {1, 0, 0})});
    until.slots[0] = Always(32);
    Block waitUntil = MakeBlock(Op::WaitUntil, 14);
    waitUntil.slots[0] = Always(33);
    Block stopIf = Make(Op::If, 19, {}, {MakeBlock(Op::Stop, 20)});
    stopIf.slots[0] = Always(34);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0}), Make(Op::Wait, 3, {0.5f}),
         Make(Op::GlideTo, 4, {0.5f, 1, 0, 0}), ifTrue, ifElse,
         Make(Op::Repeat, 10, {2}, {Make(Op::MoveBy, 11, {1, 0, 0})}), until, waitUntil, MakeBlock(Op::Broadcast, 15),
         Make(Op::Forever, 17, {}, {Make(Op::MoveBy, 18, {1, 0, 0}), stopIf})});
    EXPECT_EQ(At(), State{}); // not started: nowhere
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_TRUE(runner_.IsPaused());
    EXPECT_EQ(At(), (State{.script = 0, .running = true, .last = 1, .next = 2})); // on its hat

    std::vector<std::uint32_t> glowed; // every block that has glowed so far
    for (int step = 1; step <= 60 && At().running; ++step)
    {
        const std::uint32_t ran = runner_.Debug().pausedBlock; // the block this Step runs
        Step();
        const State now = At();
        const ScriptHighlights found = Collect({now});
        SCOPED_TRACE("step " + std::to_string(step) + ", ran block " + std::to_string(ran));

        ASSERT_EQ(found.current, (std::vector<std::uint32_t>{ran}));
        for (const std::uint32_t block : glowed)
        {
            EXPECT_NE(HighlightOf(found, block), Highlight::None) << "block " << block << " lost its fade";
        }
        if (now.running)
        {
            EXPECT_EQ(found.next, (std::vector<std::uint32_t>{runner_.Debug().pausedBlock}));
        }
        else
        {
            EXPECT_TRUE(found.next.empty());
        }
        glowed.push_back(ran);
    }
    ASSERT_FALSE(At().running) << "the script never reached its Stop";
    EXPECT_EQ(At().last, 20u);
    EXPECT_TRUE(At().justEnded);
    for (int i = 0; i < 10; ++i)
    {
        Update(); // paused: it stays as it is
    }
    EXPECT_TRUE(At().justEnded);

    Step(); // the game goes on: the end block is faded like the rest, and nothing glows or is next
    const ScriptHighlights after = Collect({At()});
    EXPECT_TRUE(after.current.empty());
    EXPECT_TRUE(after.next.empty());
    EXPECT_EQ(HighlightOf(after, 20), Highlight::Previous);
}

// Every other statement block: each glows when it runs, the next one is "ready", all the earlier ones are faded
TEST_F(LogicDebugTest, EveryStatementBlockFollowsTheThreeRules)
{
    const std::vector<Op> ops = {Op::MoveForward,   Op::MoveRight,  Op::TurnBy,       Op::GoTo,        Op::GoToObject,
                                 Op::PointTowards,  Op::Show,       Op::Hide,         Op::SetColor,    Op::SetSize,
                                 Op::ChangeSizeBy,  Op::Say,        Op::SetVariable,  Op::ChangeVariable,
                                 Op::ResetTimer,    Op::Broadcast};
    std::vector<Block> blocks = {MakeBlock(Op::WhenPlayStarts, 1)};
    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        blocks.push_back(MakeBlock(ops[i], static_cast<std::uint32_t>(2 + i)));
    }
    Run(blocks);
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_TRUE(runner_.IsPaused());

    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        const std::uint32_t ran = static_cast<std::uint32_t>(2 + i);
        SCOPED_TRACE("block " + std::to_string(ran));
        ASSERT_EQ(runner_.Debug().pausedBlock, ran);
        Step();
        const ScriptHighlights found = Collect({At()});
        EXPECT_EQ(found.current, (std::vector<std::uint32_t>{ran}));
        if (i + 1 < ops.size())
        {
            EXPECT_EQ(found.next, (std::vector<std::uint32_t>{ran + 1}));
        }
        else
        {
            EXPECT_TRUE(found.next.empty());
        }
        for (std::uint32_t earlier = 1; earlier < ran; ++earlier)
        {
            EXPECT_EQ(HighlightOf(found, earlier), Highlight::Previous) << "block " << earlier;
        }
    }
}

// While the game runs: a wait in progress glows, with the block after it "ready"; a pause freezes it; the end block
// glows in the frame the script ended and is faded from the next; an edit starts the script over from nowhere
TEST_F(LogicDebugTest, HighlightWhileTheGameRuns)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0}), Make(Op::Wait, 3, {1}),
         Make(Op::MoveBy, 4, {1, 0, 0})});
    Update();
    EXPECT_EQ(At(), (State{.script = 0, .running = true, .last = 3, .next = 4, .before = {1, 2}}));

    runner_.Pause(1);
    const State paused = At();
    for (int i = 0; i < 100; ++i)
    {
        Update(1.0);
    }
    EXPECT_EQ(At(), paused);
    runner_.Resume();

    Update(1.0); // the wait is over: the last block runs and the script ends
    EXPECT_EQ(At(), (State{.script = 0, .running = false, .last = 4, .before = {1, 2, 3}, .justEnded = true}));
    Update();
    EXPECT_EQ(At(), (State{.script = 0, .running = false, .last = 4, .before = {1, 2, 3}}));

    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0})}); // edited: new scripts
    EXPECT_EQ(At(), State{});
}

// A loop goes round once per frame and its loop block runs every round, like a while loop's check: with the wait last
// in the loop, the loop block is "ready". From the second round every block is in the trail, so whatever does not
// glow and is not next stays faded
TEST_F(LogicDebugTest, ALoopKeepsItsBlocksFaded)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1),
         Make(Op::Forever, 2, {}, {Make(Op::MoveBy, 3, {1, 0, 0}), Make(Op::Wait, 4, {1})})});
    Update();
    EXPECT_EQ(At(), (State{.script = 0, .running = true, .last = 4, .next = 2, .before = {1, 2, 3}}));
    Update(1.0); // the wait is over: the round ends, the loop block is next
    EXPECT_EQ(At(), (State{.script = 0, .running = true, .last = 4, .next = 2, .before = {1, 2, 3}}));
    Update();
    EXPECT_EQ(At(), (State{.script = 0, .running = true, .last = 4, .next = 2, .before = {1, 2, 3, 4}}));
}

// An object runs several scripts at once, each somewhere of its own: here both are on a wait at the end of their script
TEST_F(LogicDebugTest, EveryScriptHasItsOwnPlace)
{
    LogicDesc logic{.scripts = {Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0}),
                                                  Make(Op::Wait, 3, {5})}},
                                Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 4), Make(Op::TurnBy, 5, {5}),
                                                  Make(Op::Wait, 6, {5})}}}};
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    Update();
    EXPECT_EQ(At(0), (State{.script = 0, .running = true, .last = 3, .next = 0, .before = {1, 2}}));
    EXPECT_EQ(At(1), (State{.script = 1, .running = true, .last = 6, .next = 0, .before = {4, 5}}));
}

// While the game plays only what is running has its colour: the block, and what it runs inside of
TEST(ExecutionHighlightTest, OnlyWhatIsRunningKeepsItsColour)
{
    using Editor::Panels::ExecutionHighlight::RunningBlocks;
    using Ids = std::vector<std::uint32_t>;

    // when Play starts (1) > forever (2) { if (3) { move (4) } else { turn (5) } } ; when key pressed (6) > say (7)
    Block choice{.id = 3, .op = Op::IfElse};
    choice.body = {Block{.id = 4, .op = Op::MoveForward}};
    choice.elseBody = {Block{.id = 5, .op = Op::TurnBy}};
    Block loop{.id = 2, .op = Op::Forever};
    loop.body = {choice};
    LogicDesc logic;
    logic.scripts = {{.blocks = {Block{.id = 1, .op = Op::WhenPlayStarts}, loop}},
                     {.blocks = {Block{.id = 6, .op = Op::WhenKeyPressed}, Block{.id = 7, .op = Op::Say}}}};

    EXPECT_EQ(RunningBlocks(logic, {}, {}), Ids{}) << "nothing running: everything grey";
    EXPECT_EQ(RunningBlocks(logic, {4}, {}), (Ids{1, 2, 3, 4})) << "the block, what holds it, and its hat";
    EXPECT_EQ(RunningBlocks(logic, {5}, {}), (Ids{1, 2, 3, 5})) << "the else arm too";
    EXPECT_EQ(RunningBlocks(logic, {7}, {}), (Ids{6, 7})) << "the other script stays grey";

    // The block a paused script stopped on counts, though no time has passed for it to be active
    EXPECT_EQ(RunningBlocks(logic, {}, ScriptHighlights{.current = {7}}), (Ids{6, 7}));
    // Ran before or runs next is not running
    EXPECT_EQ(RunningBlocks(logic, {}, ScriptHighlights{.previous = {4}, .next = {5}}), Ids{});
    EXPECT_EQ(RunningBlocks(logic, {7}, ScriptHighlights{.current = {7}}), (Ids{6, 7})) << "listed once";
}

// Which look each block gets from where the scripts are
TEST(ExecutionHighlightTest, WhichLookEachBlockGets)
{
    // Running: the block it ran glows, the ones before it are faded, the one it runs next is "ready"
    ScriptHighlights found = Collect({State{.script = 0, .running = true, .last = 5, .next = 6, .before = {3, 4}}});
    EXPECT_EQ(HighlightOf(found, 5), Highlight::Current);
    EXPECT_EQ(HighlightOf(found, 4), Highlight::Previous);
    EXPECT_EQ(HighlightOf(found, 3), Highlight::Previous);
    EXPECT_EQ(HighlightOf(found, 6), Highlight::Next);
    EXPECT_EQ(HighlightOf(found, 7), Highlight::None);

    // Just ended: its last block still glows, nothing is next; after that it is faded too
    found = Collect({State{.running = false, .last = 5, .before = {3, 4}, .justEnded = true}});
    EXPECT_EQ(found.current, (std::vector<std::uint32_t>{5}));
    EXPECT_EQ(found.previous, (std::vector<std::uint32_t>{3, 4}));
    EXPECT_TRUE(found.next.empty());
    found = Collect({State{.running = false, .last = 5, .before = {3, 4}}});
    EXPECT_TRUE(found.current.empty());
    EXPECT_EQ(found.previous, (std::vector<std::uint32_t>{3, 4, 5}));

    // Not started: nothing. A block is not its own previous or next
    found = Collect({State{}, State{.running = true, .last = 3, .next = 3, .before = {3}}});
    EXPECT_EQ(found.current, (std::vector<std::uint32_t>{3}));
    EXPECT_TRUE(found.previous.empty());
    EXPECT_TRUE(found.next.empty());

    // Scripts that share a block: glow beats "ready" beats faded
    found = Collect({State{.script = 0, .running = true, .last = 5, .next = 6, .before = {4}},
                     State{.script = 1, .running = true, .last = 6, .next = 7, .before = {5}},
                     State{.script = 2, .running = true, .last = 8, .next = 4, .before = {7}}});
    EXPECT_EQ(HighlightOf(found, 5), Highlight::Current);
    EXPECT_EQ(HighlightOf(found, 6), Highlight::Current);
    EXPECT_EQ(HighlightOf(found, 4), Highlight::Next);
    EXPECT_EQ(HighlightOf(found, 7), Highlight::Next);
}

// Step Over runs a whole My Block as one step; on any other block it is a Step
TEST_F(LogicDebugTest, StepOverRunsAMyBlockInOneStep)
{
    Engine::Logic::Library library;
    library.blocks.push_back(
        {.name = "Two Moves", .blocks = {Make(Op::MoveBy, 1, {1, 0, 0}), Make(Op::MoveBy, 2, {1, 0, 0})}});
    runner_.SetLibrary(library);
    Run({MakeBlock(Op::WhenPlayStarts, 1), Engine::Logic::MakeCall("Two Moves", 7), Make(Op::MoveBy, 8, {1, 0, 0}),
         Make(Op::MoveBy, 9, {1, 0, 0})});
    runner_.ToggleBreakpoint(1, 7);
    Update();
    ASSERT_TRUE(runner_.IsPaused());

    runner_.StepOver();
    Update();
    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_NEAR(X(), 2.0f, kTolerance) << "both of its moves ran";
    EXPECT_EQ(runner_.Debug().pausedBlock, 8u);

    runner_.StepOver();
    Update();
    EXPECT_NEAR(X(), 3.0f, kTolerance) << "one block, as Step does";
    EXPECT_EQ(runner_.Debug().pausedBlock, 9u);
}

// Where breakpoints are kept for the next session
TEST(BreakpointFile, SavedBreakpointsComeBack)
{
    const auto path = std::filesystem::temp_directory_path() / "atelier_breakpoint_tests" / "scene.txt";
    std::filesystem::remove_all(path.parent_path());
    const Engine::Logic::Breakpoints saved{{1, {7, 9}}, {4, {2}}};

    Engine::Logic::SaveBreakpoints(path, saved);
    EXPECT_EQ(Engine::Logic::LoadBreakpoints(path), saved);

    Engine::Logic::SaveBreakpoints(path, {});
    EXPECT_FALSE(std::filesystem::exists(path)) << "none left: no file";
    EXPECT_TRUE(Engine::Logic::LoadBreakpoints(path).empty());
    std::filesystem::remove_all(path.parent_path());
}

TEST(DebugLinesShapes, AnArrowIsItsLineAndAHead)
{
    DebugLines lines;
    lines.Arrow(glm::vec3(0.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec4(1.0f));
    ASSERT_EQ(lines.Vertices().size(), 6u);
    EXPECT_EQ(lines.Vertices()[1].position, glm::vec3(0.0f, 2.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[2].position, glm::vec3(0.0f, 2.0f, 0.0f)) << "the head starts at the tip";
    EXPECT_LT(lines.Vertices()[3].position.y, 2.0f);

    DebugLines nothing;
    nothing.Arrow(glm::vec3(1.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    EXPECT_EQ(nothing.Vertices().size(), 2u) << "no length, no direction for a head";
}

TEST(DebugLinesShapes, ContactsAreDrawnOnlyWhenAskedFor)
{
    Engine::Scene::Scene ecs;
    Engine::Assets::ResourceManager resources;
    const std::vector<Engine::Systems::ContactMark> contacts{{glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)}};

    DebugLines off;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{}, {}, {}, off, contacts);
    EXPECT_TRUE(off.Vertices().empty());

    DebugLines on;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.contacts = true}, {}, {}, on, contacts);
    ASSERT_FALSE(on.Vertices().empty());
    EXPECT_EQ(on.Vertices().back().color, DebugLineSystem::kContactColor);
}

// ---------------------------------------------------------------- Breakpoint rules

TEST(BreakpointCondition, ConditionsAreRead)
{
    using Engine::Logic::ParseBreakpointCondition;
    const auto above = ParseBreakpointCondition("Speed > 5");
    ASSERT_TRUE(above.has_value());
    EXPECT_EQ(above->variable, "Speed");
    EXPECT_EQ(above->comparison, ">");
    EXPECT_EQ(above->value, 5.0);
    EXPECT_TRUE(above->Holds(6.0));
    EXPECT_FALSE(above->Holds(5.0));

    EXPECT_TRUE(ParseBreakpointCondition("Top Score>=10")->Holds(10.0)) << "names can have spaces";
    EXPECT_EQ(ParseBreakpointCondition("Top Score>=10")->variable, "Top Score");
    EXPECT_TRUE(ParseBreakpointCondition("Lives = 0")->Holds(0.0)) << "= reads as ==";
    EXPECT_TRUE(ParseBreakpointCondition("Alive != true")->Holds(0.0));
    EXPECT_TRUE(ParseBreakpointCondition("Height < -1.5")->Holds(-2.0));

    for (const char* bad : {"", "Speed", "> 5", "Speed >", "Speed > fast", "Speed > 5 km"})
    {
        EXPECT_FALSE(ParseBreakpointCondition(bad).has_value()) << bad;
    }
}

TEST_F(LogicDebugTest, ABreakpointSwitchedOffDoesNotStop)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    runner_.SetBreakpointRule(1, 2, {.enabled = false});
    Update();
    EXPECT_FALSE(runner_.IsPaused());
    EXPECT_NEAR(X(), 1.0f, kTolerance);
}

TEST_F(LogicDebugTest, AConditionStopsOnlyWhileItHolds)
{
    Block set = Make(Op::SetVariable, 2, {3});
    set.text = "Speed";
    Block raise = Make(Op::SetVariable, 4, {9});
    raise.text = "Speed";
    auto logic = OneScript(
        {MakeBlock(Op::WhenPlayStarts, 1), set, Make(Op::MoveBy, 3, {1, 0, 0}), raise, Make(Op::MoveBy, 5, {1, 0, 0})});
    logic.variables.push_back({.name = "Speed"});
    runner_.Set(1, "Cube", entity_.GetId(), logic);
    runner_.ToggleBreakpoint(1, 3);
    runner_.ToggleBreakpoint(1, 5);
    runner_.SetBreakpointRule(1, 3, {.condition = "Speed > 5"});
    runner_.SetBreakpointRule(1, 5, {.condition = "Speed > 5"});

    Update();

    EXPECT_TRUE(runner_.IsPaused());
    EXPECT_EQ(runner_.Debug().pausedBlock, 5u) << "Speed was 3 at the first one";
    EXPECT_NEAR(X(), 1.0f, kTolerance);
}

TEST_F(LogicDebugTest, AConditionThatCannotBeWorkedOutStillStops)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveBy, 2, {1, 0, 0})});
    runner_.ToggleBreakpoint(1, 2);
    runner_.SetBreakpointRule(1, 2, {.condition = "Missing > 5"});
    Update();
    EXPECT_TRUE(runner_.IsPaused());
}

TEST(BreakpointFile, RulesAreSavedWithTheirBreakpoints)
{
    const auto path = std::filesystem::temp_directory_path() / "atelier_breakpoint_tests" / "rules.txt";
    std::filesystem::remove_all(path.parent_path());
    const Engine::Logic::Breakpoints saved{{1, {7, 9}}};
    const Engine::Logic::BreakpointRules rules{{{1, 7}, {.enabled = false}}, {{1, 9}, {.condition = "Top Score > 5"}}};

    Engine::Logic::SaveBreakpoints(path, saved, rules);
    Engine::Logic::BreakpointRules loaded;
    EXPECT_EQ(Engine::Logic::LoadBreakpoints(path, &loaded), saved);
    EXPECT_EQ(loaded, rules);
    std::filesystem::remove_all(path.parent_path());
}

TEST(BreakpointFile, FilesOfOlderBuildsStillLoad)
{
    const auto path = std::filesystem::temp_directory_path() / "atelier_breakpoint_tests" / "old.txt";
    std::filesystem::remove_all(path.parent_path());
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << "1 7\n1 9\n";

    Engine::Logic::BreakpointRules rules;
    EXPECT_EQ(Engine::Logic::LoadBreakpoints(path, &rules), (Engine::Logic::Breakpoints{{1, {7, 9}}}));
    EXPECT_TRUE(rules.empty());
    std::filesystem::remove_all(path.parent_path());
}

// ---------------------------------------------------------------- Forces and layers

TEST(DebugLinesShapes, ForcesAreArrowsOfACappedLength)
{
    Engine::Scene::Scene ecs;
    Engine::Assets::ResourceManager resources;
    const std::vector<Engine::Systems::ForceMark> forces{{glm::vec3(0.0f), glm::vec3(1000.0f, 0.0f, 0.0f)},
                                                         {glm::vec3(0.0f), glm::vec3(0.0f)}};
    DebugLines lines;
    DebugLineSystem::Collect(ecs, resources, DebugLineSettings{.forces = true}, {}, {}, lines, {}, forces);

    ASSERT_EQ(lines.Vertices().size(), 6u) << "one arrow: no push, no arrow";
    EXPECT_EQ(lines.Vertices()[1].position, glm::vec3(DebugLineSystem::kMaxForceLength, 0.0f, 0.0f));
    EXPECT_EQ(lines.Vertices()[0].color, DebugLineSystem::kForceColor);
}

TEST(DebugLinesShapes, CollidersCanBeColouredByLayer)
{
    Engine::Systems::ColliderOutline first;
    first.layers = 1u;
    first.touching = true;
    Engine::Systems::ColliderOutline second = first;
    second.layers = 2u;
    EXPECT_NE(DebugLineSystem::ColliderColorFor(first, true), DebugLineSystem::ColliderColorFor(second, true));
    EXPECT_EQ(DebugLineSystem::ColliderColorFor(first), DebugLineSystem::kDynamicColor) << "by body type otherwise";
    EXPECT_EQ(DebugLineSystem::LayerColor(6u), DebugLineSystem::LayerColor(2u)) << "its lowest layer decides";
}
