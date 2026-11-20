/* !
@file    	    Window.cpp
@author  	    c.kojat@digipen.edu [96%]
         	    sanni.go@digipen.edu [4%]
@date    		30/09/2026
@brief
    Implements CreateWindow: a WindowsWindow on Windows, a GlfwWindow everywhere
    else.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/platform/Window.hpp"

#ifdef _WIN32
#include "engine/platform/WindowsWindow.hpp"
#else
#include "engine/platform/GlfwWindow.hpp"
#endif

namespace Engine::Platform
{
    // Picks the window backend for the platform we're building on: plain Win32 on Windows,
    // GLFW (with X11) on Linux. The rest of the engine only talks to the Window interface.
    std::unique_ptr<Window> CreateWindow(const WindowSpec& spec)
    {
#ifdef _WIN32
        return std::make_unique<WindowsWindow>(spec);
#else
        return std::make_unique<GlfwWindow>(spec);
#endif
    }
}
