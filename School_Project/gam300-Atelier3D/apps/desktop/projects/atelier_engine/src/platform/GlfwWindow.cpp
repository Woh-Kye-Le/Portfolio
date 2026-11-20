/* !
@file    	    GlfwWindow.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		25/09/2026
@brief
    Implements GlfwWindow: GLFW initialisation, window and context creation,
    input callback hookup, resize tracking and closing on Escape.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/platform/GlfwWindow.hpp"

#include "engine/core/Log.hpp"
#include "engine/input/Input.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace Engine::Platform
{
    namespace
    {
        void ErrorCallback(int code, const char* description)
        {
            LOG_ERROR(Render, "GLFW error {}: {}", code, description);
        }

        // GLFW holds a captured pointer by moving it back to the middle of the window, and measures each movement from
        // there. WSL's own display (WSLg) ignores those moves, so every small movement would count as the whole distance
        // from the middle and send the view spinning. An X server on another machine (no ':' first) can move it as usual.
        bool CanHoldPointer()
        {
            const char* display = std::getenv("DISPLAY");
            std::error_code ec;
            return !(display && display[0] == ':' && std::filesystem::exists("/mnt/wslg", ec));
        }

        // The monitor showing the most of the window (GLFW only says which one a fullscreen window is on)
        GLFWmonitor* MonitorOf(GLFWwindow* window)
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            glfwGetWindowPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);

            GLFWmonitor* best = glfwGetPrimaryMonitor();
            int bestArea = 0;
            int count = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            for (int i = 0; i < count; ++i)
            {
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                if (!mode)
                {
                    continue;
                }
                int left = 0;
                int top = 0;
                glfwGetMonitorPos(monitors[i], &left, &top);
                const int across = std::min(x + width, left + mode->width) - std::max(x, left);
                const int down = std::min(y + height, top + mode->height) - std::max(y, top);
                if (across > 0 && down > 0 && across * down > bestArea)
                {
                    bestArea = across * down;
                    best = monitors[i];
                }
            }
            return best;
        }
    }

    // Opens the window and makes its OpenGL context current. Throws if GLFW can't do it
    GlfwWindow::GlfwWindow(WindowSpec spec) : spec_(std::move(spec))
    {
        glfwSetErrorCallback(&ErrorCallback);
        if (!glfwInit())
        {
            throw std::runtime_error("glfwInit failed");
        }

        // 24-bit depth for the 3D scene, stencil for the selection outline
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_MAXIMIZED, spec_.maximized ? GLFW_TRUE : GLFW_FALSE);

        window_ = glfwCreateWindow(spec_.width, spec_.height, spec_.title.c_str(), nullptr, nullptr);
        if (!window_)
        {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }

        // so the static callbacks can get back to this object
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, &GlfwWindow::FramebufferSizeCallback);
        glfwSetKeyCallback(window_, &GlfwWindow::KeyCallback);
        glfwSetDropCallback(window_, &GlfwWindow::DropCallback);
        glfwSetWindowCloseCallback(window_, &GlfwWindow::CloseCallback);
        glfwSetWindowFocusCallback(window_, &GlfwWindow::FocusCallback);

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1); // vsync on

        // the framebuffer can be bigger than the requested size on high-DPI screens
        glfwGetFramebufferSize(window_, &spec_.width, &spec_.height);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    GlfwWindow::~GlfwWindow()
    {
        if (window_)
        {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
    }

    // Hooks the window's keyboard/mouse events up to an InputSystem (nullptr to unhook)
    void GlfwWindow::AttachInput(Input::InputSystem* input)
    {
        if (input)
        {
            // Replaces this window's own key callback, so Escape is checked in PollEvents() instead
            Input::RegisterCallbacks(window_, *input);
            return;
        }

        // RegisterCallbacks keeps a pointer to the InputSystem, so stop GLFW calling into it
        glfwSetKeyCallback(window_, nullptr);
        glfwSetMouseButtonCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetCharCallback(window_, nullptr);
    }

    // Handle window events; Escape closes the window if it was made to (the editor has its own uses for Escape)
    void GlfwWindow::PollEvents()
    {
        glfwPollEvents();
        if (spec_.closeOnEscape && glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            RequestClose();
        }
        if (glfwWindowShouldClose(window_))
        {
            shouldClose_ = true;
        }
    }

    void GlfwWindow::Present()
    {
        glfwSwapBuffers(window_);
    }

    bool GlfwWindow::ShouldClose() const
    {
        return shouldClose_;
    }

    void GlfwWindow::RequestClose()
    {
        shouldClose_ = true;
        if (window_)
        {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }

    int GlfwWindow::Width() const
    {
        return spec_.width;
    }

    int GlfwWindow::Height() const
    {
        return spec_.height;
    }

    const std::string& GlfwWindow::Title() const
    {
        return spec_.title;
    }

    void* GlfwWindow::NativeHandle() const
    {
        return window_;
    }

    float GlfwWindow::ContentScale() const
    {
        float x = 1.0f;
        float y = 1.0f;
        if (window_)
        {
            glfwGetWindowContentScale(window_, &x, &y);
        }
        return x;
    }

    void GlfwWindow::SetClientSize(int width, int height)
    {
        if (!window_ || fullscreen_ || width <= 0 || height <= 0)
        {
            return;
        }
        glfwSetWindowSize(window_, width, height);
    }

    // Covers the whole monitor the window is on, at the monitor's own resolution
    void GlfwWindow::SetFullscreen(bool fullscreen)
    {
        if (!window_ || fullscreen == fullscreen_)
        {
            return;
        }

        if (fullscreen)
        {
            GLFWmonitor* monitor = MonitorOf(window_);
            const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
            if (!mode)
            {
                return;
            }
            windowedMaximized_ = glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE;
            glfwGetWindowPos(window_, &windowedX_, &windowedY_);
            glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
            glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedWidth_, windowedHeight_, 0);
            if (windowedMaximized_)
            {
                glfwMaximizeWindow(window_);
            }
        }
        fullscreen_ = fullscreen;
    }

    void GlfwWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
        if (!self)
        {
            return;
        }

        // minimising gives 0 x 0, keep it at least 1 so aspect ratio maths doesn't divide by zero
        self->spec_.width = width > 0 ? width : 1;
        self->spec_.height = height > 0 ? height : 1;
    }

    // Only used until an InputSystem is attached: lets Escape close the window
    void GlfwWindow::KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
    {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
        if (self && self->spec_.closeOnEscape && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            self->RequestClose();
        }
    }

    // The user closing the window can be refused (RequestClose has already set shouldClose_)
    void GlfwWindow::CloseCallback(GLFWwindow* window)
    {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
        if (self && !self->shouldClose_ && self->canClose_ && !self->canClose_())
        {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
        }
    }

    // Another window has the mouse now
    void GlfwWindow::FocusCallback(GLFWwindow* window, int focused)
    {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
        if (self && focused == GLFW_FALSE)
        {
            self->SetCursorCaptured(false);
        }
    }

    void GlfwWindow::DropCallback(GLFWwindow* window, int count, const char** paths)
    {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
        for (int i = 0; self && i < count; ++i)
        {
            self->dropped_.emplace_back(paths[i]); // UTF-8, the narrow encoding on Linux
        }
    }

    std::vector<std::filesystem::path> GlfwWindow::TakeDroppedFiles()
    {
        std::vector<std::filesystem::path> files;
        files.swap(dropped_);
        return files;
    }

    // GLFW's own "disabled" cursor: hidden, held in the window, and reporting movement without edges
    void GlfwWindow::SetCursorCaptured(bool captured)
    {
        if (captured == cursorCaptured_ || window_ == nullptr ||
            (captured && glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_FALSE))
        {
            return;
        }
        // No GLFW_RAW_MOUSE_MOTION: GLFW adds raw values up as movement, but a pointer that reports where it is rather than
        // how far it moved (virtual machines, WSL, remote desktops) then sends the view spinning
        const bool hold = CanHoldPointer();
        glfwSetInputMode(window_, GLFW_CURSOR, captured && hold ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        // Where the pointer can't be held, the mouse turns the view by what it really moved, up to the edge of the window.
        // WSL shows a blank cursor as the normal arrow, so the pointer becomes a single pixel instead
        if (!hold)
        {
            if (!dotCursor_)
            {
                unsigned char pixel[4] = {0, 0, 0, 255};
                const GLFWimage image{1, 1, pixel};
                dotCursor_ = glfwCreateCursor(&image, 0, 0);
            }
            glfwSetCursor(window_, captured ? dotCursor_ : nullptr);
        }
        cursorCaptured_ = captured;
    }
}
