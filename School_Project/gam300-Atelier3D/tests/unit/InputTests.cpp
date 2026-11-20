#include "engine/core/Log.hpp"
#include "engine/input/GameInput.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputActions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace Engine::Input;

namespace
{
    bool AnyContains(const std::vector<std::string>& lines, const std::string& text)
    {
        for (const auto& line : lines)
        {
            if (line.find(text) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    InputBinding Key(int code)
    {
        return {InputBinding::Device::Key, code};
    }

    InputBinding Mouse(int button)
    {
        return {InputBinding::Device::Mouse, button};
    }

    struct LogLine
    {
        Engine::Core::LogLevel level;
        std::string message;
    };

    class LogRecorder final : public Engine::Core::ILogSink
    {
    public:
        void Write(const Engine::Core::LogEntry& entry) override
        {
            const std::scoped_lock lock(mutex_);
            lines_.push_back({entry.level, entry.message});
        }

        std::vector<LogLine> Lines()
        {
            const std::scoped_lock lock(mutex_);
            return lines_;
        }

    private:
        std::mutex mutex_;
        std::vector<LogLine> lines_;
    };

    // Records what is logged while it exists
    class ScopedLogCapture
    {
    public:
        ScopedLogCapture() { Engine::Core::Logger::Get().AddSink(sink_); }
        ~ScopedLogCapture() { Engine::Core::Logger::Get().RemoveSink(sink_); }

        std::vector<LogLine> Lines() { return sink_->Lines(); }

    private:
        std::shared_ptr<LogRecorder> sink_ = std::make_shared<LogRecorder>();
    };

    // Gives each test an empty folder for input_actions.json files
    class InputActionsFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_input_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::filesystem::path Write(const std::string& contents) const
        {
            const auto path = dir_ / "input_actions.json";
            std::ofstream(path) << contents;
            return path;
        }

        std::filesystem::path dir_;
    };
}

// --- InputSystem: pressed / held / released each frame ---

TEST(InputSystem, KeyIsPressedHeldThenReleased)
{
    InputSystem input;

    input.SetKey(Key_W, KeyState::Down); // frame 1: goes down
    EXPECT_TRUE(input.IsKeyPressed(Key_W));
    EXPECT_EQ(input.GetKey(Key_W), KeyState::Down);
    EXPECT_FALSE(input.IsKeyReleased(Key_W));

    input.Update(); // frame 2: still down
    EXPECT_FALSE(input.IsKeyPressed(Key_W));
    EXPECT_EQ(input.GetKey(Key_W), KeyState::Down);

    input.Update();
    input.SetKey(Key_W, KeyState::Up); // frame 3: goes up
    EXPECT_TRUE(input.IsKeyReleased(Key_W));
    EXPECT_EQ(input.GetKey(Key_W), KeyState::Up);

    input.Update(); // frame 4: stays up
    EXPECT_FALSE(input.IsKeyReleased(Key_W));
}

// Pressed is true for one frame only, Down for as long as it is held, then Released for one frame.
TEST(InputSystem, MouseButtonIsPressedHeldThenReleased)
{
    InputSystem input;

    input.SetMouseButton(Mouse_Left, KeyState::Down);
    EXPECT_TRUE(input.IsMouseButtonPressed(Mouse_Left));
    EXPECT_TRUE(input.IsMouseButtonDown(Mouse_Left));
    EXPECT_FALSE(input.IsMouseButtonReleased(Mouse_Left));
    EXPECT_FALSE(input.IsMouseButtonPressed(Mouse_Right));

    input.Update();
    EXPECT_FALSE(input.IsMouseButtonPressed(Mouse_Left));
    EXPECT_TRUE(input.IsMouseButtonDown(Mouse_Left));
    EXPECT_TRUE(input.IsLeftMouseDown());

    input.SetMouseButton(Mouse_Left, KeyState::Up);
    EXPECT_TRUE(input.IsMouseButtonReleased(Mouse_Left));
    EXPECT_FALSE(input.IsMouseButtonDown(Mouse_Left));

    input.Update();
    EXPECT_FALSE(input.IsMouseButtonReleased(Mouse_Left));
}

// A quick click still shows up as Pressed in one frame and Released in the next.
TEST(InputSystem, ClickWithinOneFrameIsPressedAndReleasedAcrossTwoFrames)
{
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    EXPECT_TRUE(input.IsMouseButtonPressed(Mouse_Right));
    input.Update();
    input.SetMouseButton(Mouse_Right, KeyState::Up);
    EXPECT_TRUE(input.IsMouseButtonReleased(Mouse_Right));
}

// Bad key/button codes are ignored instead of writing outside the arrays.
TEST(InputSystem, OutOfRangeCodesAreSafe)
{
    InputSystem input;
    input.SetKey(-1, KeyState::Down);
    input.SetKey(100000, KeyState::Down);
    input.SetMouseButton(42, KeyState::Down);
    EXPECT_FALSE(input.IsKeyPressed(-1));
    EXPECT_FALSE(input.IsMouseButtonPressed(42));
    EXPECT_FALSE(input.IsMouseButtonDown(-3));
    EXPECT_FALSE(input.IsMouseButtonReleased(42));
}

// Mouse delta is the movement since the last Update(), and scroll only lasts one frame.
TEST(InputSystem, MousePositionMovementAndScroll)
{
    InputSystem input;
    input.SetMousePosition(10.0f, 20.0f);
    input.Update();
    input.SetMousePosition(15.0f, 12.0f);
    input.SetScrollDelta(-2.0f);

    EXPECT_FLOAT_EQ(input.GetMousePosition().x, 15.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDelta().x, 5.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDelta().y, -8.0f);
    EXPECT_FLOAT_EQ(input.GetScrollDelta(), -2.0f);

    input.Update(); // movement and scroll are per frame
    EXPECT_FLOAT_EQ(input.GetMouseDelta().x, 0.0f);
    EXPECT_FLOAT_EQ(input.GetScrollDelta(), 0.0f);
}

// --- Binding names ---

TEST(InputBindingNames, ParseKeysAndMouseButtonsIgnoringCase)
{
    EXPECT_EQ(ParseBinding("W"), Key(Key_W));
    EXPECT_EQ(ParseBinding("w"), Key(Key_W));
    EXPECT_EQ(ParseBinding("leftshift"), Key(Key_LeftShift));
    EXPECT_EQ(ParseBinding("Space"), Key(Key_Space));
    EXPECT_EQ(ParseBinding("F12"), Key(Key_F12));
    EXPECT_EQ(ParseBinding("NumPad7"), Key(Key_NumPad7));
    EXPECT_EQ(ParseBinding("7"), Key(Key_7));
    EXPECT_EQ(ParseBinding("MouseRight"), Mouse(Mouse_Right));
    EXPECT_FALSE(ParseBinding("Hyper").has_value());
    EXPECT_FALSE(ParseBinding("").has_value());
}

// Key names from input_actions.json parse to a binding and turn back into the same name.
TEST(InputBindingNames, NamesReadBack)
{
    for (const char* name : {"A", "Z", "0", "F1", "Escape", "PageDown", "Left", "NumPadEnter", "GraveAccent",
                             "RightAlt", "MouseLeft", "MouseMiddle"})
    {
        const auto binding = ParseBinding(name);
        ASSERT_TRUE(binding.has_value()) << name;
        EXPECT_EQ(BindingName(*binding), name);
    }
}

// Every bindable key has a name, and every name parses back to the same key (the Controls panel list relies on this).
TEST(InputBindingNames, AllBindingsHaveNamesThatReadBack)
{
    const auto& all = AllBindings();
    EXPECT_GT(all.size(), 90u);
    EXPECT_NE(std::find(all.begin(), all.end(), Key(Key_W)), all.end());
    EXPECT_NE(std::find(all.begin(), all.end(), Mouse(Mouse_Right)), all.end());
    for (const InputBinding& binding : all)
    {
        EXPECT_EQ(ParseBinding(BindingName(binding)), binding) << BindingName(binding);
    }
}

// --- InputActions ---

TEST(InputActions, AddAndRemoveBindingsAndActions)
{
    InputActions actions = InputActions::Defaults();

    actions.AddBinding(Action::MoveForward, Key(Key_Up));
    actions.AddBinding(Action::MoveForward, Key(Key_Up)); // not repeated
    EXPECT_EQ(*actions.Find(Action::MoveForward), (std::vector{Key(Key_W), Key(Key_Up)}));

    actions.RemoveBinding(Action::MoveForward, Key(Key_W));
    EXPECT_EQ(*actions.Find(Action::MoveForward), std::vector{Key(Key_Up)});
    actions.RemoveBinding(Action::MoveForward, Key(Key_Up));
    ASSERT_NE(actions.Find(Action::MoveForward), nullptr); // the action stays, with no keys
    EXPECT_TRUE(actions.Find(Action::MoveForward)->empty());

    actions.AddBinding("Jump", Key(Key_Space)); // adds the action
    ASSERT_NE(actions.Find("Jump"), nullptr);
    EXPECT_EQ(*actions.Find("Jump"), std::vector{Key(Key_Space)});
    actions.RemoveAction("Jump");
    EXPECT_EQ(actions.Find("Jump"), nullptr);

    actions.RemoveBinding("NoSuchAction", Key(Key_W)); // harmless
    actions.RemoveAction("NoSuchAction");
}

// The default bindings: W forward, either Shift fast, right mouse to look, and no Jump.
TEST(InputActions, DefaultsBindTheCameraActions)
{
    const InputActions actions = InputActions::Defaults();
    ASSERT_NE(actions.Find(Action::MoveForward), nullptr);
    EXPECT_EQ(*actions.Find(Action::MoveForward), std::vector{Key(Key_W)});
    EXPECT_EQ(*actions.Find(Action::MoveFast), (std::vector{Key(Key_LeftShift), Key(Key_RightShift)}));
    EXPECT_EQ(*actions.Find(Action::Look), std::vector{Mouse(Mouse_Right)});
    EXPECT_EQ(actions.Find("Jump"), nullptr);
}

// A valid file remaps keys (names are case-insensitive, one key can be a plain string), can unbind with [] and can add
// new actions like Jump.
TEST_F(InputActionsFileTest, LoadsRemappedKeysAndNewActions)
{
    const auto path = Write(R"({ "version": 1, "actions": {
        "MoveForward": ["Up", "w"], "MoveBack": "Down", "MoveLeft": ["A"], "MoveRight": ["D"],
        "MoveUp": ["E"], "MoveDown": ["Q"], "MoveFast": [], "Look": ["MouseMiddle"],
        "Jump": ["Space"]
    } })");

    std::vector<std::string> problems;
    const InputActions actions = InputActions::Load(path, &problems);

    EXPECT_TRUE(problems.empty()) << problems.front();
    EXPECT_EQ(*actions.Find(Action::MoveForward), (std::vector{Key(Key_Up), Key(Key_W)}));
    EXPECT_EQ(*actions.Find(Action::MoveBack), std::vector{Key(Key_Down)});
    EXPECT_TRUE(actions.Find(Action::MoveFast)->empty()); // [] unbinds on purpose
    EXPECT_EQ(*actions.Find(Action::Look), std::vector{Mouse(Mouse_Middle)});
    ASSERT_NE(actions.Find("Jump"), nullptr);
    EXPECT_EQ(*actions.Find("Jump"), std::vector{Key(Key_Space)});
}

// Save then Load gives back the same actions, including an empty one; the folder is created if needed.
TEST_F(InputActionsFileTest, SaveThenLoadGivesTheSameActions)
{
    InputActions saved = InputActions::Defaults();
    saved.Bind(Action::MoveForward, {Key(Key_Up), Key(Key_NumPad8)});
    saved.Bind("Fire", {Mouse(Mouse_Left), Key(Key_LeftCtrl)});
    saved.Bind("Unused", {});
    const auto path = dir_ / "configs" / "input_actions.json"; // folder is created
    saved.Save(path);

    std::vector<std::string> problems;
    const InputActions loaded = InputActions::Load(path, &problems);

    EXPECT_TRUE(problems.empty()) << problems.front();
    EXPECT_EQ(loaded.All(), saved.All());
}

// No file is normal for an old game: defaults are used and only a warning is logged.
TEST_F(InputActionsFileTest, MissingFileUsesTheDefaultsWithAWarning)
{
    ScopedLogCapture log;

    std::vector<std::string> problems;
    const InputActions actions = InputActions::Load(dir_ / "nope.json", &problems);

    EXPECT_EQ(actions.All(), InputActions::Defaults().All());
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_TRUE(AnyContains(problems, "file not found"));
    const auto lines = log.Lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, Engine::Core::LogLevel::Warning);
    EXPECT_NE(lines[0].message.find("nope.json: file not found"), std::string::npos) << lines[0].message;
}

// Broken JSON: defaults are used and one error is logged.
TEST_F(InputActionsFileTest, CorruptFileUsesTheDefaultsAndLogsAnError)
{
    const auto path = Write(R"({ "version": 1, "actions": { "MoveForward": ["W" )");
    ScopedLogCapture log;

    std::vector<std::string> problems;
    const InputActions actions = InputActions::Load(path, &problems);

    EXPECT_EQ(actions.All(), InputActions::Defaults().All());
    EXPECT_TRUE(AnyContains(problems, "not valid JSON"));
    const auto lines = log.Lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, Engine::Core::LogLevel::Error);
}

// Not an object, no actions, actions in the wrong shape or a newer version: the whole file is ignored and the defaults
// are used.
TEST_F(InputActionsFileTest, WrongLayoutOrVersionUsesTheDefaults)
{
    for (const char* contents : {R"([ "W" ])", R"({ "version": 1 })", R"({ "version": 1, "actions": [] })",
                                 R"({ "version": 2, "actions": { "MoveForward": ["Up"] } })"})
    {
        std::vector<std::string> problems;
        const InputActions actions = InputActions::Load(Write(contents), &problems);

        EXPECT_EQ(actions.All(), InputActions::Defaults().All()) << contents;
        EXPECT_FALSE(problems.empty()) << contents;
    }
}

// Unknown key names, wrong types and missing actions are each reported, and only those actions fall back to their
// default keys.
TEST_F(InputActionsFileTest, MissingActionsAndBadEntriesKeepTheirDefaults)
{
    const auto path = Write(R"({ "actions": {
        "MoveForward": ["Hyper"],
        "MoveBack": ["Nope", "Down"],
        "MoveLeft": 5,
        "MoveRight": ["D"], "MoveUp": ["E"], "MoveDown": ["Q"], "MoveFast": ["LeftShift"],
        "Jump": ["Warp"]
    } })");
    ScopedLogCapture log;

    std::vector<std::string> problems;
    const InputActions actions = InputActions::Load(path, &problems);

    EXPECT_TRUE(AnyContains(problems, "missing \"version\""));
    EXPECT_TRUE(AnyContains(problems, "missing action \"Look\" - using MouseRight"));
    EXPECT_TRUE(AnyContains(problems, "action \"MoveForward\": unknown key \"Hyper\""));
    EXPECT_TRUE(AnyContains(problems, "action \"MoveForward\": no valid keys - using W"));
    EXPECT_TRUE(AnyContains(problems, "action \"MoveBack\": unknown key \"Nope\""));
    EXPECT_TRUE(AnyContains(problems, "action \"MoveLeft\": must be a key name or a list of key names - using A"));
    EXPECT_TRUE(AnyContains(problems, "action \"Jump\": no valid keys - left unbound"));

    EXPECT_EQ(*actions.Find(Action::MoveForward), std::vector{Key(Key_W)});  // default kept
    EXPECT_EQ(*actions.Find(Action::MoveBack), std::vector{Key(Key_Down)});  // the valid name is used
    EXPECT_EQ(*actions.Find(Action::MoveLeft), std::vector{Key(Key_A)});     // default kept
    EXPECT_EQ(*actions.Find(Action::Look), std::vector{Mouse(Mouse_Right)}); // default kept
    ASSERT_NE(actions.Find("Jump"), nullptr);
    EXPECT_TRUE(actions.Find("Jump")->empty());

    // Every problem is in the log, as an error
    const auto lines = log.Lines();
    EXPECT_EQ(lines.size(), problems.size());
    for (const auto& line : lines)
    {
        EXPECT_EQ(line.level, Engine::Core::LogLevel::Error) << line.message;
    }
}

// --- GameInput: actions, and editor / game separation ---

TEST(GameInput, ActionsArePressedHeldAndReleased)
{
    InputSystem input;
    InputActions actions = InputActions::Defaults();
    actions.Bind("Fire", {Mouse(Mouse_Left), Key(Key_Space)});
    const GameInput game(input, actions);

    input.SetKey(Key_W, KeyState::Down);
    input.SetMouseButton(Mouse_Left, KeyState::Down);
    EXPECT_TRUE(game.IsActionPressed(Action::MoveForward));
    EXPECT_TRUE(game.IsActionDown(Action::MoveForward));
    EXPECT_TRUE(game.IsActionPressed("Fire"));

    input.Update();
    EXPECT_FALSE(game.IsActionPressed(Action::MoveForward));
    EXPECT_TRUE(game.IsActionDown(Action::MoveForward));
    EXPECT_TRUE(game.IsActionDown("Fire"));

    input.SetKey(Key_W, KeyState::Up);
    input.SetMouseButton(Mouse_Left, KeyState::Up);
    EXPECT_TRUE(game.IsActionReleased(Action::MoveForward));
    EXPECT_TRUE(game.IsActionReleased("Fire"));
    EXPECT_FALSE(game.IsActionDown("Fire"));
}

// An action with two keys counts as down when either one is.
TEST(GameInput, AnyBindingTriggersTheAction)
{
    InputSystem input;
    InputActions actions;
    actions.Bind("Jump", {Key(Key_Space), Key(Key_W)});
    const GameInput game(input, actions);

    input.SetKey(Key_W, KeyState::Down);
    EXPECT_TRUE(game.IsActionDown("Jump"));
}

// Asking about an action with no keys, or one that does not exist, is just false.
TEST(GameInput, UnknownAndUnboundActionsAreNeverDown)
{
    InputSystem input;
    InputActions actions;
    actions.Bind("Nothing", {});
    const GameInput game(input, actions);
    input.SetKey(Key_W, KeyState::Down);

    EXPECT_FALSE(game.IsActionDown("Nothing"));
    EXPECT_FALSE(game.IsActionPressed("NoSuchAction"));
}

// While a text box has the keyboard, key bindings report nothing but mouse bindings still work.
TEST(GameInput, TypingInTheEditorBlocksKeyBindingsOnly)
{
    InputSystem input;
    InputActions actions = InputActions::Defaults();
    actions.Bind("Fire", {Key(Key_Space), Mouse(Mouse_Left)});
    const GameInput game(input, actions);
    input.SetKey(Key_W, KeyState::Down);
    input.SetKey(Key_Space, KeyState::Down);
    input.SetCaptureInput(true); // an Inspector field has the keyboard

    EXPECT_TRUE(game.IsKeyboardBlocked());
    EXPECT_FALSE(game.IsActionDown(Action::MoveForward));
    EXPECT_FALSE(game.IsActionPressed(Action::MoveForward));
    EXPECT_FALSE(game.IsActionDown("Fire"));

    input.SetMouseButton(Mouse_Left, KeyState::Down); // the mouse still reaches the game
    EXPECT_TRUE(game.IsActionDown("Fire"));

    input.SetCaptureInput(false); // back in the game
    EXPECT_TRUE(game.IsActionDown(Action::MoveForward));
}

// While the mouse is over an editor panel, mouse buttons, movement and scroll report nothing but keys still work.
TEST(GameInput, MouseOverTheEditorBlocksMouseBindingsMovementAndScroll)
{
    InputSystem input;
    const InputActions actions = InputActions::Defaults();
    const GameInput game(input, actions);
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetMousePosition(40.0f, 30.0f);
    input.SetScrollDelta(1.0f);
    input.SetKey(Key_W, KeyState::Down);
    input.SetCaptureMouse(true); // pointer over an editor panel

    EXPECT_TRUE(game.IsMouseBlocked());
    EXPECT_FALSE(game.IsActionDown(Action::Look));
    EXPECT_FLOAT_EQ(game.GetMouseDelta().x, 0.0f);
    EXPECT_FLOAT_EQ(game.GetScrollDelta(), 0.0f);
    EXPECT_FLOAT_EQ(game.GetMousePosition().x, 40.0f);   // position is still known
    EXPECT_TRUE(game.IsActionDown(Action::MoveForward)); // keys still reach the game

    input.SetCaptureMouse(false);
    EXPECT_TRUE(game.IsActionDown(Action::Look));
    EXPECT_FLOAT_EQ(game.GetMouseDelta().x, 40.0f);
    EXPECT_FLOAT_EQ(game.GetScrollDelta(), 1.0f);
}
