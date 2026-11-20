/* !
@file    	    GameInput.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		29/09/2026
@brief
    Declares GameInput, the game's view of input: named actions and mouse
    movement that report nothing while the editor UI has the keyboard or mouse.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/input/Input.hpp"
#include "engine/input/InputActions.hpp"

#include <cstdint>
#include <string_view>

namespace Engine::Input
{
    // What game code (and the play-mode camera) sees of the input: named actions from input_actions.json and mouse
    // movement. It is how editor and game input are kept apart - while the editor UI has the keyboard (typing in a
    // text field, InputSystem::IsInputCaptured) no key binding is reported, and while it has the mouse (pointer over
    // a panel, InputSystem::IsMouseCaptured) no mouse button, movement or scroll is reported. Game code only gets a
    // GameInput, so it cannot forget the check. Cheap to make: build one per frame.
    class GameInput
    {
    public:
        GameInput(const InputSystem& input, const InputActions& actions) : input_(input), actions_(actions) {}

        // Unknown actions are never down, pressed or released.
        // Down: any binding is held. Pressed / Released: any binding went down / up this frame.
        bool IsActionDown(std::string_view action) const;
        bool IsActionPressed(std::string_view action) const;
        bool IsActionReleased(std::string_view action) const;

        // A key or mouse button by code (KeyCode / MouseButton), for controls that are not actions (e.g. the editor
        // camera's Alt + drag orbit). Never down while the editor UI has that device.
        bool IsKeyDown(int keyCode) const { return !IsKeyboardBlocked() && input_.GetKey(keyCode) == KeyState::Down; }
        // True only on the frame the key went down
        bool IsKeyPressed(int keyCode) const { return !IsKeyboardBlocked() && input_.IsKeyPressed(keyCode); }
        bool IsMouseButtonDown(int button) const { return !IsMouseBlocked() && input_.IsMouseButtonDown(button); }
        bool IsMouseButtonPressed(int button) const { return !IsMouseBlocked() && input_.IsMouseButtonPressed(button); }
        bool IsKeyReleased(int code) const { return !IsKeyboardBlocked() && input_.IsKeyReleased(code); }
        bool IsMouseButtonReleased(int code) const { return !IsMouseBlocked() && input_.IsMouseButtonReleased(code); }
        bool IsAltDown() const { return !IsKeyboardBlocked() && input_.IsAltDown(); }

        // Zero while the editor UI has the mouse
        Vec2 GetMouseDelta() const;
        float GetScrollDelta() const;
        // Where the pointer is, even over the editor UI (so games can still draw a cursor)
        Vec2 GetMousePosition() const { return input_.GetMousePosition(); }

        bool IsKeyboardBlocked() const { return input_.IsInputCaptured(); }
        bool IsMouseBlocked() const { return input_.IsMouseCaptured(); }
        // The game has the pointer (InputSystem::SetPointerLocked): the mouse looks around with no button held
        bool IsPointerLocked() const { return input_.IsPointerLocked(); }

    private:
        // which of the three IsAction* functions is asking, so they can share Check()
        enum class Query : std::uint8_t
        {
            Down,
            Pressed,
            Released
        };

        bool Check(std::string_view action, Query query) const;

        const InputSystem& input_;
        const InputActions& actions_;
    };
}
