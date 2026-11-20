/* !
@file    	    ImGuiLayer.cpp
@author  	    sanni.go@digipen.edu [100%]
@date    		22/09/2026
@brief
    Implements ImGuiLayer: ImGui context, platform and OpenGL renderer backends
    with dynamic font textures, the editor font, the layout file location and
    Win32 message handling.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/ui/ImGuiLayer.hpp"

#include "engine/core/Log.hpp"
#include "engine/platform/Paths.hpp"

#include <imgui/imgui.h>

// Before any other OpenGL header (GLFW's includes gl.h)
#include <GL/glew.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#else
#include <GLFW/glfw3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine::ImGuiLayer
{
    namespace
    {
        bool g_initialized = false;
        // ImGui keeps a pointer to the settings path, so it must outlive the context
        std::string g_iniPath;
#ifndef _WIN32
        bool g_glfwBackend = false;
#endif

        // ImGui 1.92 asks the renderer to create / update / destroy its textures (ImGuiBackendFlags_RendererHasTextures).
        // That lets it rasterize text at the exact size it is drawn - so text stays sharp at any UI scale or display
        // DPI - instead of stretching one small font bitmap, which looked blurry when the UI was scaled.
        void UpdateTexture(ImTextureData* texture)
        {
            if (texture->Status == ImTextureStatus_WantCreate)
            {
                GLuint id = 0;
                glGenTextures(1, &id);
                glBindTexture(GL_TEXTURE_2D, id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->Width, texture->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             texture->GetPixels());
                texture->SetTexID(static_cast<ImTextureID>(id));
                texture->SetStatus(ImTextureStatus_OK);
                // Normally a couple of times at startup; again later means the font texture was rebuilt
                LOG_INFO(UI, "UI font texture {} made ({} x {})", id, texture->Width, texture->Height);
            }
            else if (texture->Status == ImTextureStatus_WantUpdates)
            {
                // Only the changed rectangles (e.g. glyphs just rasterized at a new size)
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture->TexID));
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, texture->Width);
                for (const ImTextureRect& rect : texture->Updates)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h, GL_RGBA, GL_UNSIGNED_BYTE,
                                    texture->GetPixelsAt(rect.x, rect.y));
                }
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                texture->SetStatus(ImTextureStatus_OK);
            }
            else if (texture->Status == ImTextureStatus_WantDestroy && texture->UnusedFrames > 0)
            {
                const GLuint id = static_cast<GLuint>(texture->TexID);
                glDeleteTextures(1, &id);
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
                LOG_INFO(UI, "UI font texture {} deleted", id);
            }
        }

        void DestroyTextures()
        {
            for (ImTextureData* texture : ImGui::GetPlatformIO().Textures)
            {
                if (texture->RefCount == 1 && texture->TexID != ImTextureID_Invalid)
                {
                    const GLuint id = static_cast<GLuint>(texture->TexID);
                    glDeleteTextures(1, &id);
                    texture->SetTexID(ImTextureID_Invalid);
                    texture->SetStatus(ImTextureStatus_Destroyed);
                }
            }
        }

        // A vector UI font that stays crisp at every size (ImGui's built-in font is a small pixel font): the editor's
        // own (DM Sans, copied to assets/fonts/ by its build), else the system's
        void LoadFont()
        {
            ImGuiIO& io = ImGui::GetIO();
            std::vector<std::filesystem::path> candidates{Platform::ExecutableDir() / "assets" / "fonts" /
                                                          "DMSans-Regular.ttf"};
#ifdef _WIN32
            candidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
            candidates.emplace_back("C:/Windows/Fonts/tahoma.ttf");
#endif
            for (const std::filesystem::path& candidate : candidates)
            {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec) &&
                    io.Fonts->AddFontFromFileTTF(candidate.string().c_str(), 16.0f) != nullptr)
                {
                    return;
                }
            }
            io.Fonts->AddFontDefault();
        }

        void SetupRenderState(const ImDrawData* drawData, int fbWidth, int fbHeight)
        {
            // The UI is drawn the old fixed-function way, from arrays in memory, onto the window. Whatever the
            // engine's drawing left bound would take over: a shader program draws the UI with a game shader, a bound
            // vertex array or buffer turns the UI's vertex and index pointers into offsets into a game mesh, and
            // another texture unit makes every glyph sample a game texture. Any of these garbles the whole window,
            // and which is left bound depends on what the scene drew last, so it comes and goes.
            if (glUseProgram != nullptr) // GLEW is loaded once the engine draws anything
            {
                glUseProgram(0);
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glActiveTexture(GL_TEXTURE0);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDisableClientState(GL_NORMAL_ARRAY);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_TEXTURE_2D);

            glViewport(0, 0, fbWidth, fbHeight);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(drawData->DisplayPos.x, drawData->DisplayPos.x + drawData->DisplaySize.x,
                    drawData->DisplayPos.y + drawData->DisplaySize.y, drawData->DisplayPos.y, -1.0f, 1.0f);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glLoadIdentity();

            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glEnableClientState(GL_COLOR_ARRAY);
        }

        void RestoreRenderState()
        {
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);

            glMatrixMode(GL_TEXTURE);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
        }

        void RenderDrawData(ImDrawData* drawData)
        {
            const int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
            const int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
            if (fbWidth <= 0 || fbHeight <= 0)
            {
                return;
            }

            // Create / update / destroy the textures ImGui asked for (the font atlas grows as new sizes are drawn)
            if (drawData->Textures != nullptr)
            {
                for (ImTextureData* texture : *drawData->Textures)
                {
                    if (texture->Status != ImTextureStatus_OK)
                    {
                        UpdateTexture(texture);
                    }
                }
            }

            SetupRenderState(drawData, fbWidth, fbHeight);

            const ImVec2 clipOff = drawData->DisplayPos;
            const ImVec2 clipScale = drawData->FramebufferScale;

            for (int n = 0; n < drawData->CmdListsCount; n++)
            {
                const ImDrawList* cmdList = drawData->CmdLists[n];
                const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data;

                glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), reinterpret_cast<const void*>(&vtxBuffer->pos));
                glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), reinterpret_cast<const void*>(&vtxBuffer->uv));
                glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), reinterpret_cast<const void*>(&vtxBuffer->col));

                for (int cmd_i = 0; cmd_i < cmdList->CmdBuffer.Size; cmd_i++)
                {
                    const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd_i];
                    if (pcmd->UserCallback != nullptr)
                    {
                        pcmd->UserCallback(cmdList, pcmd);
                    }
                    else
                    {
                        ImVec4 clipRect;
                        clipRect.x = (pcmd->ClipRect.x - clipOff.x) * clipScale.x;
                        clipRect.y = (pcmd->ClipRect.y - clipOff.y) * clipScale.y;
                        clipRect.z = (pcmd->ClipRect.z - clipOff.x) * clipScale.x;
                        clipRect.w = (pcmd->ClipRect.w - clipOff.y) * clipScale.y;

                        if (clipRect.x < static_cast<float>(fbWidth) && clipRect.y < static_cast<float>(fbHeight) &&
                            clipRect.z >= 0.0f && clipRect.w >= 0.0f)
                        {
                            glScissor(static_cast<int>(clipRect.x),
                                      static_cast<int>(static_cast<float>(fbHeight) - clipRect.w),
                                      static_cast<int>(clipRect.z - clipRect.x),
                                      static_cast<int>(clipRect.w - clipRect.y));

                            // Each command says where its indices start: ImGui reorders commands (a modal's dimmed
                            // background is moved to the front of the list), so they are not in index order
                            const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data + pcmd->IdxOffset;
                            const GLuint texture = static_cast<GLuint>(pcmd->GetTexID());
                            glBindTexture(GL_TEXTURE_2D, texture);

#if defined(IMGUI_USE_BGRA_PACKED_COLOR)
                            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pcmd->ElemCount),
                                           sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idxBuffer);
#else
                            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pcmd->ElemCount),
                                           sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idxBuffer);
#endif
                        }
                    }
                }
            }

            RestoreRenderState();
        }

#ifdef _WIN32
        ImGuiKey VirtualKeyToImGuiKey(WPARAM wParam)
        {
            switch (wParam)
            {
            case VK_TAB: return ImGuiKey_Tab;
            case VK_LEFT: return ImGuiKey_LeftArrow;
            case VK_RIGHT: return ImGuiKey_RightArrow;
            case VK_UP: return ImGuiKey_UpArrow;
            case VK_DOWN: return ImGuiKey_DownArrow;
            case VK_PRIOR: return ImGuiKey_PageUp;
            case VK_NEXT: return ImGuiKey_PageDown;
            case VK_HOME: return ImGuiKey_Home;
            case VK_END: return ImGuiKey_End;
            case VK_INSERT: return ImGuiKey_Insert;
            case VK_DELETE: return ImGuiKey_Delete;
            case VK_BACK: return ImGuiKey_Backspace;
            case VK_SPACE: return ImGuiKey_Space;
            case VK_RETURN: return ImGuiKey_Enter;
            case VK_ESCAPE: return ImGuiKey_Escape;
            case VK_OEM_7: return ImGuiKey_Apostrophe;
            case VK_OEM_COMMA: return ImGuiKey_Comma;
            case VK_OEM_MINUS: return ImGuiKey_Minus;
            case VK_OEM_PERIOD: return ImGuiKey_Period;
            case VK_OEM_2: return ImGuiKey_Slash;
            case VK_OEM_1: return ImGuiKey_Semicolon;
            case VK_OEM_PLUS: return ImGuiKey_Equal;
            case VK_OEM_4: return ImGuiKey_LeftBracket;
            case VK_OEM_5: return ImGuiKey_Backslash;
            case VK_OEM_6: return ImGuiKey_RightBracket;
            case VK_OEM_3: return ImGuiKey_GraveAccent;
            case VK_CAPITAL: return ImGuiKey_CapsLock;
            case VK_SCROLL: return ImGuiKey_ScrollLock;
            case VK_NUMLOCK: return ImGuiKey_NumLock;
            case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
            case VK_PAUSE: return ImGuiKey_Pause;
            case VK_NUMPAD0: return ImGuiKey_Keypad0;
            case VK_NUMPAD1: return ImGuiKey_Keypad1;
            case VK_NUMPAD2: return ImGuiKey_Keypad2;
            case VK_NUMPAD3: return ImGuiKey_Keypad3;
            case VK_NUMPAD4: return ImGuiKey_Keypad4;
            case VK_NUMPAD5: return ImGuiKey_Keypad5;
            case VK_NUMPAD6: return ImGuiKey_Keypad6;
            case VK_NUMPAD7: return ImGuiKey_Keypad7;
            case VK_NUMPAD8: return ImGuiKey_Keypad8;
            case VK_NUMPAD9: return ImGuiKey_Keypad9;
            case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
            case VK_DIVIDE: return ImGuiKey_KeypadDivide;
            case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
            case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
            case VK_ADD: return ImGuiKey_KeypadAdd;
            case VK_LSHIFT: return ImGuiKey_LeftShift;
            case VK_LCONTROL: return ImGuiKey_LeftCtrl;
            case VK_LMENU: return ImGuiKey_LeftAlt;
            case VK_LWIN: return ImGuiKey_LeftSuper;
            case VK_RSHIFT: return ImGuiKey_RightShift;
            case VK_RCONTROL: return ImGuiKey_RightCtrl;
            case VK_RMENU: return ImGuiKey_RightAlt;
            case VK_RWIN: return ImGuiKey_RightSuper;
            case VK_APPS: return ImGuiKey_Menu;
            }

            if (wParam >= VK_F1 && wParam <= VK_F12)
            {
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (wParam - VK_F1));
            }
            if (wParam >= '0' && wParam <= '9')
            {
                return static_cast<ImGuiKey>(ImGuiKey_0 + (wParam - '0'));
            }
            if (wParam >= 'A' && wParam <= 'Z')
            {
                return static_cast<ImGuiKey>(ImGuiKey_A + (wParam - 'A'));
            }

            return ImGuiKey_None;
        }
#endif
    }

    // Context, backends, font, and where the layout file goes
    void ImGuiLayer::Init(Platform::Window& window)
    {
        if (g_initialized)
        {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
#ifdef _WIN32
        io.BackendPlatformName = "Atelier3D_Win32";
#else
        // Input comes from the official GLFW backend; rendering stays on our OpenGL1 renderer
        if (auto* glfwWindow = static_cast<GLFWwindow*>(window.NativeHandle()))
        {
            ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
            g_glfwBackend = true;
        }
#endif
        io.BackendRendererName = "Atelier3D_OpenGL1";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

        // Save the window layout next to the exe (the build's bin/ folder, which is gitignored).
        // ImGui's default "imgui.ini" is relative to the working directory, so it landed wherever the app was started.
        g_iniPath = (Platform::ExecutableDir() / "imgui.ini").string();
        io.IniFilename = g_iniPath.c_str();

        io.DisplaySize = ImVec2(static_cast<float>(window.Width()), static_cast<float>(window.Height()));
        io.DeltaTime = 1.0f / 60.0f;

        LoadFont();

        g_initialized = true;
    }

    // In reverse order of Init
    void ImGuiLayer::Shutdown()
    {
        if (!g_initialized)
        {
            return;
        }

        DestroyTextures();
#ifndef _WIN32
        if (g_glfwBackend)
        {
            ImGui_ImplGlfw_Shutdown();
            g_glfwBackend = false;
        }
#endif
        ImGui::DestroyContext();
        g_initialized = false;
    }

    // Tell ImGui the window size and time, then start a new frame
    void ImGuiLayer::BeginFrame(Platform::Window& window, double deltaSeconds)
    {
        if (!g_initialized)
        {
            Init(window);
        }

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(window.Width()), static_cast<float>(window.Height()));
        io.DeltaTime = static_cast<float>(std::max(deltaSeconds, 1.0 / 1000.0));

#ifndef _WIN32
        if (g_glfwBackend)
        {
            ImGui_ImplGlfw_NewFrame();
        }
#endif

        ImGui::NewFrame();
    }

    // Build the draw data and render it with OpenGL
    void ImGuiLayer::EndFrame()
    {
        if (!g_initialized)
        {
            return;
        }

        ImGui::Render();
        RenderDrawData(ImGui::GetDrawData());

        // OpenGL errors of the whole frame (the engine's drawing and the UI's), summed up every few seconds: a
        // frame that fails part-way can show as a blank or black flash
        static int errors = 0;
        static GLenum lastError = GL_NO_ERROR;
        static auto since = std::chrono::steady_clock::now();
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            ++errors;
            lastError = error;
        }
        if (std::chrono::steady_clock::now() - since >= std::chrono::seconds(5))
        {
            if (errors > 0)
            {
                LOG_WARN(Render, "{} OpenGL errors in the last 5 s (last: 0x{:04X})", errors, lastError);
            }
            errors = 0;
            since = std::chrono::steady_clock::now();
        }
    }

#ifdef _WIN32
    bool HandleWin32Message(void* hwnd, unsigned int msg, std::uintptr_t wParam, std::intptr_t lParam)
    {
        if (!ImGui::GetCurrentContext())
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();

        switch (msg)
        {
        case WM_LBUTTONDOWN:
            io.AddMouseButtonEvent(0, true);
            SetCapture(reinterpret_cast<HWND>(hwnd));
            return true;
        case WM_LBUTTONUP:
            io.AddMouseButtonEvent(0, false);
            ReleaseCapture();
            return true;
        case WM_RBUTTONDOWN:
            io.AddMouseButtonEvent(1, true);
            SetCapture(reinterpret_cast<HWND>(hwnd));
            return true;
        case WM_RBUTTONUP:
            io.AddMouseButtonEvent(1, false);
            ReleaseCapture();
            return true;
        case WM_MBUTTONDOWN:
            io.AddMouseButtonEvent(2, true);
            SetCapture(reinterpret_cast<HWND>(hwnd));
            return true;
        case WM_MBUTTONUP:
            io.AddMouseButtonEvent(2, false);
            ReleaseCapture();
            return true;
        case WM_MOUSEWHEEL:
            io.AddMouseWheelEvent(0.0f,
                                  static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA));
            return true;
        case WM_MOUSEHWHEEL:
            io.AddMouseWheelEvent(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA),
                                  0.0f);
            return true;
        case WM_MOUSEMOVE:
            io.AddMousePosEvent(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return true;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const ImGuiKey key = VirtualKeyToImGuiKey(static_cast<WPARAM>(wParam));
            if (key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, true);
            }
            io.AddKeyEvent(ImGuiKey_ModCtrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModShift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModAlt, (GetKeyState(VK_MENU) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModSuper,
                           (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0);
            return true;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            const ImGuiKey key = VirtualKeyToImGuiKey(static_cast<WPARAM>(wParam));
            if (key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, false);
            }
            io.AddKeyEvent(ImGuiKey_ModCtrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModShift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModAlt, (GetKeyState(VK_MENU) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiKey_ModSuper,
                           (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0);
            return true;
        }
        case WM_CHAR: io.AddInputCharacter(static_cast<unsigned int>(wParam)); return true;
        }

        return false;
    }
#endif
}
