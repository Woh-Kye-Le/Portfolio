/* !
@file    	    Window.hpp
@author  	    c.kojat@digipen.edu [96%]
         	    sanni.go@digipen.edu [4%]
@date    		22/09/2026
@brief
    Declares the platform-independent Window interface (window + OpenGL context
    + input forwarding) and CreateWindow, which picks the backend for the
    current platform.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Input
{
    class InputSystem;
}

namespace Engine::Platform
{
    // What window to open (size, title, ...). Games fill this in from game.cfg.
    struct WindowSpec
    {
        // Client area size at 100% display scale; on a scaled display (e.g. 150%) the window is made that much larger
        int width = 1280;
        int height = 720;
        std::string title = "Atelier3D";
        // Games close on Escape; the editor uses Escape for itself (cancelling, deselecting)
        bool closeOnEscape = true;
        // Opens filling the screen (keeping the title bar and taskbar), as the editor does; width / height are the
        // size it goes back to when restored
        bool maximized = false;
    };

    // The window + OpenGL context the engine draws into. WindowsWindow and GlfwWindow implement it,
    // and CreateWindow() picks the right one, so nothing else in the engine has platform #ifdefs for windows.
    class Window
    {
    public:
        virtual ~Window() = default;

        virtual void PollEvents() = 0;
        virtual void Present() = 0;
        virtual bool ShouldClose() const = 0;
        virtual void RequestClose() = 0;

        virtual int Width() const = 0;
        virtual int Height() const = 0;
        virtual const std::string& Title() const = 0;

        // Platform window handle (GLFWwindow* on non-Windows), or nullptr if there is none
        virtual void* NativeHandle() const { return nullptr; }

        // Sends this window's keyboard, mouse and text events to input; nullptr disconnects it again. Whoever owns the
        // InputSystem must disconnect before destroying it. Windows without real input ignore this.
        virtual void AttachInput(Input::InputSystem* /*input*/) {}
        // Called when the window needs redrawing outside the main loop (e.g. while it is being resized)
        virtual void SetRefreshCallback(std::function<void()> /*callback*/) {}
        // Asked when the user closes the window (title bar X, Alt+F4): returning false keeps it open (the editor asks
        // about unsaved changes first). RequestClose() always closes.
        virtual void SetCloseCallback(std::function<bool()> /*canClose*/) {}

        // The display's scale factor (1 at 96 DPI / 100%, 1.5 at 150%). Width() / Height() are real pixels, so UI
        // sizes should be multiplied by this to look the same size on every display.
        virtual float ContentScale() const { return 1.0f; }

        // Borderless fullscreen on the window's monitor, and back to the normal window
        virtual void SetFullscreen(bool /*fullscreen*/) {}
        virtual bool IsFullscreen() const { return false; }
        // The size of what the window shows, in pixels (its resolution), its frame and title bar around it; a
        // fullscreen window keeps filling the screen
        virtual void SetClientSize(int /*width*/, int /*height*/) {}

        // Files dragged from the desktop's file manager and dropped on the window since the last call (the editor
        // imports them); empty if none, or if the platform has no file dropping
        virtual std::vector<std::filesystem::path> TakeDroppedFiles() { return {}; }

        // For games that turn with the mouse: hides the pointer and keeps it in the window, while mouse movement is
        // still reported (with no edge to stop it). false gives the pointer back. A window that loses focus gives it
        // back by itself (IsCursorCaptured says so); one that does not have focus does not take it.
        virtual void SetCursorCaptured(bool /*captured*/) {}
        virtual bool IsCursorCaptured() const { return false; }
    };

    std::unique_ptr<Window> CreateWindow(const WindowSpec& spec);
}
