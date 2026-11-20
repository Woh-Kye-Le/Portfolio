/* !
@file    	    WindowsWindow.cpp
@author  	    c.kojat@digipen.edu [96%]
         	    sanni.go@digipen.edu [4%]
@date    		22/09/2026
@brief
    Implements WindowsWindow: window class registration, DPI handling, the WGL
    context, the message loop, keyboard/mouse translation into InputSystem and
    borderless fullscreen.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/platform/WindowsWindow.hpp"

#include "engine/input/Input.hpp"
#include "engine/input/Win32KeyMap.hpp"
#include "engine/ui/ImGuiLayer.hpp"

#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <GL/gl.h>

#include <stdexcept>
#include <string>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace Engine::Platform
{
    namespace
    {
        // How many screen refreshes SwapBuffers waits for (WGL_EXT_swap_control): 1 vsync, 0 none
        void SetSwapInterval(int interval)
        {
            using SwapIntervalFn = BOOL(WINAPI*)(int);
            if (const auto swapInterval = reinterpret_cast<SwapIntervalFn>(wglGetProcAddress("wglSwapIntervalEXT")))
            {
                swapInterval(interval);
            }
        }

        std::wstring ToWide(const std::string& utf8)
        {
            if (utf8.empty())
            {
                return {};
            }

            const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
            if (len <= 0)
            {
                return L"";
            }

            std::wstring out;
            out.resize(static_cast<size_t>(len - 1));
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), len);
            return out;
        }

        constexpr const wchar_t* kWindowClassName = L"Atelier3D_Win32Window";

        // Tells Windows the app draws at the display's real resolution. Without this, on a scaled display (125%,
        // 150%...) Windows draws the app at 100% and stretches the picture, which makes the UI and 3D view blurry.
        // Loaded at run time so it also runs on Windows versions without the per-monitor V2 call.
        void EnableDpiAwareness()
        {
            static bool done = false;
            if (done)
            {
                return;
            }
            done = true;

            if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
            {
                using SetContextFn = BOOL(WINAPI*)(HANDLE);
                if (auto setContext = reinterpret_cast<SetContextFn>(
                        reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))))
                {
                    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
                    if (setContext(reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-4))))
                    {
                        return;
                    }
                }
            }
            SetProcessDPIAware(); // older Windows: system-DPI aware
        }

        // The window's DPI (96 = 100%)
        UINT WindowDpi(HWND hwnd)
        {
            if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
            {
                using GetDpiFn = UINT(WINAPI*)(HWND);
                if (auto getDpi =
                        reinterpret_cast<GetDpiFn>(reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow"))))
                {
                    if (const UINT dpi = hwnd ? getDpi(hwnd) : 0; dpi != 0)
                    {
                        return dpi;
                    }
                }
            }
            const HDC screen = GetDC(nullptr);
            const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(nullptr, screen);
            return dpi > 0 ? static_cast<UINT>(dpi) : 96u;
        }
    }

    // DPI first, otherwise Windows scales the window up and everything is blurry
    WindowsWindow::WindowsWindow(WindowSpec spec) : spec_(std::move(spec))
    {
        EnableDpiAwareness();
        CreateNativeWindow();
        CreateOpenGLContext();
    }

    // Tear down in the reverse order we made things: GL context, then the device context, then the window
    WindowsWindow::~WindowsWindow()
    {
        if (hglrc_)
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc_);
            hglrc_ = nullptr;
        }

        if (hdc_ && hwnd_)
        {
            ReleaseDC(hwnd_, hdc_);
            hdc_ = nullptr;
        }

        if (hwnd_)
        {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    void WindowsWindow::PollEvents()
    {
        // PeekMessage instead of GetMessage so we don't block - the game has to keep running with no input
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                shouldClose_ = true;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // Moving or resizing the window moves its middle, where a captured pointer is held
        if (cursorCaptured_)
        {
            ClipCursorToCentre();
        }
    }

    void WindowsWindow::Present()
    {
        if (hdc_)
        {
            SwapBuffers(hdc_);
            // One frame per screen refresh: wait for the compositor to take this frame (see CreateOpenGLContext)
            if (pacedByCompositor_ && FAILED(DwmFlush()))
            {
                pacedByCompositor_ = false;
                SetSwapInterval(1);
            }
        }
    }

    bool WindowsWindow::ShouldClose() const
    {
        return shouldClose_;
    }

    void WindowsWindow::RequestClose()
    {
        shouldClose_ = true;
        if (hwnd_)
        {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        }
    }

    int WindowsWindow::Width() const
    {
        return spec_.width;
    }

    int WindowsWindow::Height() const
    {
        return spec_.height;
    }

    const std::string& WindowsWindow::Title() const
    {
        return spec_.title;
    }

    void WindowsWindow::AttachInput(Input::InputSystem* input)
    {
        input_ = input;
    }

    float WindowsWindow::ContentScale() const
    {
        return static_cast<float>(WindowDpi(hwnd_)) / 96.0f;
    }

    void WindowsWindow::SetClientSize(int width, int height)
    {
        if (!hwnd_ || fullscreen_ || width <= 0 || height <= 0)
        {
            return;
        }
        RECT rect{0, 0, width, height};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        AdjustWindowRect(&rect, style, FALSE);
        ShowWindow(hwnd_, SW_RESTORE); // a maximized window would stay maximized
        SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void WindowsWindow::SetFullscreen(bool fullscreen)
    {
        if (!hwnd_ || fullscreen == fullscreen_)
        {
            return;
        }

        if (fullscreen)
        {
            // Borderless, covering the whole monitor the window is on
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (!GetWindowPlacement(hwnd_, &windowedPlacement_) ||
                !GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor))
            {
                return;
            }
            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(hwnd_, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                         monitor.rcMonitor.right - monitor.rcMonitor.left,
                         monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        else
        {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(hwnd_, &windowedPlacement_);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        fullscreen_ = fullscreen;
    }

    void WindowsWindow::SetCursorCaptured(bool captured)
    {
        if (captured == cursorCaptured_ || hwnd_ == nullptr || (captured && GetForegroundWindow() != hwnd_))
        {
            return;
        }
        // Raw mouse input while captured: movement as the mouse reports it, not limited by the screen's edges
        RAWINPUTDEVICE mouse{};
        mouse.usUsagePage = 0x01; // generic desktop controls
        mouse.usUsage = 0x02;     // mouse
        mouse.dwFlags = captured ? 0u : static_cast<DWORD>(RIDEV_REMOVE);
        mouse.hwndTarget = captured ? hwnd_ : nullptr;
        RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
        cursorCaptured_ = captured;
        if (captured)
        {
            if (input_)
            {
                const Input::Vec2 at = input_->GetMousePosition();
                rawX_ = at.x;
                rawY_ = at.y;
            }
            ClipCursorToCentre();
            SetCursor(nullptr);
        }
        else
        {
            ClipCursor(nullptr);
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        }
    }

    void WindowsWindow::ClipCursorToCentre() const
    {
        RECT client{};
        GetClientRect(hwnd_, &client);
        POINT centre{(client.left + client.right) / 2, (client.top + client.bottom) / 2};
        ClientToScreen(hwnd_, &centre);
        const RECT spot{centre.x, centre.y, centre.x + 1, centre.y + 1};
        ClipCursor(&spot);
    }

    // Turns Win32 messages into InputSystem calls
    void WindowsWindow::ForwardInput(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (!input_)
        {
            return;
        }

        // Win32 virtual key -> our key codes. The scan code and "extended" bit are needed to tell left/right
        // Ctrl/Shift/Alt and the numpad Enter apart
        const auto keyCode = [&]
        {
            const auto scanCode = static_cast<unsigned int>((lParam >> 16) & 0xFF);
            const bool extended = ((lParam >> 24) & 1) != 0;
            return Input::TranslateWin32Key(static_cast<unsigned int>(wParam), scanCode, extended);
        };

        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: input_->SetKey(keyCode(), Input::KeyState::Down); break;
        case WM_KEYUP:
        case WM_SYSKEYUP: input_->SetKey(keyCode(), Input::KeyState::Up); break;
        case WM_CHAR: input_->AddTextInput(static_cast<unsigned int>(wParam)); break;
        case WM_MOUSEMOVE:
            if (!cursorCaptured_) // a captured pointer does not move: its movement comes from WM_INPUT
            {
                input_->SetMousePosition(static_cast<float>(GET_X_LPARAM(lParam)),
                                         static_cast<float>(GET_Y_LPARAM(lParam)));
            }
            break;
        case WM_INPUT:
            if (cursorCaptured_)
            {
                RAWINPUT raw{};
                UINT size = sizeof(raw);
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size,
                                    sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
                    raw.header.dwType == RIM_TYPEMOUSE && (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                {
                    rawX_ += static_cast<float>(raw.data.mouse.lLastX);
                    rawY_ += static_cast<float>(raw.data.mouse.lLastY);
                    input_->SetMousePosition(rawX_, rawY_);
                }
            }
            break;
        case WM_LBUTTONDOWN: input_->SetMouseButton(Input::Mouse_Left, Input::KeyState::Down); break;
        case WM_LBUTTONUP: input_->SetMouseButton(Input::Mouse_Left, Input::KeyState::Up); break;
        case WM_RBUTTONDOWN: input_->SetMouseButton(Input::Mouse_Right, Input::KeyState::Down); break;
        case WM_RBUTTONUP: input_->SetMouseButton(Input::Mouse_Right, Input::KeyState::Up); break;
        case WM_MBUTTONDOWN: input_->SetMouseButton(Input::Mouse_Middle, Input::KeyState::Down); break;
        case WM_MBUTTONUP: input_->SetMouseButton(Input::Mouse_Middle, Input::KeyState::Up); break;
        case WM_MOUSEWHEEL: // one notch = WHEEL_DELTA (120), so this gives 1.0 per notch
            input_->SetScrollDelta(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                                   static_cast<float>(WHEEL_DELTA));
            break;
        case WM_KILLFOCUS:
            // Key-up messages go to whichever window has focus now, so release everything instead of leaving it held
            for (int key = 0; key < 512; ++key)
            {
                input_->SetKey(key, Input::KeyState::Up);
            }
            for (int button = 0; button < 8; ++button)
            {
                input_->SetMouseButton(button, Input::KeyState::Up);
            }
            break;
        default: break;
        }
    }

    // Every message for our window ends up here
    LRESULT WindowsWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // ImGui and our InputSystem both see every message; the editor decides later who actually gets to use it
        Engine::ImGuiLayer::HandleWin32Message(hwnd_, msg, static_cast<std::uintptr_t>(wParam),
                                               static_cast<std::intptr_t>(lParam));
        ForwardInput(msg, wParam, lParam);

        switch (msg)
        {
        case WM_CLOSE:
            // The user closing the window can be refused (RequestClose has already set shouldClose_)
            if (!shouldClose_ && canClose_ && !canClose_())
            {
                return 0;
            }
            shouldClose_ = true;
            DestroyWindow(hwnd_);
            return 0;

        case WM_DROPFILES:
        {
            const auto drop = reinterpret_cast<HDROP>(wParam);
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i)
            {
                std::wstring path(DragQueryFileW(drop, i, nullptr, 0), L'\0');
                DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size() + 1));
                dropped_.emplace_back(path);
            }
            DragFinish(drop);
            return 0;
        }

        case WM_DESTROY:
            shouldClose_ = true;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            spec_.width = LOWORD(lParam);
            spec_.height = HIWORD(lParam);
            if (spec_.width <= 0)
            {
                spec_.width = 1;
            }
            if (spec_.height <= 0)
            {
                spec_.height = 1;
            }
            // Windows stops our main loop while the window is being dragged/resized, so redraw from here
            // or the window just shows garbage until you let go
            if (refresh_ && wParam != SIZE_MINIMIZED)
            {
                refresh_();
            }
            return 0;

        case WM_SETCURSOR:
            // A captured pointer is hidden over the window (ImGui would otherwise put its own cursor back)
            if (cursorCaptured_ && LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case WM_KILLFOCUS: SetCursorCaptured(false); break; // another window has the mouse now
        case WM_ERASEBKGND:
            return 1; // we redraw everything with OpenGL anyway; letting Windows clear it causes flicker

        case WM_DPICHANGED:
        {
            // Moved to a monitor with a different scale: take the size Windows suggests (the same physical size)
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && spec_.closeOnEscape)
            {
                RequestClose();
                return 0;
            }
            return 0;
        }

        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }

    LRESULT CALLBACK WindowsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // The first message: grab the "this" pointer we passed to CreateWindowExW and store it on the HWND,
        // so every later message can find its WindowsWindow
        if (msg == WM_NCCREATE)
        {
            const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* window = static_cast<WindowsWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            window->hwnd_ = hwnd;
        }

        auto* window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (window)
        {
            return window->HandleMessage(msg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // Registers the window class and makes the window itself
    void WindowsWindow::CreateNativeWindow()
    {
        HINSTANCE hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // CS_OWNDC: OpenGL wants the window to keep one DC
        wc.lpfnWndProc = &WindowsWindow::WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClassName;

        RegisterClassExW(&wc);

        // The requested size is at 100%: make it as large on screen on a scaled display
        const float scale = static_cast<float>(WindowDpi(nullptr)) / 96.0f;
        const DWORD style = WS_OVERLAPPEDWINDOW;
        RECT rect{0, 0, static_cast<LONG>(static_cast<float>(spec_.width) * scale),
                  static_cast<LONG>(static_cast<float>(spec_.height) * scale)};
        AdjustWindowRect(&rect, style, FALSE);

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;

        hwnd_ = CreateWindowExW(0, kWindowClassName, ToWide(spec_.title).c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                                width, height, nullptr, nullptr, hInstance, this);

        if (!hwnd_)
        {
            throw std::runtime_error("CreateWindowExW failed");
        }

        DragAcceptFiles(hwnd_, TRUE); // files dropped from Explorer arrive as WM_DROPFILES
        ShowWindow(hwnd_, spec_.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
        UpdateWindow(hwnd_);
    }

    std::vector<std::filesystem::path> WindowsWindow::TakeDroppedFiles()
    {
        std::vector<std::filesystem::path> files;
        files.swap(dropped_);
        return files;
    }

    // Old-school WGL setup: pick a pixel format, make a context, make it current
    void WindowsWindow::CreateOpenGLContext()
    {
        hdc_ = GetDC(hwnd_);
        if (!hdc_)
        {
            throw std::runtime_error("GetDC failed");
        }

        // Ask for a normal double-buffered RGBA format with depth + stencil (same as the GLFW window)
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        const int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
        if (pixelFormat == 0)
        {
            throw std::runtime_error("ChoosePixelFormat failed");
        }

        if (!SetPixelFormat(hdc_, pixelFormat, &pfd))
        {
            throw std::runtime_error("SetPixelFormat failed");
        }

        hglrc_ = wglCreateContext(hdc_);
        if (!hglrc_)
        {
            throw std::runtime_error("wglCreateContext failed");
        }

        if (!wglMakeCurrent(hdc_, hglrc_))
        {
            throw std::runtime_error("wglMakeCurrent failed");
        }

        // One frame per screen refresh, so each is shown whole and no more are drawn than the screen can show. A window
        // is shown by the desktop compositor, so Present waits for it (DwmFlush) rather than asking the driver for
        // vsync: on a fast variable-refresh monitor (G-SYNC, 144 Hz) the driver's vsync now and then showed a frame
        // left from the first ones drawn, which flickered across the whole picture. Without the compositor (it can
        // be off before Windows 8), the driver's vsync it is.
        BOOL composited = FALSE;
        pacedByCompositor_ = SUCCEEDED(DwmIsCompositionEnabled(&composited)) && composited;
        SetSwapInterval(pacedByCompositor_ ? 0 : 1);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }
}
