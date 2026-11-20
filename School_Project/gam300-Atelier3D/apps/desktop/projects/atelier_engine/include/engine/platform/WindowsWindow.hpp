/* !
@file    	    WindowsWindow.hpp
@author  	    c.kojat@digipen.edu [96%]
         	    sanni.go@digipen.edu [4%]
@date    		22/09/2026
@brief
    Declares WindowsWindow, the Win32 window with a WGL OpenGL context, DPI
    awareness, fullscreen and message forwarding to ImGui and the input system.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/platform/Window.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// windows.h #defines CreateWindow to CreateWindowW, which breaks our Platform::CreateWindow
#ifdef CreateWindow
#undef CreateWindow
#endif

namespace Engine::Platform
{
    // The Windows window: a plain Win32 window with a WGL OpenGL context. We use this instead of GLFW on
    // Windows so we get the raw messages (needed for ImGui and our own input handling).
    class WindowsWindow final : public Window
    {
    public:
        explicit WindowsWindow(WindowSpec spec);
        ~WindowsWindow() override;

        void PollEvents() override;
        void Present() override;
        bool ShouldClose() const override;
        void RequestClose() override;

        int Width() const override;
        int Height() const override;
        const std::string& Title() const override;
        void SetRefreshCallback(std::function<void()> callback) override { refresh_ = std::move(callback); }
        void SetCloseCallback(std::function<bool()> canClose) override { canClose_ = std::move(canClose); }

        void AttachInput(Input::InputSystem* input) override;

        float ContentScale() const override;
        void SetFullscreen(bool fullscreen) override;
        bool IsFullscreen() const override { return fullscreen_; }
        void SetClientSize(int width, int height) override;
        std::vector<std::filesystem::path> TakeDroppedFiles() override;
        void SetCursorCaptured(bool captured) override;
        bool IsCursorCaptured() const override { return cursorCaptured_; }

    private:
        // A captured pointer is held on the middle of the window (a 1 px clip), so it can never leave it
        void ClipCursorToCentre() const;
        // Feeds keyboard, mouse and text messages to input_ (if attached)
        void ForwardInput(UINT msg, WPARAM wParam, LPARAM lParam);

        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
        // Win32 wants a plain function, so this looks up the WindowsWindow stored on the HWND and calls HandleMessage
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void CreateNativeWindow();
        void CreateOpenGLContext();

        WindowSpec spec_;
        bool shouldClose_ = false;
        Input::InputSystem* input_ = nullptr;
        std::function<void()> refresh_;
        std::function<bool()> canClose_;

        HWND hwnd_ = nullptr;
        // While the pointer is captured, mouse movement comes from raw input (WM_INPUT), added up here as a pointer
        // position with no edges
        bool cursorCaptured_ = false;
        float rawX_ = 0.0f;
        float rawY_ = 0.0f;
        HDC hdc_ = nullptr;
        HGLRC hglrc_ = nullptr;
        bool pacedByCompositor_ = false; // Present waits for the desktop compositor in place of the driver's vsync

        bool fullscreen_ = false;
        std::vector<std::filesystem::path> dropped_;                 // WM_DROPFILES since the last TakeDroppedFiles
        WINDOWPLACEMENT windowedPlacement_{sizeof(WINDOWPLACEMENT)}; // where to go back to from fullscreen
    };
}
