/* !
@file    	    ImGuiLayer.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		22/09/2026
@brief
    Declares ImGuiLayer, which sets up Dear ImGui for the editor and wraps each
    UI frame.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/platform/Window.hpp"

#include <cstdint>

namespace Engine::ImGuiLayer
{
    // Sets up Dear ImGui for the editor (context, fonts, OpenGL backend, docking).
    // Call BeginFrame before any ImGui:: calls in a frame and EndFrame after, which draws it all.
    class ImGuiLayer
    {
    public:
        void Init(Platform::Window& window);
        void Shutdown();

        void BeginFrame(Platform::Window& window, double deltaSeconds);
        void EndFrame();
    };

#ifdef _WIN32
    // The Win32 window forwards its messages here so ImGui gets keyboard/mouse input.
    // Returns true if ImGui used the message.
    bool HandleWin32Message(void* hwnd, unsigned int msg, std::uintptr_t wParam, std::intptr_t lParam);
#endif
}
