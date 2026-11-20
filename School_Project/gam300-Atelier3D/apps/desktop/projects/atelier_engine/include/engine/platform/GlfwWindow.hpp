/* !
@file    	    GlfwWindow.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		25/09/2026
@brief
    Declares GlfwWindow, the window and OpenGL context used on non-Windows
    platforms.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/platform/Window.hpp"

struct GLFWcursor;
struct GLFWwindow;

namespace Engine::Platform
{
    // Window + OpenGL context for non-Windows platforms (Linux, macOS)
    class GlfwWindow final : public Window
    {
    public:
        explicit GlfwWindow(WindowSpec spec);
        ~GlfwWindow() override;

        void PollEvents() override;
        void Present() override;
        bool ShouldClose() const override;
        void RequestClose() override;

        int Width() const override;
        int Height() const override;
        const std::string& Title() const override;

        void* NativeHandle() const override;

        // Uses Input::RegisterCallbacks (the GLFW input callbacks); nullptr removes them again
        void AttachInput(Input::InputSystem* input) override;
        void SetCloseCallback(std::function<bool()> canClose) override { canClose_ = std::move(canClose); }
        float ContentScale() const override;
        void SetFullscreen(bool fullscreen) override;
        bool IsFullscreen() const override { return fullscreen_; }
        void SetClientSize(int width, int height) override;
        std::vector<std::filesystem::path> TakeDroppedFiles() override;
        void SetCursorCaptured(bool captured) override;
        bool IsCursorCaptured() const override { return cursorCaptured_; }

    private:
        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void CloseCallback(GLFWwindow* window);
        static void DropCallback(GLFWwindow* window, int count, const char** paths);
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void FocusCallback(GLFWwindow* window, int focused);

        WindowSpec spec_;
        bool shouldClose_ = false;
        std::function<bool()> canClose_;

        GLFWwindow* window_ = nullptr;
        bool cursorCaptured_ = false;
        GLFWcursor* dotCursor_ = nullptr; // made when first needed; GLFW frees it with the window system
        bool fullscreen_ = false;
        // Where the window was before going fullscreen, to go back to
        int windowedX_ = 0;
        int windowedY_ = 0;
        int windowedWidth_ = 0;
        int windowedHeight_ = 0;
        bool windowedMaximized_ = false;
        std::vector<std::filesystem::path> dropped_; // files dropped since the last TakeDroppedFiles
    };
}
