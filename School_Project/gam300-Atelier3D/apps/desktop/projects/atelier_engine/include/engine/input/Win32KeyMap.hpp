/* !
@file    	    Win32KeyMap.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		28/09/2026
@brief
    Declares TranslateWin32Key, which converts Win32 key messages into the
    engine's key codes.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Input
{
    /**
     * @brief   Translates a Win32 key message into the GLFW key code InputSystem uses (see KeyCode in Input.hpp).
     *
     * Win32 reports Shift, Ctrl and Alt without saying which side; the side comes from the scan code (Shift) or the
     * "extended key" flag (Ctrl, Alt, and Enter on the numpad), which are bits 16-23 and 24 of the message's lParam.
     * Plain function with no Windows headers, so it builds and is unit-tested on every platform.
     *
     * @param   virtualKey  wParam of WM_KEYDOWN / WM_KEYUP (a VK_* code)
     * @param   scanCode    (lParam >> 16) & 0xFF
     * @param   extended    (lParam >> 24) & 1
     * @return  The GLFW key code, or -1 for keys InputSystem does not track
     */
    int TranslateWin32Key(unsigned int virtualKey, unsigned int scanCode, bool extended);
}
