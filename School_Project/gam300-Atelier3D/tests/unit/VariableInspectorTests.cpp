#include "engine/logic/Blocks.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/GameObjectValues.hpp"
#include "engine/scene/Scene.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// What the Variable Inspector (F03) shows: which blocks write a value (Blocks), which block wrote it last
// (LogicRunner::LastWrite), how values read, and that one Step changes exactly what its block writes

using Engine::Logic::Block;
using Engine::Logic::FormatNumber;
using Engine::Logic::LogicDesc;
using Engine::Logic::LogicRunner;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;
using Engine::Logic::Property;
using Engine::Logic::PropertyOf;
using Engine::Logic::Script;
using Engine::Logic::ValueText;
using Engine::Logic::Variable;
using Engine::Logic::VariableKind;

namespace
{
    // A block with its numbers, text (key, message, object or variable) and body set
    Block Make(Op op, std::uint32_t id, const std::vector<float>& numbers = {}, std::string text = {},
               std::vector<Block> body = {})
    {
        Block block = MakeBlock(op, id);
        for (std::size_t i = 0; i < numbers.size() && i < block.numbers.size(); ++i)
        {
            block.numbers[i] = numbers[i];
        }
        if (!text.empty())
        {
            block.text = std::move(text);
        }
        block.body = std::move(body);
        return block;
    }

    std::vector<std::uint32_t> IdsOf(const std::vector<Engine::Logic::BlockAt>& found)
    {
        std::vector<std::uint32_t> ids(found.size());
        std::transform(found.begin(), found.end(), ids.begin(),
                       [](const Engine::Logic::BlockAt& at) { return at.block->id; });
        return ids;
    }

    // An ECS world with object 1 (Transform, something to draw) and a runner for its logic, with the
    // variables Speed (0) and IsAlive (true)
    class InspectTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            entity_ = ecs_.CreateEntity();
            ecs_.AddComponent<Engine::Scene::MeshRendererComponent>(entity_);
        }

        LogicDesc Logic(std::vector<Block> blocks) const
        {
            return LogicDesc{
                .scripts = {Script{.blocks = std::move(blocks)}},
                .variables = {Variable{.name = "Speed", .value = 0.0f},
                              Variable{.name = "IsAlive", .value = 1.0f, .kind = VariableKind::TrueFalse}}};
        }

        void Run(std::vector<Block> blocks) { runner_.Set(1, "Player", entity_.GetId(), Logic(std::move(blocks))); }

        void Update() { runner_.Update(ecs_, 1.0 / 60.0, LogicRunner::Keys{}); }

        // StepOnce, then the frame that runs it
        void Step()
        {
            runner_.StepOnce();
            Update();
        }

        double VariableNow(const std::string& name) const
        {
            for (const auto& [each, value] : runner_.Variables(1))
            {
                if (each == name)
                {
                    return value;
                }
            }
            return std::numeric_limits<double>::quiet_NaN();
        }

        // Every value the Inspector shows that a block can change, written out in full
        std::map<std::string, std::string> Values() const
        {
            const auto t = ecs_.GetTransform(entity_).value_or(Engine::Scene::TransformComponent{});
            const auto renderer = ecs_.GetComponent<Engine::Scene::MeshRendererComponent>(entity_).value();
            return {{"Speed", std::format("{}", VariableNow("Speed"))},
                    {"IsAlive", std::format("{}", VariableNow("IsAlive"))},
                    {"Position", std::format("{} {} {}", t.x, t.y, t.z)},
                    {"Rotation", std::format("{} {} {}", t.rotationX, t.rotation, t.rotationZ)},
                    {"Size", std::format("{}", t.scale)},
                    {"Visible", std::format("{}", renderer.visible)},
                    {"Colour", std::format("{} {} {}", renderer.albedoR, renderer.albedoG, renderer.albedoB)},
                    {"Says", runner_.LastSaid(1)}};
        }

        // The names of the values that differ
        static std::vector<std::string> Changed(const std::map<std::string, std::string>& before,
                                                const std::map<std::string, std::string>& after)
        {
            std::vector<std::string> changed;
            for (const auto& [name, value] : before)
            {
                if (after.at(name) != value)
                {
                    changed.push_back(name);
                }
            }
            return changed;
        }

        Engine::Scene::Scene ecs_;
        Engine::Scene::Entity entity_;
        LogicRunner runner_;
    };
}

// --- Which blocks write what ---

// The table says which value a block writes (its row's writes). Run on its own, in a state where it has something to
// change, every block changes that value and no other, so the table and what the blocks do cannot drift apart.
TEST_F(InspectTest, EveryBlockChangesOnlyTheValueItsRowSays)
{
    static constexpr std::array<const char*, Engine::Logic::kPropertyCount> kNames{
        "Position", "Rotation", "Size", "Visible", "Colour", "Says"};
    const Engine::Scene::Entity target = ecs_.CreateEntity();
    ecs_.SetTransform(target, Engine::Scene::TransformComponent{.x = 5.0f, .z = 5.0f});
    for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
    {
        if (info.shape != Engine::Logic::BlockShape::Stack || info.hasBody || Engine::Logic::HasVariableField(info.op))
        {
            continue; // the control blocks and the variable blocks have their own tests
        }
        Block block = MakeBlock(info.op, 2);
        if (info.op == Op::GoToObject || info.op == Op::PointTowards)
        {
            block.text = "Target";
        }
        if (info.op == Op::SetTilt)
        {
            block.numbers[0] = 30.0f; // its default is level, which changes nothing
        }
        // Away from the origin and from the table's defaults, and shown unless the block is the one that shows it
        ecs_.SetTransform(entity_, Engine::Scene::TransformComponent{
                                       .x = 1.0f, .y = 2.0f, .z = 3.0f, .rotation = 40.0f, .scale = 3.0f});
        auto* renderer = ecs_.TryGetComponent<Engine::Scene::MeshRendererComponent>(entity_);
        renderer->visible = info.op != Op::Show;
        renderer->albedoR = renderer->albedoG = renderer->albedoB = 0.25f;
        runner_.SetObjects(
            {{.name = "Player", .entity = entity_.GetId()}, {.name = "Target", .entity = target.GetId()}});

        Run({MakeBlock(Op::WhenPlayStarts, 1), block}); // a new start: nothing said yet
        const auto before = Values();
        Update();
        Update(); // a glide moves over the frames after the block
        const std::optional<Property> property = PropertyOf(info.op);
        const std::vector<std::string> expected =
            property ? std::vector<std::string>{kNames.at(static_cast<std::size_t>(*property))}
                     : std::vector<std::string>{};
        EXPECT_EQ(Changed(before, Values()), expected) << info.name;
    }
}

// The set and change blocks of a variable are the blocks that write a variable
TEST(InspectBlocks, OnlyTheSetAndChangeBlocksWriteAVariable)
{
    for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
    {
        EXPECT_EQ(Engine::Logic::WritesVariable(info.op), info.op == Op::SetVariable || info.op == Op::ChangeVariable)
            << info.name;
    }
}

// A variable's set / change blocks are found at the top, inside a loop and in an else, across scripts, in order; a
// block that only reads it, and another variable's, are not. A loose stack never runs.
TEST(InspectBlocks, WritersOfAVariableAreFoundWhereverTheyAre)
{
    Block read = Make(Op::MoveBy, 9);
    read.inputs[0] = Make(Op::VariableValue, 8, {}, "Speed");
    Block ifElse = MakeBlock(Op::IfElse, 5);
    ifElse.elseBody = {Make(Op::ChangeVariable, 6, {1}, "Speed")};
    const LogicDesc logic{
        .scripts = {Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"),
                                      Make(Op::Repeat, 3, {3}, {}, {Make(Op::ChangeVariable, 4, {2}, "Speed")}), ifElse,
                                      Make(Op::ChangeVariable, 7, {1}, "Score"), read}},
                    Script{.x = 200.0f, .blocks = {Make(Op::SetVariable, 10, {0}, "Speed")}}},
        .variables = {Variable{.name = "Speed"}, Variable{.name = "Score"}}};

    const auto found = Engine::Logic::WritersOf(logic, "Speed");
    EXPECT_EQ(IdsOf(found), (std::vector<std::uint32_t>{2, 4, 6, 10}));
    ASSERT_EQ(found.size(), 4u);
    EXPECT_TRUE(found[0].runs);
    EXPECT_EQ(found[2].script, 0u);
    EXPECT_FALSE(found[3].runs); // no "when" block above it
    EXPECT_EQ(found[3].script, 1u);
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, "Score")), (std::vector<std::uint32_t>{7}));
    EXPECT_TRUE(Engine::Logic::WritersOf(logic, "Lives").empty());
}

TEST(InspectBlocks, WritersOfAPropertyAreItsBlocks)
{
    const LogicDesc logic{.scripts = {Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 1), Make(Op::MoveForward, 2, {1}),
                                                        Make(Op::TurnBy, 3, {90}), Make(Op::GoTo, 4), Make(Op::Hide, 5),
                                                        Make(Op::Show, 6), Make(Op::Wait, 7, {1}),
                                                        Make(Op::Say, 8, {}, "hi"), Make(Op::SetColor, 9, {1, 0, 0}),
                                                        Make(Op::SetSize, 10, {2}), Make(Op::Wait, 11, {1})}}}};

    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Position)), (std::vector<std::uint32_t>{2, 4}));
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Rotation)), (std::vector<std::uint32_t>{3}));
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Visible)), (std::vector<std::uint32_t>{5, 6}));
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Says)), (std::vector<std::uint32_t>{8}));
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Colour)), (std::vector<std::uint32_t>{9}));
    EXPECT_EQ(IdsOf(Engine::Logic::WritersOf(logic, Property::Size)), (std::vector<std::uint32_t>{10}));
}

// --- How values read ---

TEST(InspectFormat, NumbersReadAsPeopleWriteThem)
{
    EXPECT_EQ(FormatNumber(5.0), "5");
    EXPECT_EQ(FormatNumber(2.5), "2.5");
    EXPECT_EQ(FormatNumber(1.0 / 3.0), "0.333");
    EXPECT_EQ(FormatNumber(-2.0), "-2");
    EXPECT_EQ(FormatNumber(-0.0), "0");
    EXPECT_EQ(FormatNumber(-0.0001), "0"); // rounds to 0, not -0
    EXPECT_EQ(FormatNumber(1e10), "10000000000");
    EXPECT_EQ(FormatNumber(std::numeric_limits<double>::infinity()), "infinity");
    EXPECT_EQ(FormatNumber(-std::numeric_limits<double>::infinity()), "-infinity");
    EXPECT_EQ(FormatNumber(std::numeric_limits<double>::quiet_NaN()), "not a number");
}

TEST(InspectFormat, TrueFalseVariablesReadZeroAsFalse)
{
    EXPECT_EQ(ValueText(VariableKind::TrueFalse, 0.0), "false");
    for (const double value : {1.0, -1.0, 0.5})
    {
        EXPECT_EQ(ValueText(VariableKind::TrueFalse, value), "true") << value;
    }
    EXPECT_EQ(ValueText(VariableKind::Number, 0.0), "0");
    EXPECT_EQ(ValueText(VariableKind::Number, 2.5), "2.5");
}

// --- Which block wrote a value last ---

TEST_F(InspectTest, NothingIsWrittenUntilABlockRuns)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed")});
    EXPECT_FALSE(runner_.LastWrite(1, "Speed").has_value());
    EXPECT_FALSE(runner_.LastWrite(2, "Speed").has_value()); // no such object
}

TEST_F(InspectTest, TheLastWriterIsTheLastBlockThatWrote)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"),
         Make(Op::ChangeVariable, 3, {2}, "Speed")});
    Update();

    const auto write = runner_.LastWrite(1, "Speed");
    ASSERT_TRUE(write.has_value());
    EXPECT_EQ(write->block, 3u);
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 7.0);
    EXPECT_FALSE(runner_.LastWrite(1, "IsAlive").has_value());
}

// Setting the same value again is still a new write: the serial goes up, so the Inspector shows it
TEST_F(InspectTest, TheSameValueAgainIsANewWrite)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {}, {Make(Op::SetVariable, 3, {5}, "Speed")})});
    Update();
    const auto first = runner_.LastWrite(1, "Speed");
    Update();
    const auto second = runner_.LastWrite(1, "Speed");

    ASSERT_TRUE(first && second);
    EXPECT_EQ(second->block, 3u);
    EXPECT_GT(second->serial, first->serial);
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 5.0);
}

// Reading a variable is not writing it, and a block that could not write (no such variable, nothing to draw) records
// nothing
TEST_F(InspectTest, OnlyBlocksThatWriteAreRecorded)
{
    Block read = Make(Op::MoveBy, 2);
    read.inputs[0] = Make(Op::VariableValue, 3, {}, "IsAlive");
    ecs_.RemoveComponent<Engine::Scene::MeshRendererComponent>(entity_);
    Run({MakeBlock(Op::WhenPlayStarts, 1), read, Make(Op::SetVariable, 4, {1}, "Missing"), Make(Op::Hide, 5)});
    Update();

    EXPECT_FALSE(runner_.LastWrite(1, "IsAlive").has_value());
    EXPECT_FALSE(runner_.LastWrite(1, "Missing").has_value());
    EXPECT_FALSE(runner_.LastWrite(1, Property::Visible).has_value()); // nothing to hide
    ASSERT_TRUE(runner_.LastWrite(1, Property::Position).has_value()); // the move did happen
    EXPECT_EQ(runner_.LastWrite(1, Property::Position)->block, 2u);
}

// Each object has its own writes, and Set (an edit while playing) starts them again
TEST_F(InspectTest, WritesArePerObjectAndForgottenBySet)
{
    const Engine::Scene::Entity other = ecs_.CreateEntity();
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed")});
    runner_.Set(2, "Enemy", other.GetId(), Logic({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Wait, 2, {9})}));
    Update();

    EXPECT_TRUE(runner_.LastWrite(1, "Speed").has_value());
    EXPECT_FALSE(runner_.LastWrite(2, "Speed").has_value());

    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed")});
    EXPECT_FALSE(runner_.LastWrite(1, "Speed").has_value());
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 0.0); // back to its start value
}

// Every block PropertyOf names records a write of that property when it runs, so the table and the runner agree
TEST_F(InspectTest, EveryPropertyBlockRecordsItsProperty)
{
    const Engine::Scene::Entity target = ecs_.CreateEntity();
    ecs_.SetTransform(target, Engine::Scene::TransformComponent{.x = 5.0f, .z = 5.0f});
    for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
    {
        const auto property = PropertyOf(info.op);
        if (!property)
        {
            continue;
        }
        Block block = MakeBlock(info.op, 2);
        if (info.op == Op::GoToObject || info.op == Op::PointTowards)
        {
            block.text = "Target";
        }
        if (info.op == Op::SetTilt)
        {
            block.numbers[0] = 30.0f; // its default is level, which changes nothing
        }
        runner_.SetObjects(
            {{.name = "Player", .entity = entity_.GetId()}, {.name = "Target", .entity = target.GetId()}});
        ecs_.SetTransform(entity_, {}); // back at the origin, away from the target (go to object moved it there)
        Run({MakeBlock(Op::WhenPlayStarts, 1), block});
        Update();

        const auto write = runner_.LastWrite(1, *property);
        ASSERT_TRUE(write.has_value()) << info.name;
        EXPECT_EQ(write->block, 2u) << info.name;
    }
}

// --- One Step, one block's change (ATE3D-339) ---

// From a breakpoint, each Step runs one block: exactly the value that block writes changes, by what the block says,
// and the Inspector's "set by" names that block
TEST_F(InspectTest, StepChangesExactlyWhatItsBlockWrites)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"),
         Make(Op::ChangeVariable, 3, {2}, "Speed"), Make(Op::SetVariable, 4, {0}, "IsAlive"),
         Make(Op::ChangeVariable, 5, {-10}, "Speed"), Make(Op::MoveForward, 6, {2}), Make(Op::TurnBy, 7, {90}), Make(Op::Hide, 8),
         Make(Op::SetColor, 9, {1, 0, 0}), Make(Op::SetSize, 10, {2}), Make(Op::Say, 11, {}, "hi")});
    runner_.ToggleBreakpoint(1, 2);
    Update();
    ASSERT_TRUE(runner_.IsPaused());
    ASSERT_EQ(runner_.Debug().pausedBlock, 2u);

    struct Expected
    {
        std::uint32_t block;
        std::string value;
        std::string now;
    };
    const std::vector<Expected> steps{{2, "Speed", "5"},       {3, "Speed", "7"},         {4, "IsAlive", "0"},
                                      {5, "Speed", "-3"},     {6, "Position", "0 0 -2"}, {7, "Rotation", "0 90 0"},
                                      {8, "Visible", "false"}, {9, "Colour", "1 0 0"},    {10, "Size", "2"},
                                      {11, "Says", "hi"}};
    const std::map<std::string, Property> properties{{"Position", Property::Position},
                                                     {"Rotation", Property::Rotation}, {"Visible", Property::Visible},
                                                     {"Colour", Property::Colour},     {"Size", Property::Size},
                                                     {"Says", Property::Says}};
    for (const Expected& step : steps)
    {
        const auto before = Values();
        Step();
        const auto after = Values();

        EXPECT_EQ(Changed(before, after), std::vector<std::string>{step.value}) << "block " << step.block;
        EXPECT_EQ(after.at(step.value), step.now) << "block " << step.block;
        const auto property = properties.find(step.value);
        const auto write =
            property != properties.end() ? runner_.LastWrite(1, property->second) : runner_.LastWrite(1, step.value);
        ASSERT_TRUE(write.has_value()) << "block " << step.block;
        EXPECT_EQ(write->block, step.block);
        EXPECT_TRUE(runner_.IsPaused());
    }
}

// A Step over a block that writes nothing changes no value and no write
TEST_F(InspectTest, AStepOverABlockThatWritesNothingChangesNothing)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"), Make(Op::Broadcast, 3, {}, "go"),
         Make(Op::SetVariable, 4, {6}, "Speed")});
    runner_.ToggleBreakpoint(1, 3);
    Update();
    const auto before = Values();
    const auto write = runner_.LastWrite(1, "Speed");

    Step();
    EXPECT_EQ(Values(), before);
    EXPECT_EQ(runner_.LastWrite(1, "Speed"), write);
    EXPECT_EQ(runner_.Debug().pausedBlock, 4u);
}

// Each script that can run says where it is: not started, the block it ran last and the one it runs next, then ended
TEST_F(InspectTest, EachScriptSaysWhichBlockItIsOn)
{
    LogicDesc logic = Logic({MakeBlock(Op::WhenKeyPressed, 1), Make(Op::MoveForward, 2, {1})});
    logic.scripts.push_back(Script{.blocks = {MakeBlock(Op::WhenPlayStarts, 3), Make(Op::SetVariable, 4, {5}, "Speed"),
                                              Make(Op::TurnBy, 5, {15})}});
    logic.scripts.push_back(Script{.blocks = {Make(Op::MoveForward, 6, {1})}}); // no hat: never runs, so no row
    runner_.Set(1, "Player", entity_.GetId(), logic);
    runner_.ToggleBreakpoint(1, 5);

    using State = LogicRunner::ScriptState;
    EXPECT_EQ(runner_.Scripts(1), (std::vector<State>{{.script = 0}, {.script = 1}}));

    Update(); // the second script runs its Set block, then stops before the Turn block
    EXPECT_EQ(runner_.Scripts(1),
              (std::vector<State>{{.script = 0}, {.script = 1, .running = true, .last = 4, .next = 5, .before = {3}}}));

    Step();
    const std::vector<State> scripts = runner_.Scripts(1);
    ASSERT_EQ(scripts.size(), 2u);
    EXPECT_EQ(scripts[1].last, 5u);
    EXPECT_FALSE(scripts[1].running);
    EXPECT_EQ(scripts[1].next, 0u);
    EXPECT_TRUE(runner_.Scripts(2).empty()); // an object with no logic
}

// In a loop, each Step adds at most once, and Speed counts the times the change block ran
TEST_F(InspectTest, ALoopAddsOncePerStep)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::Forever, 2, {}, {}, {Make(Op::ChangeVariable, 3, {1}, "Speed")})});
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_DOUBLE_EQ(VariableNow("Speed"), 0.0);
    runner_.ToggleBreakpoint(1, 3); // off: Step alone moves it on

    int ran = 0;
    for (int i = 0; i < 6; ++i)
    {
        const bool next = runner_.Debug().pausedBlock == 3;
        const double before = VariableNow("Speed");
        Step();
        const double added = VariableNow("Speed") - before;
        EXPECT_DOUBLE_EQ(added, next ? 1.0 : 0.0) << "step " << i;
        ran += next ? 1 : 0;
    }
    EXPECT_GE(ran, 2);
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), ran);
    EXPECT_EQ(runner_.LastWrite(1, "Speed")->block, 3u);
}

// Paused, nothing moves: no value and no write changes however many frames go by
TEST_F(InspectTest, NothingChangesWhilePaused)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1),
         Make(Op::Forever, 2, {}, {}, {Make(Op::ChangeVariable, 3, {1}, "Speed"), Make(Op::MoveBy, 4, {1, 0, 0})})});
    Update();
    runner_.Pause(1);
    const auto before = Values();
    const auto speed = runner_.LastWrite(1, "Speed");
    const auto position = runner_.LastWrite(1, Property::Position);
    for (int i = 0; i < 100; ++i)
    {
        Update();
    }
    EXPECT_EQ(Values(), before);
    EXPECT_EQ(runner_.LastWrite(1, "Speed"), speed);
    EXPECT_EQ(runner_.LastWrite(1, Property::Position), position);
}

// A breakpoint stops before its block writes; Resume writes once
TEST_F(InspectTest, ABreakpointStopsBeforeTheWrite)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed")});
    runner_.ToggleBreakpoint(1, 2);
    Update();
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 0.0);
    EXPECT_FALSE(runner_.LastWrite(1, "Speed").has_value());

    runner_.Resume();
    Update();
    const auto write = runner_.LastWrite(1, "Speed");
    ASSERT_TRUE(write.has_value());
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 5.0);
    Update(); // the script has ended: no more writes
    EXPECT_EQ(runner_.LastWrite(1, "Speed"), write);
}

// An edit while paused (Set) puts the variables back to their start values and forgets who wrote them
TEST_F(InspectTest, AnEditWhilePausedGoesBackToStartValues)
{
    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"),
         Make(Op::SetVariable, 3, {6}, "Speed")});
    runner_.ToggleBreakpoint(1, 3);
    Update();
    ASSERT_DOUBLE_EQ(VariableNow("Speed"), 5.0);

    Run({MakeBlock(Op::WhenPlayStarts, 1), Make(Op::SetVariable, 2, {5}, "Speed"),
         Make(Op::SetVariable, 3, {6}, "Speed")});
    EXPECT_DOUBLE_EQ(VariableNow("Speed"), 0.0);
    EXPECT_DOUBLE_EQ(VariableNow("IsAlive"), 1.0);
    EXPECT_FALSE(runner_.LastWrite(1, "Speed").has_value());
}

// --- Which saved fields there are ---

namespace
{
    // An object with every kind of component, a tint that is not white and a prefab link
    Engine::Scene::GameObject FullObject()
    {
        Engine::Scene::GameObject object;
        object.id = 1;
        object.name = "Everything";
        object.transform = Engine::Scene::TransformComponent{.x = 2.0f};
        object.renderable = Engine::Scene::RenderableDesc{.rotationSpeed = 30.0f, .albedoR = 0.5f};
        object.audioSource = Engine::Scene::AudioSourceDesc{.clip = "sounds/a.wav", .loop = true};
        object.player.emplace();
        object.camera.emplace();
        object.light.emplace();
        object.logic.emplace();
        object.prefabInstance.emplace();
        return object;
    }
}

// The live view lists every field the scene file saves, whatever the components are
TEST(InspectValues, ListsTheSavedFieldsOfEveryComponent)
{
    const Engine::Scene::ObjectValues values = Engine::Scene::ValuesOf(FullObject());

    for (const char* component : {"transform", "renderable", "audio_source", "player", "camera", "light"})
    {
        EXPECT_TRUE(values.contains(component)) << component;
    }
    EXPECT_FALSE(values.contains("logic"));  // the blocks are not a component
    EXPECT_FALSE(values.contains("prefab")); // nor is the prefab link
    EXPECT_FALSE(values.contains("name"));

    using Type = Engine::Scene::FieldValue::Type;
    EXPECT_EQ(values.at("transform").at("x").number, 2.0);
    EXPECT_EQ(values.at("renderable").at("rotation_speed").number, 30.0);
    EXPECT_EQ(values.at("renderable").at("albedo_r").number, 0.5);
    EXPECT_EQ(values.at("audio_source").at("loop").type, Type::Flag);
    EXPECT_EQ(values.at("audio_source").at("loop").number, 1.0);
    EXPECT_EQ(values.at("audio_source").at("clip").type, Type::Text);
    EXPECT_EQ(values.at("audio_source").at("clip").text, "sounds/a.wav");
}

// An object only has the components it has
TEST(InspectValues, AnObjectWithoutComponentsHasNoValues)
{
    Engine::Scene::GameObject object;
    object.id = 1;
    EXPECT_TRUE(Engine::Scene::ValuesOf(object).empty());
}

// Blocks write Position, Rotation, Size, and Colour, which are saved fields; Visible and Says are not saved,
// so no field maps to them. A property added later has to be one or the other, so it cannot be forgotten.
TEST(InspectValues, EveryPropertyThatIsSavedMapsFromItsFields)
{
    std::set<Property> found;
    for (const auto& [component, fields] : Engine::Scene::ValuesOf(FullObject()))
    {
        for (const auto& [field, value] : fields)
        {
            if (const auto property = Engine::Logic::PropertyOfField(component, field))
            {
                found.insert(*property);
            }
        }
    }
    for (std::size_t i = 0; i < Engine::Logic::kPropertyCount; ++i)
    {
        const auto property = static_cast<Property>(i);
        const bool saved = property != Property::Visible && property != Property::Says;
        EXPECT_EQ(found.contains(property), saved) << "property " << i;
    }

    EXPECT_EQ(Engine::Logic::PropertyOfField("transform", "rotation_z"), Property::Rotation);
    EXPECT_EQ(Engine::Logic::PropertyOfField("transform", "scale_x"), std::nullopt); // no block writes the axes' scale
    EXPECT_EQ(Engine::Logic::PropertyOfField("player", "speed"), std::nullopt);
}
