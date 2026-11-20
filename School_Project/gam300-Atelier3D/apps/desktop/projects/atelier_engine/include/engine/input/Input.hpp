/* !
@file    	    Input.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		27/09/2026
@brief
    Input system following the GLFW input model. Provides keyboard and mouse
    input handling.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstring>
#include <string>

namespace Engine::Input
{
    // Released or pressed
    enum class KeyState : std::uint8_t
    {
        Up,
        Down,
    };

    struct Vec2
    {
        float x = 0.f;
        float y = 0.f;
    };

    /**
     * @brief   Modifier keys enumeration for keyboard input handling.
     *
     * These keys are used to modify the behavior of other keys when pressed in combination. They are commonly used for
     * shortcuts and special commands in applications.
     */
    enum KeyCode : int
    {
        // Letters
        Key_A = GLFW_KEY_A,
        Key_B = GLFW_KEY_B,
        Key_C = GLFW_KEY_C,
        Key_D = GLFW_KEY_D,
        Key_E = GLFW_KEY_E,
        Key_F = GLFW_KEY_F,
        Key_G = GLFW_KEY_G,
        Key_H = GLFW_KEY_H,
        Key_I = GLFW_KEY_I,
        Key_J = GLFW_KEY_J,
        Key_K = GLFW_KEY_K,
        Key_L = GLFW_KEY_L,
        Key_M = GLFW_KEY_M,
        Key_N = GLFW_KEY_N,
        Key_O = GLFW_KEY_O,
        Key_P = GLFW_KEY_P,
        Key_Q = GLFW_KEY_Q,
        Key_R = GLFW_KEY_R,
        Key_S = GLFW_KEY_S,
        Key_T = GLFW_KEY_T,
        Key_U = GLFW_KEY_U,
        Key_V = GLFW_KEY_V,
        Key_W = GLFW_KEY_W,
        Key_X = GLFW_KEY_X,
        Key_Y = GLFW_KEY_Y,
        Key_Z = GLFW_KEY_Z,

        // Numbers (top row)
        Key_0 = GLFW_KEY_0,
        Key_1 = GLFW_KEY_1,
        Key_2 = GLFW_KEY_2,
        Key_3 = GLFW_KEY_3,
        Key_4 = GLFW_KEY_4,
        Key_5 = GLFW_KEY_5,
        Key_6 = GLFW_KEY_6,
        Key_7 = GLFW_KEY_7,
        Key_8 = GLFW_KEY_8,
        Key_9 = GLFW_KEY_9,

        // Function keys
        Key_F1 = GLFW_KEY_F1,
        Key_F2 = GLFW_KEY_F2,
        Key_F3 = GLFW_KEY_F3,
        Key_F4 = GLFW_KEY_F4,
        Key_F5 = GLFW_KEY_F5,
        Key_F6 = GLFW_KEY_F6,
        Key_F7 = GLFW_KEY_F7,
        Key_F8 = GLFW_KEY_F8,
        Key_F9 = GLFW_KEY_F9,
        Key_F10 = GLFW_KEY_F10,
        Key_F11 = GLFW_KEY_F11,
        Key_F12 = GLFW_KEY_F12,

        // Navigation / editing
        Key_Space = GLFW_KEY_SPACE,
        Key_Enter = GLFW_KEY_ENTER,
        Key_Escape = GLFW_KEY_ESCAPE,
        Key_Tab = GLFW_KEY_TAB,
        Key_Backspace = GLFW_KEY_BACKSPACE,
        Key_Delete = GLFW_KEY_DELETE,
        Key_Insert = GLFW_KEY_INSERT,
        Key_Home = GLFW_KEY_HOME,
        Key_End = GLFW_KEY_END,
        Key_PageUp = GLFW_KEY_PAGE_UP,
        Key_PageDown = GLFW_KEY_PAGE_DOWN,

        // Arrows
        Key_Up = GLFW_KEY_UP,
        Key_Down = GLFW_KEY_DOWN,
        Key_Left = GLFW_KEY_LEFT,
        Key_Right = GLFW_KEY_RIGHT,

        // Numpad
        Key_NumPad0 = GLFW_KEY_KP_0,
        Key_NumPad1 = GLFW_KEY_KP_1,
        Key_NumPad2 = GLFW_KEY_KP_2,
        Key_NumPad3 = GLFW_KEY_KP_3,
        Key_NumPad4 = GLFW_KEY_KP_4,
        Key_NumPad5 = GLFW_KEY_KP_5,
        Key_NumPad6 = GLFW_KEY_KP_6,
        Key_NumPad7 = GLFW_KEY_KP_7,
        Key_NumPad8 = GLFW_KEY_KP_8,
        Key_NumPad9 = GLFW_KEY_KP_9,
        Key_NumPadEnter = GLFW_KEY_KP_ENTER,
        Key_NumPadAdd = GLFW_KEY_KP_ADD,
        Key_NumPadSubtract = GLFW_KEY_KP_SUBTRACT,
        Key_NumPadMultiply = GLFW_KEY_KP_MULTIPLY,
        Key_NumPadDivide = GLFW_KEY_KP_DIVIDE,

        // Punctuation
        Key_Comma = GLFW_KEY_COMMA,
        Key_Period = GLFW_KEY_PERIOD,
        Key_Slash = GLFW_KEY_SLASH,
        Key_Semicolon = GLFW_KEY_SEMICOLON,
        Key_Apostrophe = GLFW_KEY_APOSTROPHE,
        Key_LeftBracket = GLFW_KEY_LEFT_BRACKET,
        Key_RightBracket = GLFW_KEY_RIGHT_BRACKET,
        Key_Backslash = GLFW_KEY_BACKSLASH,
        Key_Minus = GLFW_KEY_MINUS,
        Key_Equal = GLFW_KEY_EQUAL,
        Key_GraveAccent = GLFW_KEY_GRAVE_ACCENT,

        // Lock keys
        Key_CapsLock = GLFW_KEY_CAPS_LOCK,
        Key_NumLock = GLFW_KEY_NUM_LOCK,
        Key_ScrollLock = GLFW_KEY_SCROLL_LOCK,

        // Modifier keys
        Key_LeftCtrl = GLFW_KEY_LEFT_CONTROL,
        Key_RightCtrl = GLFW_KEY_RIGHT_CONTROL,
        Key_LeftShift = GLFW_KEY_LEFT_SHIFT,
        Key_RightShift = GLFW_KEY_RIGHT_SHIFT,
        Key_LeftAlt = GLFW_KEY_LEFT_ALT,
        Key_RightAlt = GLFW_KEY_RIGHT_ALT,
    };

    /**
     * @brief   Mouse button enumeration for mouse input handling.
     *
     * Mouse buttons are used to interact with graphical user interfaces and perform various actions in applications.
     */
    enum MouseButton : int
    {
        Mouse_Left = GLFW_MOUSE_BUTTON_LEFT,     // 0
        Mouse_Right = GLFW_MOUSE_BUTTON_RIGHT,   // 1
        Mouse_Middle = GLFW_MOUSE_BUTTON_MIDDLE, // 2
    };

    /**
     * @class   InputSystem
     *
     * Input system class that manages keyboard and mouse input states. It provides methods to set and query the state
     * of keys and mouse buttons, as well as to check for modifier key states.
     */
    class InputSystem
    {
    public:
        // ---- Keyboard ----
        void SetKey(int keyCode, KeyState state)
        {
            if (keyCode < 0 || keyCode >= MaxKeys)
                return;
            keys_[keyCode] = state;
        }

        // KeyCode input as (Key_W) etc.
        KeyState GetKey(int keyCode) const
        {
            if (keyCode < 0 || keyCode >= MaxKeys)
                return KeyState::Up;
            return keys_[keyCode];
        }

        bool IsKeyPressed(int keyCode) const // true only on the frame it went down
        {
            if (keyCode < 0 || keyCode >= MaxKeys)
                return false;
            return keys_[keyCode] == KeyState::Down && prevKeys_[keyCode] == KeyState::Up;
        }

        bool IsKeyReleased(int keyCode) const // true only on the frame it went up
        {
            if (keyCode < 0 || keyCode >= MaxKeys)
                return false;
            return keys_[keyCode] == KeyState::Up && prevKeys_[keyCode] == KeyState::Down;
        }

        // ---- Modifiers (combined - either side) ----
        bool IsCtrlDown() const
        {
            return GetKey(Key_LeftCtrl) == KeyState::Down || GetKey(Key_RightCtrl) == KeyState::Down;
        }

        bool IsShiftDown() const
        {
            return GetKey(Key_LeftShift) == KeyState::Down || GetKey(Key_RightShift) == KeyState::Down;
        }

        bool IsAltDown() const
        {
            return GetKey(Key_LeftAlt) == KeyState::Down || GetKey(Key_RightAlt) == KeyState::Down;
        }

        // ---- Modifiers (side-specific) ----
        bool IsLeftCtrlDown() const { return GetKey(Key_LeftCtrl) == KeyState::Down; }
        bool IsRightCtrlDown() const { return GetKey(Key_RightCtrl) == KeyState::Down; }
        bool IsLeftShiftDown() const { return GetKey(Key_LeftShift) == KeyState::Down; }
        bool IsRightShiftDown() const { return GetKey(Key_RightShift) == KeyState::Down; }
        bool IsLeftAltDown() const { return GetKey(Key_LeftAlt) == KeyState::Down; }
        bool IsRightAltDown() const { return GetKey(Key_RightAlt) == KeyState::Down; }

        // ---- Mouse ----
        void SetMouseButton(int button, KeyState state)
        {
            if (button < 0 || button >= MaxMouseButtons)
                return;
            mouseButtons_[button] = state;
        }

        KeyState GetMouseButton(int button) const
        {
            if (button < 0 || button >= MaxMouseButtons)
                return KeyState::Up;
            return mouseButtons_[button];
        }

        bool IsMouseButtonDown(int button) const { return GetMouseButton(button) == KeyState::Down; }

        bool IsMouseButtonPressed(int button) const // true only on the frame it went down
        {
            if (button < 0 || button >= MaxMouseButtons)
                return false;
            return mouseButtons_[button] == KeyState::Down && prevMouseButtons_[button] == KeyState::Up;
        }

        bool IsMouseButtonReleased(int button) const // true only on the frame it went up
        {
            if (button < 0 || button >= MaxMouseButtons)
                return false;
            return mouseButtons_[button] == KeyState::Up && prevMouseButtons_[button] == KeyState::Down;
        }

        bool IsLeftMouseDown() const { return IsMouseButtonDown(Mouse_Left); }
        bool IsRightMouseDown() const { return IsMouseButtonDown(Mouse_Right); }
        bool IsMiddleMouseDown() const { return IsMouseButtonDown(Mouse_Middle); }

        void SetMousePosition(float x, float y) { mousePos_ = {x, y}; }

        Vec2 GetMousePosition() const { return mousePos_; }

        Vec2 GetMouseDelta() const { return {mousePos_.x - prevMousePos_.x, mousePos_.y - prevMousePos_.y}; }

        void SetScrollDelta(float delta) { scrollDelta_ = delta; }

        float GetScrollDelta() const { return scrollDelta_; }

        // ---- Text input ----
        void AddTextInput(unsigned int codepoint)
        {
            // Basic ASCII append; extend to UTF-8 encoding if you need full Unicode.
            if (codepoint < 128)
                textBuffer_.push_back(static_cast<char>(codepoint));
        }

        std::string GetTextInputBuffer() const { return textBuffer_; }

        void ClearTextInput() { textBuffer_.clear(); }

        /**
         * @brief   Sets whether the keyboard is captured by a UI element (e.g. a text field),
         *          gating it away from the rest of the application (e.g. the viewport).
         *          GameInput reports no key bindings while this is set.
         */
        void SetCaptureInput(bool capture) { captureInput_ = capture; }

        bool IsInputCaptured() const { return captureInput_; }

        /**
         * @brief   Sets whether the mouse is captured by a UI element (e.g. the pointer is over an editor panel).
         *          GameInput reports no mouse buttons, movement or scroll while this is set.
         */
        void SetCaptureMouse(bool capture) { captureMouse_ = capture; }

        bool IsMouseCaptured() const { return captureMouse_; }

        /**
         * @brief   Sets whether the game has the pointer locked (hidden and held in the window, Window::
         *          SetCursorCaptured): the mouse then turns the player without a button held.
         */
        void SetPointerLocked(bool locked) { pointerLocked_ = locked; }

        bool IsPointerLocked() const { return pointerLocked_; }

        /**
         * @brief   Advances internal frame state. Call once per frame, after all input for
         *          the frame has been dispatched, and before polling begins for the next frame.
         */
        void Update()
        {
            std::memcpy(prevKeys_, keys_, sizeof(keys_));
            std::memcpy(prevMouseButtons_, mouseButtons_, sizeof(mouseButtons_));
            prevMousePos_ = mousePos_;
            scrollDelta_ = 0.f;
            textBuffer_.clear();
        }

    private:
        static constexpr int MaxKeys = 512;
        KeyState keys_[MaxKeys]{};
        KeyState prevKeys_[MaxKeys]{};

        static constexpr int MaxMouseButtons = 8;
        KeyState mouseButtons_[MaxMouseButtons]{};
        KeyState prevMouseButtons_[MaxMouseButtons]{};
        Vec2 mousePos_{};
        Vec2 prevMousePos_{};
        float scrollDelta_ = 0.f;

        std::string textBuffer_;
        bool captureInput_ = false;
        bool captureMouse_ = false;
        bool pointerLocked_ = false;
    };

    // Matching declaration for the RegisterCallbacks function, which sets up GLFW callbacks to handle input events and
    // update the InputSystem accordingly.
    void RegisterCallbacks(GLFWwindow* window, InputSystem& input);
}
