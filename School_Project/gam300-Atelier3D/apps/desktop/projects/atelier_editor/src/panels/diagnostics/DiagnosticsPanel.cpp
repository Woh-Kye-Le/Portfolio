/* !
@file    	    DiagnosticsPanel.cpp
@author  	    sanni.go@digipen.edu [80%]
                c.kojat@digipen.edu [20%]
@date    		28/09/2026
@brief
    Implements DiagnosticsPanel: the shortcut cheat sheet, editor camera values,
    held keys, mouse state, capture flags and a log of recent input events.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// EditorCamera.hpp first: its graphics headers must come before Input.hpp's GLFW header (which includes GL/gl.h)
#include "editor/panels/diagnostics/DiagnosticsPanel.hpp"
#include "editor/camera/EditorCamera.hpp"
#include "engine/input/Input.hpp"
#include "engine/messenger/Events.hpp"

#include <imgui/imgui.h>

#include <array>
#include <string>
#include <utility>

namespace Editor::Panels
{
    namespace
    {
        using namespace Engine::Input;

        // Every key InputSystem knows about (Input.hpp's KeyCode), with a readable name
        constexpr std::array kKeys = {
            std::pair{Key_A, "A"},
            std::pair{Key_B, "B"},
            std::pair{Key_C, "C"},
            std::pair{Key_D, "D"},
            std::pair{Key_E, "E"},
            std::pair{Key_F, "F"},
            std::pair{Key_G, "G"},
            std::pair{Key_H, "H"},
            std::pair{Key_I, "I"},
            std::pair{Key_J, "J"},
            std::pair{Key_K, "K"},
            std::pair{Key_L, "L"},
            std::pair{Key_M, "M"},
            std::pair{Key_N, "N"},
            std::pair{Key_O, "O"},
            std::pair{Key_P, "P"},
            std::pair{Key_Q, "Q"},
            std::pair{Key_R, "R"},
            std::pair{Key_S, "S"},
            std::pair{Key_T, "T"},
            std::pair{Key_U, "U"},
            std::pair{Key_V, "V"},
            std::pair{Key_W, "W"},
            std::pair{Key_X, "X"},
            std::pair{Key_Y, "Y"},
            std::pair{Key_Z, "Z"},
            std::pair{Key_0, "0"},
            std::pair{Key_1, "1"},
            std::pair{Key_2, "2"},
            std::pair{Key_3, "3"},
            std::pair{Key_4, "4"},
            std::pair{Key_5, "5"},
            std::pair{Key_6, "6"},
            std::pair{Key_7, "7"},
            std::pair{Key_8, "8"},
            std::pair{Key_9, "9"},
            std::pair{Key_F1, "F1"},
            std::pair{Key_F2, "F2"},
            std::pair{Key_F3, "F3"},
            std::pair{Key_F4, "F4"},
            std::pair{Key_F5, "F5"},
            std::pair{Key_F6, "F6"},
            std::pair{Key_F7, "F7"},
            std::pair{Key_F8, "F8"},
            std::pair{Key_F9, "F9"},
            std::pair{Key_F10, "F10"},
            std::pair{Key_F11, "F11"},
            std::pair{Key_F12, "F12"},
            std::pair{Key_Space, "Space"},
            std::pair{Key_Enter, "Enter"},
            std::pair{Key_Escape, "Escape"},
            std::pair{Key_Tab, "Tab"},
            std::pair{Key_Backspace, "Backspace"},
            std::pair{Key_Delete, "Delete"},
            std::pair{Key_Insert, "Insert"},
            std::pair{Key_Home, "Home"},
            std::pair{Key_End, "End"},
            std::pair{Key_PageUp, "PageUp"},
            std::pair{Key_PageDown, "PageDown"},
            std::pair{Key_Up, "Up"},
            std::pair{Key_Down, "Down"},
            std::pair{Key_Left, "Left"},
            std::pair{Key_Right, "Right"},
            std::pair{Key_NumPad0, "NumPad0"},
            std::pair{Key_NumPad1, "NumPad1"},
            std::pair{Key_NumPad2, "NumPad2"},
            std::pair{Key_NumPad3, "NumPad3"},
            std::pair{Key_NumPad4, "NumPad4"},
            std::pair{Key_NumPad5, "NumPad5"},
            std::pair{Key_NumPad6, "NumPad6"},
            std::pair{Key_NumPad7, "NumPad7"},
            std::pair{Key_NumPad8, "NumPad8"},
            std::pair{Key_NumPad9, "NumPad9"},
            std::pair{Key_NumPadEnter, "NumPadEnter"},
            std::pair{Key_NumPadAdd, "NumPad+"},
            std::pair{Key_NumPadSubtract, "NumPad-"},
            std::pair{Key_NumPadMultiply, "NumPad*"},
            std::pair{Key_NumPadDivide, "NumPad/"},
            std::pair{Key_Comma, ","},
            std::pair{Key_Period, "."},
            std::pair{Key_Slash, "/"},
            std::pair{Key_Semicolon, ";"},
            std::pair{Key_Apostrophe, "'"},
            std::pair{Key_LeftBracket, "["},
            std::pair{Key_RightBracket, "]"},
            std::pair{Key_Backslash, "\\"},
            std::pair{Key_Minus, "-"},
            std::pair{Key_Equal, "="},
            std::pair{Key_GraveAccent, "`"},
            std::pair{Key_CapsLock, "CapsLock"},
            std::pair{Key_NumLock, "NumLock"},
            std::pair{Key_ScrollLock, "ScrollLock"},
            std::pair{Key_LeftCtrl, "LeftCtrl"},
            std::pair{Key_RightCtrl, "RightCtrl"},
            std::pair{Key_LeftShift, "LeftShift"},
            std::pair{Key_RightShift, "RightShift"},
            std::pair{Key_LeftAlt, "LeftAlt"},
            std::pair{Key_RightAlt, "RightAlt"},
        };

        constexpr std::array kMouseButtons = {std::pair{Mouse_Left, "Left"}, std::pair{Mouse_Right, "Right"},
                                              std::pair{Mouse_Middle, "Middle"}};

        // keep the panel short, we only need the last few events to check the input works
        constexpr std::size_t kMaxLogLines = 10;
        constexpr std::size_t kMaxEventLines = 20; // the Game events log
        constexpr std::size_t kMaxTypedText = 80;

        const char* YesNo(bool value)
        {
            return value ? "yes" : "no";
        }

        // newest at the top, oldest falls off the bottom
        void PushLine(std::deque<std::string>& log, std::string line)
        {
            log.push_front(std::move(line));
            if (log.size() > kMaxLogLines)
            {
                log.pop_back();
            }
        }
    }

    DiagnosticsPanel::DiagnosticsPanel(Engine::Input::InputSystem& input, const Camera::EditorCamera& camera)
        : Panel("Diagnostics"), input_(input), camera_(camera)
    {
    }

    // Two sections: a cheat sheet for the editor controls, and the raw input state
    void DiagnosticsPanel::DrawContents()
    {
        DrawEditorInput();
        DrawInput();
        DrawGameEvents();
    }

    // ---------------------------------------------------------------- Editor input (shortcuts + fly camera)

    // Lists the shortcuts and shows where the editor camera is
    void DiagnosticsPanel::DrawEditorInput()
    {
        if (!ImGui::CollapsingHeader("Editor input", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::TextUnformatted("Shortcuts: F5 run / stop   Ctrl+S save   Ctrl+B build   Ctrl+R refresh games");
        ImGui::TextUnformatted("  F11 fullscreen   Shift+Space maximize the Viewport");
        ImGui::TextUnformatted("Viewport camera (fly keys remap in configs/input_actions.json):");
        ImGui::TextUnformatted("  hold right mouse: drag to look, WASD fly, Q/E down/up, Shift faster, wheel = speed");
        ImGui::TextUnformatted("  Alt + left drag orbit   middle drag pan   wheel zoom   F frame the selection");
        ImGui::TextUnformatted("Viewport tools: click to select, W move  E rotate  R scale  Q no gizmo, Ctrl snaps;");
        ImGui::TextUnformatted("  the cube (top right) turns the view to Front / Back / Left / Right / Top / Bottom");
        ImGui::TextUnformatted("While the game runs: WASD moves the player (the camera follows it)");
        ImGui::TextUnformatted("The camera and player only get the mouse while it is over the Viewport.");

        if (camera_.IsAttached())
        {
            const auto& position = camera_.Position();
            ImGui::Text("Camera (%.2f, %.2f, %.2f)  yaw %.0f  pitch %.0f  speed %.1f", position.x, position.y,
                        position.z, camera_.YawDegrees(), camera_.PitchDegrees(), camera_.Speed());
        }
        else
        {
            ImGui::TextDisabled("Camera: select a game to fly around its scene");
        }
    }

    // ---------------------------------------------------------------- Input (InputSystem)

    // Called every frame (even when the panel is closed) because pressed/released only last one frame -
    // if we only checked while drawing the panel we'd miss most of them
    void DiagnosticsPanel::RecordInputEvents()
    {
        for (const auto& [key, name] : kKeys)
        {
            if (input_.IsKeyPressed(key))
            {
                PushLine(inputLog_, std::string("IsKeyPressed(") + name + ")");
            }
            if (input_.IsKeyReleased(key))
            {
                PushLine(inputLog_, std::string("IsKeyReleased(") + name + ")");
            }
        }
        for (const auto& [button, name] : kMouseButtons)
        {
            if (input_.IsMouseButtonPressed(button))
            {
                PushLine(inputLog_, std::string("IsMouseButtonPressed(") + name + ")");
            }
            if (input_.IsMouseButtonReleased(button))
            {
                PushLine(inputLog_, std::string("IsMouseButtonReleased(") + name + ")");
            }
        }
        if (input_.GetScrollDelta() != 0.0f)
        {
            PushLine(inputLog_, "GetScrollDelta() = " + std::to_string(input_.GetScrollDelta()));
        }
        typedText_ += input_.GetTextInputBuffer();
        if (typedText_.size() > kMaxTypedText)
        {
            typedText_.erase(0, typedText_.size() - kMaxTypedText);
        }
    }

    // Live view of the raw InputSystem state, handy for checking keys and the editor/game input split
    void DiagnosticsPanel::DrawInput()
    {
        if (!ImGui::CollapsingHeader("InputSystem"))
        {
            return;
        }

        std::string held;
        for (const auto& [key, name] : kKeys)
        {
            if (input_.GetKey(key) == KeyState::Down)
            {
                held += held.empty() ? name : std::string(" ") + name;
            }
        }
        ImGui::TextWrapped("GetKey() held: %s", held.empty() ? "(none)" : held.c_str());
        ImGui::Text("Ctrl %s (L %s, R %s)   Shift %s (L %s, R %s)", YesNo(input_.IsCtrlDown()),
                    YesNo(input_.IsLeftCtrlDown()), YesNo(input_.IsRightCtrlDown()), YesNo(input_.IsShiftDown()),
                    YesNo(input_.IsLeftShiftDown()), YesNo(input_.IsRightShiftDown()));
        ImGui::Text("Alt %s (L %s, R %s)", YesNo(input_.IsAltDown()), YesNo(input_.IsLeftAltDown()),
                    YesNo(input_.IsRightAltDown()));

        const Vec2 mouse = input_.GetMousePosition();
        const Vec2 delta = input_.GetMouseDelta();
        ImGui::Text("Mouse (%.0f, %.0f)  moved (%.0f, %.0f)", mouse.x, mouse.y, delta.x, delta.y);
        ImGui::Text("Buttons: left %s, right %s, middle %s", YesNo(input_.IsLeftMouseDown()),
                    YesNo(input_.IsRightMouseDown()), YesNo(input_.IsMiddleMouseDown()));
        ImGui::TextWrapped("Typed: %s", typedText_.empty() ? "(type anywhere in the editor)" : typedText_.c_str());
        ImGui::Text("IsInputCaptured(): %s  (yes while typing in an editor text field)",
                    YesNo(input_.IsInputCaptured()));
        ImGui::Text("IsMouseCaptured(): %s  (yes while the mouse is over an editor panel)",
                    YesNo(input_.IsMouseCaptured()));

        if (ImGui::SmallButton("Clear"))
        {
            typedText_.clear();
            inputLog_.clear();
        }
        ImGui::SeparatorText("Events (newest first)");
        for (const auto& line : inputLog_)
        {
            ImGui::BulletText("%s", line.c_str());
        }
    }

    // ---------------------------------------------------------------- Game events (EventBus)

    // Subscribes to every game event; each one becomes a line in the Game events log (newest first)
    void DiagnosticsPanel::ConnectEvents(Engine::Core::EventBus& events)
    {
        namespace Ev = Engine::Events;
        const auto log = [this](std::string line)
        {
            eventLog_.push_front(std::move(line));
            if (eventLog_.size() > kMaxEventLines)
            {
                eventLog_.pop_back();
            }
        };

        subscriptions_.push_back(events.Subscribe<Ev::GameStarted>(
            [this, log](const Ev::GameStarted& e)
            {
                gameUpdates_ = 0;
                log("GameStarted '" + e.projectName + "' (" + std::to_string(e.objectCount) + " objects)");
            }));
        subscriptions_.push_back(
            events.Subscribe<Ev::GameStopped>([log](const Ev::GameStopped&) { log("GameStopped"); }));
        subscriptions_.push_back(events.Subscribe<Ev::SimulationChanged>(
            [log](const Ev::SimulationChanged& e) { log(e.simulating ? "Resumed (simulating)" : "Paused"); }));
        subscriptions_.push_back(events.Subscribe<Ev::ActionPressed>([log](const Ev::ActionPressed& e)
                                                                     { log("ActionPressed(" + e.action + ")"); }));
        subscriptions_.push_back(events.Subscribe<Ev::ActionReleased>([log](const Ev::ActionReleased& e)
                                                                      { log("ActionReleased(" + e.action + ")"); }));
        subscriptions_.push_back(events.Subscribe<Ev::WindowResized>(
            [log](const Ev::WindowResized& e)
            { log("WindowResized " + std::to_string(e.width) + " x " + std::to_string(e.height)); }));
        subscriptions_.push_back(events.Subscribe<Ev::MessageBroadcast>(
            [log](const Ev::MessageBroadcast& e)
            {
                log("MessageBroadcast '" + e.message + "' from " +
                    (e.fromBlocks ? "object " + std::to_string(e.senderObjectId) : std::string("engine code")) +
                    (e.value != 0.0 ? " with " + std::to_string(e.value) : std::string()));
            }));
        subscriptions_.push_back(events.Subscribe<Ev::CollisionBegan>(
            [log](const Ev::CollisionBegan& e)
            { log("CollisionBegan " + std::to_string(e.objectA) + " + " + std::to_string(e.objectB)); }));
        subscriptions_.push_back(events.Subscribe<Ev::CollisionEnded>(
            [log](const Ev::CollisionEnded& e)
            { log("CollisionEnded " + std::to_string(e.objectA) + " + " + std::to_string(e.objectB)); }));
        subscriptions_.push_back(events.Subscribe<Ev::TimerStarted>(
            [log](const Ev::TimerStarted& e)
            { log("TimerStarted '" + e.name + "' " + std::to_string(e.seconds) + " s"); }));
        subscriptions_.push_back(events.Subscribe<Ev::TimerElapsed>([log](const Ev::TimerElapsed& e)
                                                                    { log("TimerElapsed '" + e.name + "'"); }));
        subscriptions_.push_back(
            events.Subscribe<Ev::KeyPressed>([log](const Ev::KeyPressed& e) { log("KeyPressed(" + e.name + ")"); }));
        // Every frame, so counted instead of logged
        subscriptions_.push_back(events.Subscribe<Ev::GameUpdate>([this](const Ev::GameUpdate&) { ++gameUpdates_; }));
    }

    // The events the running game sends - the same ones block logic will listen to. Press Play to fill it.
    void DiagnosticsPanel::DrawGameEvents()
    {
        if (!ImGui::CollapsingHeader("Game events", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::Text("GameUpdate: %llu frames this run", static_cast<unsigned long long>(gameUpdates_));
        // ## gives this button its own ImGui id: the Input section above has a "Clear" button too
        if (ImGui::SmallButton("Clear##GameEvents"))
        {
            eventLog_.clear();
        }
        ImGui::SeparatorText("Events (newest first)");
        if (eventLog_.empty())
        {
            ImGui::TextDisabled("(none yet - press Play)");
        }
        for (const auto& line : eventLog_)
        {
            ImGui::BulletText("%s", line.c_str());
        }
    }
}
