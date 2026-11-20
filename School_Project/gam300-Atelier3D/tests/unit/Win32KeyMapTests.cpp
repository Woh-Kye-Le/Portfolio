#include "engine/input/Input.hpp"
#include "engine/input/Win32KeyMap.hpp"

#include <gtest/gtest.h>

using namespace Engine::Input;

// Win32 virtual-key codes used below (WinUser.h values); the translator has no Windows dependency
namespace
{
    constexpr unsigned int kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12, kVkReturn = 0x0D, kVkF1 = 0x70,
                           kVkF12 = 0x7B, kVkNumpad0 = 0x60, kVkNumpad9 = 0x69, kVkLeft = 0x25, kVkUp = 0x26,
                           kVkRight = 0x27, kVkDown = 0x28, kVkSpace = 0x20, kVkEscape = 0x1B, kVkOemComma = 0xBC,
                           kVkOemMinus = 0xBD, kVkOem3 = 0xC0, kVkAdd = 0x6B, kVkCapital = 0x14, kVkLWin = 0x5B;
    constexpr unsigned int kLeftShiftScan = 0x2A, kRightShiftScan = 0x36;

    int Translate(unsigned int vk, unsigned int scan = 0, bool extended = false)
    {
        return TranslateWin32Key(vk, scan, extended);
    }
}

// Letters and the top-row digits have the same codes in Win32 and GLFW.
TEST(Win32KeyMap, LettersAndDigitsKeepTheirCodes)
{
    EXPECT_EQ(Translate('A'), Key_A);
    EXPECT_EQ(Translate('W'), Key_W);
    EXPECT_EQ(Translate('Z'), Key_Z);
    EXPECT_EQ(Translate('0'), Key_0);
    EXPECT_EQ(Translate('9'), Key_9);
}

// F-keys and the numpad keys map across.
TEST(Win32KeyMap, FunctionKeysAndNumpad)
{
    EXPECT_EQ(Translate(kVkF1), Key_F1);
    EXPECT_EQ(Translate(kVkF12), Key_F12);
    EXPECT_EQ(Translate(kVkNumpad0), Key_NumPad0);
    EXPECT_EQ(Translate(kVkNumpad9), Key_NumPad9);
    EXPECT_EQ(Translate(kVkAdd), Key_NumPadAdd);
}

// Arrow keys, Space and Escape map across.
TEST(Win32KeyMap, ArrowsAndEditingKeys)
{
    EXPECT_EQ(Translate(kVkLeft), Key_Left);
    EXPECT_EQ(Translate(kVkUp), Key_Up);
    EXPECT_EQ(Translate(kVkRight), Key_Right);
    EXPECT_EQ(Translate(kVkDown), Key_Down);
    EXPECT_EQ(Translate(kVkSpace), Key_Space);
    EXPECT_EQ(Translate(kVkEscape), Key_Escape);
}

// Both Enters send the same virtual key; the extended bit says which one it was.
TEST(Win32KeyMap, EnterAndNumpadEnterAreToldApartByTheExtendedFlag)
{
    EXPECT_EQ(Translate(kVkReturn, 0x1C, false), Key_Enter);
    EXPECT_EQ(Translate(kVkReturn, 0x1C, true), Key_NumPadEnter);
}

// Win32 only says "Shift", so the scan code is used to tell left from right.
TEST(Win32KeyMap, ShiftSideComesFromTheScanCode)
{
    EXPECT_EQ(Translate(kVkShift, kLeftShiftScan), Key_LeftShift);
    EXPECT_EQ(Translate(kVkShift, kRightShiftScan), Key_RightShift);
}

// For Ctrl and Alt the right-hand key is the "extended" one.
TEST(Win32KeyMap, CtrlAndAltSideComesFromTheExtendedFlag)
{
    EXPECT_EQ(Translate(kVkControl, 0x1D, false), Key_LeftCtrl);
    EXPECT_EQ(Translate(kVkControl, 0x1D, true), Key_RightCtrl);
    EXPECT_EQ(Translate(kVkMenu, 0x38, false), Key_LeftAlt);
    EXPECT_EQ(Translate(kVkMenu, 0x38, true), Key_RightAlt);
}

// The OEM punctuation keys and Caps Lock map across.
TEST(Win32KeyMap, PunctuationAndLocks)
{
    EXPECT_EQ(Translate(kVkOemComma), Key_Comma);
    EXPECT_EQ(Translate(kVkOemMinus), Key_Minus);
    EXPECT_EQ(Translate(kVkOem3), Key_GraveAccent);
    EXPECT_EQ(Translate(kVkCapital), Key_CapsLock);
}

// Keys we do not track (like the Windows key) give -1 instead of some random code.
TEST(Win32KeyMap, UntrackedKeysGiveMinusOne)
{
    EXPECT_EQ(Translate(kVkLWin), -1); // Windows key
    EXPECT_EQ(Translate(0x00), -1);
    EXPECT_EQ(Translate(0xFF), -1);
}

// End to end: a translated right Shift shows up in InputSystem as right Shift, and -1 is ignored.
TEST(Win32KeyMap, TranslatedKeysDriveTheInputSystem)
{
    InputSystem input;
    input.SetKey(Translate(kVkShift, kRightShiftScan), KeyState::Down);
    input.SetKey(Translate(kVkLWin), KeyState::Down); // -1 is ignored by InputSystem

    EXPECT_TRUE(input.IsRightShiftDown());
    EXPECT_FALSE(input.IsLeftShiftDown());
}
