/* !
@file    	    Win32KeyMap.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		28/09/2026
@brief
    Implements TranslateWin32Key, including telling left/right modifier keys and
    numpad Enter apart.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/input/Win32KeyMap.hpp"

#include "engine/input/Input.hpp"

namespace Engine::Input
{
    namespace
    {
        // Win32 virtual-key codes (values from WinUser.h), spelled out so this file needs no Windows headers
        constexpr unsigned int VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_SHIFT = 0x10, VK_CONTROL = 0x11,
                               VK_MENU = 0x12, VK_CAPITAL = 0x14, VK_ESCAPE = 0x1B, VK_SPACE = 0x20, VK_PRIOR = 0x21,
                               VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24, VK_LEFT = 0x25, VK_UP = 0x26,
                               VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_INSERT = 0x2D, VK_DELETE = 0x2E, VK_NUMPAD0 = 0x60,
                               VK_NUMPAD9 = 0x69, VK_MULTIPLY = 0x6A, VK_ADD = 0x6B, VK_SUBTRACT = 0x6D,
                               VK_DIVIDE = 0x6F, VK_F1 = 0x70, VK_F12 = 0x7B, VK_NUMLOCK = 0x90, VK_SCROLL = 0x91,
                               VK_LSHIFT = 0xA0, VK_RSHIFT = 0xA1, VK_LCONTROL = 0xA2, VK_RCONTROL = 0xA3,
                               VK_LMENU = 0xA4, VK_RMENU = 0xA5, VK_OEM_1 = 0xBA, VK_OEM_PLUS = 0xBB,
                               VK_OEM_COMMA = 0xBC, VK_OEM_MINUS = 0xBD, VK_OEM_PERIOD = 0xBE, VK_OEM_2 = 0xBF,
                               VK_OEM_3 = 0xC0, VK_OEM_4 = 0xDB, VK_OEM_5 = 0xDC, VK_OEM_6 = 0xDD, VK_OEM_7 = 0xDE;

        // Scan code of the right Shift key (left Shift is 0x2A); Shift is never an "extended" key
        constexpr unsigned int kRightShiftScanCode = 0x36;
    }

    // Win32 virtual key codes -> our (GLFW-style) key codes, so input works the same on both platforms
    int TranslateWin32Key(unsigned int virtualKey, unsigned int scanCode, bool extended)
    {
        // Letters and top-row digits share their ASCII codes in both Win32 and GLFW
        if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9'))
        {
            return static_cast<int>(virtualKey);
        }
        if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
        {
            return Key_F1 + static_cast<int>(virtualKey - VK_F1);
        }
        if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
        {
            return Key_NumPad0 + static_cast<int>(virtualKey - VK_NUMPAD0);
        }

        switch (virtualKey)
        {
        // Modifiers: generic codes are split into left/right
        case VK_SHIFT: return scanCode == kRightShiftScanCode ? Key_RightShift : Key_LeftShift;
        case VK_CONTROL: return extended ? Key_RightCtrl : Key_LeftCtrl;
        case VK_MENU: return extended ? Key_RightAlt : Key_LeftAlt;
        case VK_LSHIFT: return Key_LeftShift;
        case VK_RSHIFT: return Key_RightShift;
        case VK_LCONTROL: return Key_LeftCtrl;
        case VK_RCONTROL: return Key_RightCtrl;
        case VK_LMENU: return Key_LeftAlt;
        case VK_RMENU: return Key_RightAlt;

        // Navigation / editing
        case VK_SPACE: return Key_Space;
        case VK_RETURN: return extended ? Key_NumPadEnter : Key_Enter;
        case VK_ESCAPE: return Key_Escape;
        case VK_TAB: return Key_Tab;
        case VK_BACK: return Key_Backspace;
        case VK_DELETE: return Key_Delete;
        case VK_INSERT: return Key_Insert;
        case VK_HOME: return Key_Home;
        case VK_END: return Key_End;
        case VK_PRIOR: return Key_PageUp;
        case VK_NEXT: return Key_PageDown;

        // Arrows
        case VK_UP: return Key_Up;
        case VK_DOWN: return Key_Down;
        case VK_LEFT: return Key_Left;
        case VK_RIGHT: return Key_Right;

        // Numpad operators
        case VK_ADD: return Key_NumPadAdd;
        case VK_SUBTRACT: return Key_NumPadSubtract;
        case VK_MULTIPLY: return Key_NumPadMultiply;
        case VK_DIVIDE: return Key_NumPadDivide;

        // Punctuation (US layout positions)
        case VK_OEM_COMMA: return Key_Comma;
        case VK_OEM_PERIOD: return Key_Period;
        case VK_OEM_2: return Key_Slash;
        case VK_OEM_1: return Key_Semicolon;
        case VK_OEM_7: return Key_Apostrophe;
        case VK_OEM_4: return Key_LeftBracket;
        case VK_OEM_6: return Key_RightBracket;
        case VK_OEM_5: return Key_Backslash;
        case VK_OEM_MINUS: return Key_Minus;
        case VK_OEM_PLUS: return Key_Equal;
        case VK_OEM_3: return Key_GraveAccent;

        // Lock keys
        case VK_CAPITAL: return Key_CapsLock;
        case VK_NUMLOCK: return Key_NumLock;
        case VK_SCROLL: return Key_ScrollLock;

        default: return -1;
        }
    }
}
