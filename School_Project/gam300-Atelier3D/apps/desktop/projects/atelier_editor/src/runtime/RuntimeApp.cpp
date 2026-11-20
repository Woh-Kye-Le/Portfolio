/* !
@file    	    RuntimeApp.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements RuntimeApp, the game-only Application: opens a window sized from
    game.cfg, runs a GameRuntime with full keyboard and mouse input, and plays
    the start scene or a chosen scene.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/runtime/RuntimeApp.hpp"

#include "editor/Theme.hpp"
#include "editor/runtime/GameHud.hpp"
#include "engine/Engine.hpp"
#include "engine/ui/ImGuiLayer.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace Editor::Runtime
{
    namespace
    {
        // The "game only" app: same engine, but no editor UI. Window size and title come from game.cfg.
        class RuntimeApp final : public Engine::Core::Application
        {
        public:
            RuntimeApp(Engine::Game::GameProject project, std::filesystem::path scene)
                : Engine::Core::Application(Engine::Core::ApplicationSpec{
                      .window = Engine::Platform::WindowSpec{.width = project.Settings().windowWidth,
                                                             .height = project.Settings().windowHeight,
                                                             .title = project.Settings().name},
                      .maxFrames = 0}),
                  project_(std::move(project)), scene_(std::move(scene))
            {
            }

        protected:
            void OnInit() override
            {
                GetWindow().AttachInput(&input_);
                // The HUD is drawn with ImGui, in the editor's fonts
                imgui_.Init(GetWindow());
                Theme::LoadFonts();
                // The HUD is only drawn: the mouse (and the pointer's look) are the game's
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
                runtime_ = std::make_unique<Engine::Game::GameRuntime>();
                runtime_->SetEventBus(&GetEvents());
                runtime_->LoadProject(project_);
                // --scene lets us jump straight into one scene (we use it for the demos), otherwise the start scene
                if (scene_.empty())
                {
                    runtime_->Start();
                }
                else
                {
                    runtime_->Start(project_.LoadScene(scene_));
                }
            }

            void OnUpdate(double deltaSeconds) override
            {
                // The game has the mouse from the start (and after a click): hidden, held in the window, turning the
                // player. Esc gives it back.
                // A click on something the game shows on its screen (a button) goes to its object instead, and the
                // blocks can lock and free the mouse themselves.
                Engine::Platform::Window& window = GetWindow();
                const Engine::Input::Vec2 mouse = input_.GetMousePosition();
                const bool clicked = input_.IsMouseButtonPressed(Engine::Input::Mouse_Left);
                std::optional<std::uint32_t> clickedItem;
                if (!window.IsCursorCaptured() && clicked)
                {
                    for (const Runtime::ScreenTarget& target : screenTargets_)
                    {
                        if (mouse.x >= target.min.x && mouse.x <= target.max.x && mouse.y >= target.min.y &&
                            mouse.y <= target.max.y)
                        {
                            clickedItem = target.objectId;
                        }
                    }
                }
                if (clickedItem)
                {
                    runtime_->ScreenClicked(*clickedItem);
                }
                const std::optional<bool> asked = runtime_->TakeMouseRequest();
                if (window.IsCursorCaptured() && (asked == false || input_.IsKeyPressed(Engine::Input::Key_Escape)))
                {
                    window.SetCursorCaptured(false);
                }
                else if (!window.IsCursorCaptured() &&
                         (asked == true || (firstFrame_ && screenTargets_.empty()) || (clicked && !clickedItem)))
                {
                    window.SetCursorCaptured(true);
                }
                firstFrame_ = false;
                input_.SetPointerLocked(window.IsCursorCaptured());

                // No editor UI here, so nothing captures the input: the player gets every key
                const Engine::Input::GameInput gameInput(input_, runtime_->Actions());
                runtime_->Tick(deltaSeconds, window, &gameInput, &GetAudio());
                // The Game blocks' window settings, and quitting
                for (const auto& request : runtime_->TakeGameRequests())
                {
                    using Kind = Engine::Logic::LogicRunner::GameRequest::Kind;
                    switch (request.kind)
                    {
                    case Kind::Fullscreen: window.SetFullscreen(request.a != 0.0f); break;
                    case Kind::WindowSize:
                        window.SetClientSize(static_cast<int>(request.a), static_cast<int>(request.b));
                        break;
                    case Kind::Quit: Close(); break;
                    }
                }

                // The HUD over the whole window
                imgui_.BeginFrame(window, deltaSeconds);
                const float scale = std::clamp(window.ContentScale(), 0.5f, 4.0f);
                screenTargets_ = Runtime::DrawGameHud(
                    ImGui::GetForegroundDrawList(),
                    {.min = ImVec2(0.0f, 0.0f),
                     .size = ImVec2(static_cast<float>(window.Width()), static_cast<float>(window.Height())),
                     .scale = scale,
                     .pointerLocked = window.IsCursorCaptured(),
                     .mouse = ImVec2(mouse.x, mouse.y)},
                    *runtime_);
                imgui_.EndFrame();
                input_.Update();
            }

            void OnShutdown() override
            {
                GetWindow().SetCursorCaptured(false);
                runtime_.reset();
                imgui_.Shutdown();
                GetWindow().AttachInput(nullptr); // input_ is destroyed before the window
            }

        private:
            Engine::Game::GameProject project_;
            std::filesystem::path scene_;
            Engine::Input::InputSystem input_;
            Engine::ImGuiLayer::ImGuiLayer imgui_;
            bool firstFrame_ = true;
            std::vector<Runtime::ScreenTarget> screenTargets_; // what can be clicked on the screen, last frame
            std::unique_ptr<Engine::Game::GameRuntime> runtime_;
        };
    }

    // Entry point for --play and for built games
    int RunGame(const std::filesystem::path& gameDir, const std::filesystem::path& scene)
    {
        if (!Engine::Game::GameProject::IsGameProject(gameDir))
        {
            std::cerr << "ERROR: not a game project (no configs/game.cfg): " << gameDir.string() << "\n";
            return 1;
        }

        // A corrupted scene is written to the log when GameRuntime loads it (the game still starts)
        const Engine::Game::GameProject project(gameDir);

        RuntimeApp app{project, scene};
        app.Run();
        return 0;
    }
}
