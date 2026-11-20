#include "editor/panels/blocks/BlockPreview.hpp"
#include "editor/panels/blocks/ScriptPreview.hpp"

#include <gtest/gtest.h>

#include <deque>

// The block previews (Editor::Panels::PoseAt): Atti acts each block out with the block's own numbers, from a still
// start to a still end

using Editor::Panels::PoseAt;
using Editor::Panels::PreviewStage;
using Editor::Panels::StageOf;
using Engine::Logic::Block;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;

namespace
{
    constexpr float kTolerance = 1e-4f;
    constexpr float kEnd = Editor::Panels::kPreviewStart + Editor::Panels::kPreviewAction;

    Block With(Op op, std::initializer_list<float> numbers)
    {
        Block block = MakeBlock(op, 1);
        std::size_t i = 0;
        for (const float number : numbers)
        {
            block.numbers[i++] = number;
        }
        return block;
    }
}

// Every kind of block has a preview, except a saved block (its tooltip lists the blocks it runs)
TEST(BlockPreview, EveryBlockHasOne)
{
    for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
    {
        EXPECT_EQ(StageOf(info.op) == PreviewStage::None, info.op == Op::Call) << info.name;
    }
    EXPECT_EQ(StageOf(Op::MoveForward), PreviewStage::Top);
    EXPECT_EQ(StageOf(Op::Repeat), PreviewStage::Side);
    EXPECT_EQ(StageOf(Op::ShowScreenText), PreviewStage::Screen);
    EXPECT_EQ(StageOf(Op::Add), PreviewStage::Value);
}

// Sums and comparisons are worked out with the block's numbers, as the game does (dividing by 0 gives 0)
TEST(BlockPreview, SumsAndConditionsAreWorkedOut)
{
    EXPECT_EQ(PoseAt(With(Op::Add, {3.0f, 4.0f}), 0.0f).readout, "3 + 4");
    EXPECT_EQ(PoseAt(With(Op::Add, {3.0f, 4.0f}), kEnd).readout, "3 + 4 = 7");
    EXPECT_EQ(PoseAt(With(Op::Divide, {1.0f, 3.0f}), kEnd).readout, "1 / 3 = 0.33");
    EXPECT_EQ(PoseAt(With(Op::Mod, {-7.0f, 3.0f}), kEnd).readout, "-7 mod 3 = 2");
    EXPECT_NE(PoseAt(With(Op::Divide, {1.0f, 0.0f}), kEnd).readout.find("= 0"), std::string::npos);

    EXPECT_FALSE(PoseAt(With(Op::Less, {2.0f, 5.0f}), 0.0f).answer.has_value()) << "the answer comes after";
    EXPECT_EQ(PoseAt(With(Op::Less, {2.0f, 5.0f}), kEnd).answer, true);
    EXPECT_EQ(PoseAt(With(Op::Greater, {2.0f, 5.0f}), kEnd).answer, false);
    // "and" goes through every pair: only true and true is true
    EXPECT_EQ(PoseAt(MakeBlock(Op::And, 1), 0.1f).answer, true);
    EXPECT_EQ(PoseAt(MakeBlock(Op::And, 1), 0.4f).answer, false);
    EXPECT_EQ(PoseAt(MakeBlock(Op::Or, 1), 0.4f).answer, true);
    EXPECT_EQ(PoseAt(MakeBlock(Op::Or, 1), 0.9f).answer, false);
}

// A value block in a sum is shown by its words, and the answer is not made up: it is only known in Play
TEST(BlockPreview, AValueBlockInASumIsNamedNotGuessed)
{
    Block less = With(Op::Less, {0.0f, 3.5f});
    less.inputs[0] = MakeBlock(Op::DistanceTo, 7);
    less.inputs[0].text = "Player";
    EXPECT_EQ(PoseAt(less, kEnd).readout, "(distance to \"Player\") < 3.5");
    EXPECT_FALSE(PoseAt(less, kEnd).answer.has_value());

    Block add = With(Op::Add, {0.0f, 1.0f});
    add.inputs[0] = MakeBlock(Op::Timer, 8);
    EXPECT_EQ(PoseAt(add, kEnd).readout, "(timer) + 1 = ?");
}

// "pick random" between whole numbers settles on a whole number in range
TEST(BlockPreview, RandomStaysInRange)
{
    for (const float t : {0.0f, 0.2f, 0.4f, 0.9f})
    {
        const std::string readout = PoseAt(With(Op::Random, {1.0f, 10.0f}), t).readout;
        const double picked = std::stod(readout.substr(readout.rfind('=') + 1));
        EXPECT_GE(picked, 1.0);
        EXPECT_LE(picked, 10.0);
        EXPECT_EQ(picked, std::floor(picked)) << readout;
    }
}

// "repeat 10" counts all the way to 10; "wait" fills its bar; an if runs its inside only while true
TEST(BlockPreview, LoopsAndConditionsShowTheirSteps)
{
    EXPECT_EQ(PoseAt(With(Op::Repeat, {10.0f}), 0.0f).readout, "0 / 10");
    EXPECT_EQ(PoseAt(With(Op::Repeat, {10.0f}), kEnd).readout, "10 / 10");
    EXPECT_NEAR(PoseAt(With(Op::Wait, {1.0f}), kEnd).progress, 1.0f, kTolerance);
    EXPECT_EQ(PoseAt(MakeBlock(Op::If, 1), 0.3f).answer, true);
    EXPECT_GT(PoseAt(MakeBlock(Op::If, 1), 0.3f).y, 0.0f) << "true: the inside runs";
    EXPECT_EQ(PoseAt(MakeBlock(Op::If, 1), 0.8f).answer, false);
    EXPECT_NEAR(PoseAt(MakeBlock(Op::If, 1), 0.8f).y, 0.0f, kTolerance) << "false: skipped";
    EXPECT_EQ(PoseAt(MakeBlock(Op::OnGround, 1), 0.45f).answer, false) << "in the air, mid-jump";
}

// Example numbers stand in for a palette block of zeros only: a block with numbers keeps them
TEST(BlockPreview, ExamplesOnlyReplaceZeros)
{
    EXPECT_EQ(PoseAt(Editor::Panels::WithExamples(MakeBlock(Op::Add, 1)), kEnd).readout, "3 + 4 = 7");
    EXPECT_EQ(PoseAt(Editor::Panels::WithExamples(With(Op::Add, {1.0f, 0.0f})), kEnd).readout, "1 + 0 = 1");
    EXPECT_EQ(PoseAt(MakeBlock(Op::Add, 1), kEnd).readout, "0 + 0 = 0") << "on the canvas, as typed";
}

// A jump leaves the ground, is at its height halfway and lands again; a higher jump goes higher
TEST(BlockPreview, AJumpGoesAsHighAsTheBlockSays)
{
    const Block jump = With(Op::Jump, {2.0f});
    EXPECT_NEAR(PoseAt(jump, 0.0f).y, 0.0f, kTolerance);
    EXPECT_NEAR(PoseAt(jump, Editor::Panels::kPreviewStart + Editor::Panels::kPreviewAction * 0.5f).y, 2.0f,
                kTolerance);
    EXPECT_NEAR(PoseAt(jump, kEnd).y, 0.0f, kTolerance);
    EXPECT_GT(PoseAt(With(Op::Jump, {5.0f}), 0.45f).y, PoseAt(jump, 0.45f).y);
    EXPECT_NEAR(PoseAt(With(Op::Jump, {-1.0f}), 0.45f).y, 0.0f, kTolerance) << "no jumping into the ground";
}

// Moving and turning end where the block says: forward is up the screen (-Z), and positive turns go left, as in Play
TEST(BlockPreview, MovesAndTurnsEndWhereTheBlockSays)
{
    EXPECT_NEAR(PoseAt(With(Op::MoveForward, {3.0f}), kEnd).y, -3.0f, kTolerance);
    EXPECT_NEAR(PoseAt(With(Op::MoveRight, {2.0f}), kEnd).x, 2.0f, kTolerance);
    EXPECT_NEAR(PoseAt(With(Op::TurnBy, {90.0f}), kEnd).facing, 90.0f, kTolerance);
    EXPECT_NEAR(PoseAt(With(Op::TurnBy, {90.0f}), 0.0f).facing, 0.0f, kTolerance);

    const auto glide = PoseAt(With(Op::GlideTo, {1.0f, 2.0f, 0.0f, -1.0f}), kEnd);
    EXPECT_NEAR(glide.x, 2.0f, kTolerance);
    EXPECT_NEAR(glide.y, -1.0f, kTolerance);
    ASSERT_TRUE(glide.target.has_value());
}

// A number filled by a value block is only known in Play: the preview uses the palette's default instead
TEST(BlockPreview, AComputedNumberUsesTheDefault)
{
    Block move = With(Op::MoveForward, {4.0f});
    move.inputs[0] = MakeBlock(Op::Random, 7);
    EXPECT_NEAR(PoseAt(move, kEnd).y, -1.0f, kTolerance);
}

TEST(BlockPreview, LooksChangeAsTheBlockSays)
{
    EXPECT_NEAR(PoseAt(With(Op::Hide, {}), kEnd).alpha, 0.15f, kTolerance) << "hidden, but still there";
    EXPECT_NEAR(PoseAt(With(Op::SetSize, {2.0f}), kEnd).size, 2.0f, kTolerance);
    EXPECT_FALSE(PoseAt(With(Op::Say, {}), 0.0f).speaking);
    EXPECT_TRUE(PoseAt(With(Op::Say, {}), kEnd).speaking);
}

namespace
{
    using Editor::Panels::BuildScriptPreview;
    using Editor::Panels::FrameAt;
    using Editor::Panels::ScriptFrame;
    using Editor::Panels::ScriptPreview;
    using Engine::Logic::Variable;

    Block Named(Op op, const std::string& text, std::initializer_list<float> numbers = {})
    {
        Block block = With(op, numbers);
        block.text = text;
        return block;
    }

    Block Holding(Block block, std::vector<Block> body)
    {
        block.body = std::move(body);
        return block;
    }

    Block Asking(Op op, Block condition)
    {
        Block block = MakeBlock(op, 1);
        condition.id = 2;
        block.slots = {std::move(condition)};
        return block;
    }

    // A comparison of a variable with a number
    Block Compare(Op op, const std::string& variable, float number)
    {
        Block compare = With(op, {0.0f, number});
        compare.inputs[0] = Named(Op::VariableValue, variable);
        compare.inputs[0].id = 3;
        return compare;
    }

    // A script's steps point at its blocks, as the canvas keeps them: these keep them for the tests' temporaries
    template <typename T> const T& Kept(T value)
    {
        static std::deque<T> kept;
        return kept.emplace_back(std::move(value));
    }

    ScriptPreview Hat(const Block& hat, std::vector<Block> under, const std::vector<Variable>& variables = {},
                      const Engine::Logic::Library* library = nullptr)
    {
        return BuildScriptPreview(Kept(hat), {}, Kept(std::move(under)), variables, library);
    }

    ScriptPreview Alone(const Block& block, const std::vector<Variable>& variables = {})
    {
        return BuildScriptPreview(Kept(block), {}, {}, variables, nullptr);
    }

    // How it ends
    ScriptFrame End(const ScriptPreview& script)
    {
        return FrameAt(script, script.seconds);
    }

    // The labels of a script's outcomes, in order
    std::vector<std::string> Outcomes(const ScriptPreview& script)
    {
        std::vector<std::string> labels;
        for (const Editor::Panels::ScriptStep& step : script.steps)
        {
            if (labels.empty() || labels.back() != step.outcome)
            {
                labels.push_back(step.outcome);
            }
        }
        return labels;
    }

    // The highest the object gets while the script plays
    float HighestLift(const ScriptPreview& script)
    {
        float highest = 0.0f;
        for (double t = 0.0; t < script.seconds; t += 0.02)
        {
            highest = std::max(highest, FrameAt(script, t).lift);
        }
        return highest;
    }
}

// The bug this guards: every hat and loop hopped, whatever was under it
TEST(ScriptPreview, AScriptShowsItsOwnBlocksNotAHop)
{
    const Block hat = MakeBlock(Op::WhenPlayStarts, 1);
    const ScriptPreview move = Hat(hat, {With(Op::MoveForward, {2.0f})});
    EXPECT_NEAR(End(move).actor.z, -2.0f, kTolerance);
    EXPECT_NEAR(HighestLift(move), 0.0f, kTolerance) << "it walks, so nothing jumps";

    EXPECT_GT(HighestLift(Hat(hat, {With(Op::Jump, {1.0f})})), 0.5f) << "a jump is still a jump";

    const ScriptPreview loop = Alone(Holding(With(Op::Repeat, {3.0f}), {With(Op::TurnBy, {30.0f})}));
    EXPECT_NEAR(End(loop).actor.facing, 90.0f, kTolerance);
    EXPECT_NEAR(HighestLift(loop), 0.0f, kTolerance);
}

// Nothing touches the object: it is not shown, and the variable is
TEST(ScriptPreview, OnlyVariablesShowsTheVariablesNotTheObject)
{
    const Block hat = Named(Op::WhenKeyPressed, "Space");
    const std::vector<Variable> variables{{.name = "score", .value = 4.0f}};
    const ScriptPreview script = Hat(hat, {Named(Op::ChangeVariable, "score", {1.0f})}, variables);

    EXPECT_FALSE(script.actsOnObject);
    EXPECT_NEAR(HighestLift(script), 0.0f, kTolerance);
    ASSERT_EQ(script.steps.size(), 2u);
    const float changing = Editor::Panels::kScriptTrigger;
    EXPECT_EQ(FrameAt(script, 0.0).variables, std::vector<std::string>{"score = 4"}) << "there from the start";
    EXPECT_EQ(FrameAt(script, changing).variables, std::vector<std::string>{"score = 4"}) << "before it changes";
    EXPECT_EQ(End(script).variables, std::vector<std::string>{"score = 5"});
    EXPECT_EQ(FrameAt(script, 0.0).pose.key, "Space") << "what starts it is still shown";
    EXPECT_TRUE(FrameAt(script, 0.5).pose.keyDown);

    const ScriptPreview both = Hat(hat, {Named(Op::SetVariable, "score", {1.0f}), With(Op::MoveForward, {1.0f})});
    EXPECT_TRUE(both.actsOnObject) << "one block that moves it is enough";
    EXPECT_EQ(End(both).variables, std::vector<std::string>{"score = 1"}) << "and the variable shows beside it";
}

// A loop's rounds add up, however many are skipped; a value only the game knows is not made up
TEST(ScriptPreview, VariablesAreWorkedOutAsTheGameDoes)
{
    const std::vector<Variable> variables{{.name = "score", .value = 0.0f}};
    const Block change = Named(Op::ChangeVariable, "score", {2.0f});
    EXPECT_EQ(End(Alone(Holding(With(Op::Repeat, {100.0f}), {change}), variables)).variables,
              std::vector<std::string>{"score = 200"});

    Block sum = Named(Op::SetVariable, "score");
    sum.inputs[0] = With(Op::Add, {0.0f, 3.0f});
    sum.inputs[0].id = 5;
    sum.inputs[0].inputs[0] = Named(Op::VariableValue, "score");
    sum.inputs[0].inputs[0].id = 6;
    EXPECT_EQ(End(Hat(MakeBlock(Op::WhenPlayStarts, 1), {sum}, {{.name = "score", .value = 4.0f}})).variables,
              std::vector<std::string>{"score = 7"});

    Block random = Named(Op::SetVariable, "score");
    random.inputs[0] = With(Op::Random, {1.0f, 6.0f});
    random.inputs[0].id = 5;
    EXPECT_EQ(End(Hat(MakeBlock(Op::WhenPlayStarts, 1), {random}, variables)).variables,
              std::vector<std::string>{"score = ?"});
}

// Moves follow the way the object faces, so a loop can draw a square and come back
TEST(ScriptPreview, TurnsChangeWhereMovesGo)
{
    const ScriptPreview square = Alone(
        Holding(With(Op::Repeat, {4.0f}), {With(Op::MoveForward, {2.0f}), With(Op::TurnBy, {90.0f})}));
    EXPECT_NEAR(End(square).actor.x, 0.0f, kTolerance);
    EXPECT_NEAR(End(square).actor.z, 0.0f, kTolerance);
    EXPECT_NEAR(End(square).actor.facing, 360.0f, kTolerance);

    const ScriptPreview left =
        Hat(MakeBlock(Op::WhenPlayStarts, 1), {With(Op::TurnBy, {90.0f}), With(Op::MoveForward, {3.0f})});
    EXPECT_NEAR(End(left).actor.x, -3.0f, kTolerance) << "direction 90 faces -X";
    EXPECT_NEAR(End(left).actor.z, 0.0f, kTolerance);
}

// The bug this guards: an if on a variable showed only what the variable's start made it, usually false
TEST(ScriptPreview, AnIfShowsBothAnswers)
{
    Block choice = Asking(Op::IfElse, Compare(Op::Greater, "score", 5.0f));
    choice.body = {With(Op::MoveForward, {1.0f})};
    choice.elseBody = {With(Op::MoveRight, {1.0f})};
    const std::vector<Variable> variables{{.name = "score", .value = 2.0f}};

    // The mouse on it: both, whatever anything says, with true first
    const ScriptPreview hovered = Alone(choice, variables);
    ASSERT_EQ(Outcomes(hovered).size(), 2u);
    EXPECT_EQ(FrameAt(hovered, 0.0).pose.answer, true);
    EXPECT_NEAR(End(hovered).actor.x, 1.0f, kTolerance);
    EXPECT_NEAR(End(hovered).actor.z, 0.0f, kTolerance);

    Block literal = Asking(Op::If, With(Op::Less, {1.0f, 0.0f}));
    literal.body = {With(Op::TurnBy, {90.0f})};
    EXPECT_EQ(Outcomes(Alone(literal)).size(), 2u) << "even an if on plain numbers";

    // In a script: a variable's start proves nothing, as other scripts change it
    const Block hat = MakeBlock(Op::WhenPlayStarts, 1);
    EXPECT_EQ(Outcomes(Hat(hat, {choice}, variables)).size(), 2u);

    // But what the script itself just set is known: one outcome, the arm the game takes
    const ScriptPreview high = Hat(hat, {Named(Op::SetVariable, "score", {9.0f}), choice}, variables);
    EXPECT_EQ(Outcomes(high).size(), 1u);
    EXPECT_NEAR(End(high).actor.z, -1.0f, kTolerance);
    const ScriptPreview low = Hat(hat, {Named(Op::SetVariable, "score", {1.0f}), choice}, variables);
    EXPECT_EQ(Outcomes(low).size(), 1u);
    EXPECT_NEAR(End(low).actor.x, 1.0f, kTolerance);
}

// An answer that makes no difference is not played as an outcome of its own
TEST(ScriptPreview, OutcomesThatComeOutTheSameArePlayedOnce)
{
    Block until = Asking(Op::RepeatUntil, Compare(Op::Greater, "n", 2.0f));
    until.body = {Named(Op::ChangeVariable, "n", {1.0f})};
    EXPECT_EQ(Outcomes(Alone(until, {{.name = "n", .value = 0.0f}})).size(), 1u);
}

// A model really plays its clips: going round through what comes next, once, and stopped
TEST(ScriptPreview, AnimationsKeepPlaying)
{
    const Block hat = MakeBlock(Op::WhenPlayStarts, 1);
    const Block walk = Named(Op::PlayAnimation, "Walk");

    const ScriptPreview going = Hat(hat, {walk, With(Op::MoveForward, {2.0f})});
    EXPECT_EQ(End(going).pose.clip, "Walk") << "still walking while it moves";
    EXPECT_GT(FrameAt(going, 2.0).pose.clipTime, FrameAt(going, 1.5).pose.clipTime);

    EXPECT_EQ(End(Hat(hat, {walk, MakeBlock(Op::StopAnimation, 3)})).pose.clip, "") << "back to what it started with";

    Block fast = With(Op::SetAnimationSpeed, {2.0f});
    EXPECT_NEAR(FrameAt(Hat(hat, {fast, walk}), 2.0).pose.clipTime, 4.0f, kTolerance);

    const ScriptPreview once = Hat(hat, {Named(Op::PlayAnimationOnce, "Wave")});
    const double middle = Editor::Panels::kScriptTrigger + Editor::Panels::kScriptAction;
    EXPECT_EQ(FrameAt(once, middle).pose.clip, "Wave");
    EXPECT_NEAR(FrameAt(once, middle).pose.clipFraction, 0.5f, 0.01f);
}

// What only the game can answer goes both ways, as outcomes played in turn, each starting afresh
TEST(ScriptPreview, EveryOutcomeIsPlayed)
{
    Block touching = Asking(Op::IfElse, Named(Op::Touching, "Enemy"));
    touching.body = {With(Op::MoveForward, {1.0f})};
    touching.elseBody = {With(Op::MoveRight, {1.0f})};

    for (const ScriptPreview& script : {Alone(touching), Hat(MakeBlock(Op::WhenPlayStarts, 1), {touching})})
    {
        ASSERT_EQ(Outcomes(script).size(), 2u);
        EXPECT_NE(FrameAt(script, 0.0).outcome.find("Outcome 1 of 2"), std::string::npos);
        EXPECT_NE(FrameAt(script, 0.0).outcome.find("= true"), std::string::npos);
        EXPECT_NE(End(script).outcome.find("Outcome 2 of 2"), std::string::npos);
        EXPECT_NE(End(script).outcome.find("= false"), std::string::npos);
        EXPECT_NEAR(End(script).actor.x, 1.0f, kTolerance);
        EXPECT_NEAR(End(script).actor.z, 0.0f, kTolerance) << "the second starts afresh: the game never runs both";
    }

    // Two unknown ifs, one after the other: all four ways
    Block first = Asking(Op::If, Named(Op::KeyDown, "A"));
    first.body = {With(Op::TurnBy, {10.0f})};
    Block second = Asking(Op::If, Named(Op::KeyDown, "B"));
    second.body = {With(Op::TurnBy, {20.0f})};
    const ScriptPreview four = Hat(MakeBlock(Op::WhenPlayStarts, 1), {first, second});
    EXPECT_EQ(Outcomes(four).size(), 4u);

    // Too many to play: it plays as many as it can, and says there are more
    std::vector<Block> many;
    for (int i = 0; i < 5; ++i)
    {
        Block each = Asking(Op::If, Named(Op::KeyDown, "K"));
        each.body = {With(Op::TurnBy, {1.0f})};
        many.push_back(each);
    }
    const ScriptPreview lots = Hat(MakeBlock(Op::WhenPlayStarts, 1), many);
    EXPECT_EQ(Outcomes(lots).size(), Editor::Panels::kScriptMaxOutcomes);
    EXPECT_NE(End(lots).outcome.find("+:"), std::string::npos);

    // What can be worked out has one outcome, and no label
    const ScriptPreview known = Hat(MakeBlock(Op::WhenPlayStarts, 1), {With(Op::MoveForward, {1.0f})});
    EXPECT_TRUE(End(known).outcome.empty());
}

// The case that used to mislead: a stop under an if only the game can answer does not end every run
TEST(ScriptPreview, AStopUnderAnUnknownIfIsOnlyOneOutcome)
{
    Block check = Asking(Op::If, Named(Op::Touching, "Enemy"));
    check.body = {MakeBlock(Op::Stop, 9)};
    const ScriptPreview script =
        Hat(MakeBlock(Op::WhenPlayStarts, 1), {Holding(MakeBlock(Op::Forever, 2), {check, With(Op::TurnBy, {10.0f})})});

    ASSERT_EQ(Outcomes(script).size(), 2u);
    EXPECT_NEAR(End(script).actor.facing, 30.0f, kTolerance) << "not touching: it keeps going round";
    EXPECT_EQ(End(script).pose.readout, "and so on, forever");
}

TEST(ScriptPreview, LoopsEnd)
{
    const std::vector<Variable> variables{{.name = "n", .value = 0.0f}};
    Block until = Asking(Op::RepeatUntil, Compare(Op::Greater, "n", 9.0f));
    until.body = {Named(Op::ChangeVariable, "n", {1.0f})};
    EXPECT_EQ(End(Alone(until, variables)).variables, std::vector<std::string>{"n = 10"});

    Block never = Asking(Op::RepeatUntil, Named(Op::KeyDown, "Space"));
    never.body = {With(Op::TurnBy, {10.0f})};
    const ScriptPreview unknown = Alone(never);
    ASSERT_EQ(Outcomes(unknown).size(), 2u) << "only the game can say: it comes true, or it never does";
    EXPECT_EQ(End(unknown).pose.readout, "never comes true: it goes on");
    EXPECT_NEAR(End(unknown).actor.facing, 30.0f, kTolerance);

    // "forever" says it goes on, and what is under it never runs
    const Block hat = MakeBlock(Op::WhenPlayStarts, 1);
    const ScriptPreview forever = Hat(
        hat, {Holding(MakeBlock(Op::Forever, 2), {With(Op::TurnBy, {10.0f})}), With(Op::MoveForward, {5.0f})});
    EXPECT_NEAR(End(forever).actor.facing, 30.0f, kTolerance);
    EXPECT_NEAR(End(forever).actor.z, 0.0f, kTolerance);

    // Stop ends the script
    const ScriptPreview stopped = Hat(hat, {MakeBlock(Op::Stop, 2), With(Op::MoveForward, {5.0f})});
    EXPECT_NEAR(End(stopped).actor.z, 0.0f, kTolerance);

    // However big, it comes to an end, and not too slowly
    const Block huge = Holding(With(Op::Repeat, {1000000.0f}),
                               {Holding(With(Op::Repeat, {1000000.0f}), {With(Op::TurnBy, {1.0f})})});
    const ScriptPreview big = Alone(huge);
    EXPECT_LE(big.steps.size(), Editor::Panels::kScriptMaxSteps);
    EXPECT_LE(big.seconds, Editor::Panels::kScriptLongest + Editor::Panels::kScriptHold + kTolerance);
}

// A long loop shows its first and last rounds and goes through the rest as one step, so nothing is lost
TEST(ScriptPreview, ALongLoopSkipsItsMiddle)
{
    const ScriptPreview script = Alone(Holding(With(Op::Repeat, {50.0f}), {With(Op::TurnBy, {1.0f})}));
    EXPECT_EQ(script.steps.size(), 4u) << "rounds 1 and 2, the rounds between, round 50";
    EXPECT_NEAR(End(script).actor.facing, 50.0f, kTolerance);
    EXPECT_NE(End(script).pose.readout.find("round 50 of 50"), std::string::npos);
    EXPECT_NE(FrameAt(script, 0.0).pose.readout.find("round 1 of 50"), std::string::npos);
}

// Nothing to run says so, rather than acting something out
TEST(ScriptPreview, NothingInsideSaysSo)
{
    const ScriptPreview bare = Hat(MakeBlock(Op::WhenPlayStarts, 1), {});
    EXPECT_FALSE(bare.actsOnObject);
    EXPECT_EQ(End(bare).pose.readout, "no blocks under it yet");

    const ScriptPreview empty = Alone(With(Op::Repeat, {3.0f}));
    EXPECT_FALSE(empty.actsOnObject);
    EXPECT_EQ(End(empty).pose.readout, "nothing inside yet");
}

// A library block runs its own blocks, with what it is given as variables
TEST(ScriptPreview, LibraryBlocksAreOpened)
{
    Engine::Logic::Library library;
    Block step = With(Op::MoveForward, {0.0f});
    step.inputs[0] = Named(Op::VariableValue, "far");
    step.inputs[0].id = 3;
    library.blocks.push_back({.name = "walk", .blocks = {step}, .inputs = {"far"}});

    Block call = Named(Op::Call, "walk", {4.0f});
    const ScriptPreview script = Hat(MakeBlock(Op::WhenPlayStarts, 1), {call}, {}, &library);
    EXPECT_NEAR(End(script).actor.z, -4.0f, kTolerance);

    // Running itself does not go on for ever
    library.blocks.push_back({.name = "again", .blocks = {Named(Op::Call, "again")}});
    EXPECT_LE(Hat(MakeBlock(Op::WhenPlayStarts, 1), {Named(Op::Call, "again")}, {}, &library).steps.size(),
              Editor::Panels::kScriptMaxSteps);
}

// Words stay readable: dark on every block's colour, and quieter but still readable on a grey block not running
TEST(BlockColours, HaveContrast)
{
    const auto luminance = [](std::uint32_t rgb)
    {
        const auto channel = [rgb](int shift)
        {
            const double c = static_cast<double>((rgb >> shift) & 0xFFu) / 255.0;
            return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel(16) + 0.7152 * channel(8) + 0.0722 * channel(0);
    };
    const auto contrast = [&](std::uint32_t a, std::uint32_t b)
    {
        const double x = luminance(a);
        const double y = luminance(b);
        return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
    };
    constexpr std::uint32_t kInk = 0x1B1B2F;       // Theme::kInk
    constexpr std::uint32_t kTextMuted = 0x5C5A72; // Theme::kTextMuted
    constexpr std::uint32_t kIdleGrey = 0xDAD8D2;
    EXPECT_GE(contrast(kTextMuted, kIdleGrey), 4.5);
    for (const Engine::Logic::Category category : Engine::Logic::kCategories)
    {
        const std::uint32_t colour = Engine::Logic::CategoryColor(category);
        const std::string_view name = Engine::Logic::CategoryName(category);
        EXPECT_GE(contrast(colour, kInk), 4.5) << name;
    }
}

// Only what runs other blocks plays them: one block keeps its own preview
TEST(ScriptPreview, OnlyHatsAndCBlocksPlayAScript)
{
    for (const Op op : {Op::WhenPlayStarts, Op::WhenKeyHeld, Op::Repeat, Op::Forever, Op::If, Op::IfElse, Op::Wait})
    {
        EXPECT_TRUE(Editor::Panels::ShowsScript(op));
    }
    for (const Op op : {Op::MoveForward, Op::Jump, Op::SetVariable, Op::Say, Op::Add, Op::Call})
    {
        EXPECT_FALSE(Editor::Panels::ShowsScript(op));
    }
}

// The clips of whatever draws the object: a player's animator is often on the model under it
TEST(BlockPreview, AnObjectsClipsComeFromWhatDrawsIt)
{
    Engine::Scene::SceneDescription scene;
    Engine::Scene::GameObject player{.id = 1, .name = "Player"};
    Engine::Scene::GameObject model{.id = 2, .name = "Model", .parent = 1};
    model.renderable.emplace().model = "assets/art/models/Mage/Mage.glb";
    model.animator.emplace().clip = "Idle";
    Engine::Scene::GameObject sign{.id = 3, .name = "Sign"};
    sign.renderable.emplace();
    scene.objects = {player, model, sign};

    const auto* animator = Editor::Panels::AnimatorOf(scene, scene.objects[0]);
    ASSERT_NE(animator, nullptr);
    EXPECT_EQ(animator->clip, "Idle");
    EXPECT_EQ(Editor::Panels::AnimatorOf(scene, scene.objects[2]), nullptr);
}

// On the canvas the object acts its blocks out; an object drawing nothing itself (its model on a child, as the
// templates' players and villagers have) is shown by the first object under it that draws something
TEST(BlockPreview, AnObjectIsShownByItsOwnLookOrItsChilds)
{
    Engine::Scene::SceneDescription scene;
    Engine::Scene::GameObject villager{.id = 1, .name = "Villager"};
    Engine::Scene::GameObject model{.id = 2, .name = "Villager Model", .parent = 1};
    model.renderable.emplace().model = "assets/art/models/Mage/Mage.glb";
    Engine::Scene::GameObject sign{.id = 3, .name = "Sign"};
    sign.renderable.emplace().shape = Engine::Game::Shape::Pyramid;
    Engine::Scene::GameObject empty{.id = 4, .name = "Spawn point"};
    scene.objects = {villager, model, sign, empty};

    const Engine::Scene::RenderableDesc* look = Editor::Panels::LookOf(scene, scene.objects[0]);
    ASSERT_NE(look, nullptr);
    EXPECT_EQ(look->model, "assets/art/models/Mage/Mage.glb");
    EXPECT_EQ(Editor::Panels::LookOf(scene, scene.objects[2]), &*scene.objects[2].renderable);
    EXPECT_EQ(Editor::Panels::LookOf(scene, scene.objects[3]), nullptr) << "nothing to show: Atti acts it out";
}
