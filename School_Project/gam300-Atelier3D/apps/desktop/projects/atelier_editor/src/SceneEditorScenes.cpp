/* !
@file    	    SceneEditorScenes.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    The scene manager (the Scenes window, and the Simple interface's Scenes
    tab): the game's scenes, which one it starts with, and new, renamed,
    copied and deleted scenes. Also deleting a whole game.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "editor/RecycleBin.hpp"
#include "editor/Theme.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/scene/SceneFile.hpp"

#include <imgui/imgui.h>

#include <array>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace Editor
{
    namespace
    {
        constexpr const char* kScenesFolder = "data/scenes";
        constexpr std::array<std::string_view, 1> kSceneExtensions{".json"};

        // Deleted scenes go here when there is no Recycle Bin: in the game, but not among its scenes
        std::filesystem::path DeletedFolder(const std::filesystem::path& game)
        {
            return game / ".deleted";
        }
    }

    void SceneEditor::DrawScenes()
    {
        if (projectDir_.empty())
        {
            ImGui::TextDisabled("Open a game to see its scenes.");
            return;
        }
        const float s = uiScale_;
        const Engine::Game::GameProject project(projectDir_);
        const std::string start = project.Settings().startScene;
        const std::vector<std::string> scenes = project.FindAssets(kScenesFolder, kSceneExtensions);

        ImGui::PushTextWrapPos(0.0f);
        if (playing_ && runtime_)
        {
            const std::string running = !runtime_->SwitchedScene().empty() ? runtime_->SwitchedScene()
                                        : scenePath_.empty()
                                            ? std::string("this scene")
                                            : std::filesystem::relative(scenePath_, projectDir_).generic_string();
            ImGui::TextColored(Theme::kSuccessText, "Playing: %s",
                               std::filesystem::path(running).stem().string().c_str());
            ImGui::TextDisabled("A \"go to scene\" block moves the running game to another scene: a menu, the next "
                                "level, a game over screen.");
        }
        else
        {
            ImGui::TextDisabled("Each scene is a place in your game: a menu, a level, a game over screen. The game "
                                "starts with the one marked START, and a \"go to scene\" block moves to another.");
        }
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        ImGui::BeginDisabled(playing_ || InPrefabMode());
        if (Theme::PillButton("New scene", Theme::kObjects, Theme::kInk, 34.0f * s, 0.0f, true, Theme::Icon::Plus))
        {
            sceneNaming_ = SceneNaming::New;
            sceneName_.fill('\0');
            openSceneNaming_ = true;
        }
        ImGui::EndDisabled();
        ImGui::Spacing();

        for (const std::string& relative : scenes)
        {
            const std::filesystem::path path = projectDir_ / relative;
            const std::string name = std::filesystem::path(relative).stem().string();
            const bool isStart = relative == start;
            const bool isOpen = path == scenePath_;
            ImGui::PushID(relative.c_str());
            const bool clicked = ImGui::Selectable("##scene", isOpen, ImGuiSelectableFlags_AllowDoubleClick,
                                                   ImVec2(0.0f, ImGui::GetFrameHeight()));
            const bool open = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("%s\nDouble-click to open it; right-click for more", relative.c_str());
            }
            bool openNow = open && !isOpen && !playing_;
            if (ImGui::BeginPopupContextItem("##SceneMenu"))
            {
                ImGui::TextDisabled("%s", name.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Open", nullptr, false, !isOpen && !playing_))
                {
                    openNow = true;
                }
                if (ImGui::MenuItem("Start the game here", nullptr, isStart, !isStart))
                {
                    Engine::Game::GameProject game(projectDir_);
                    Engine::Game::GameSettings settings = game.Settings();
                    settings.startScene = relative;
                    game.SaveSettings(settings);
                    SetStatus("The game now starts with " + name);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename...", nullptr, false, !playing_))
                {
                    sceneNaming_ = SceneNaming::Rename;
                    sceneTarget_ = path;
                    sceneName_.fill('\0');
                    name.copy(sceneName_.data(), sceneName_.size() - 1);
                    openSceneNaming_ = true;
                }
                if (ImGui::MenuItem("Duplicate...", nullptr, false, !playing_))
                {
                    sceneNaming_ = SceneNaming::Duplicate;
                    sceneTarget_ = path;
                    sceneName_.fill('\0');
                    (name + "_copy").copy(sceneName_.data(), sceneName_.size() - 1);
                    openSceneNaming_ = true;
                }
                if (ImGui::MenuItem("Delete...", nullptr, false, !playing_ && !isStart && !isOpen))
                {
                    sceneTarget_ = path;
                    openSceneDelete_ = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled) &&
                    (isStart || isOpen))
                {
                    ImGui::SetTooltip(isStart ? "The game starts with it: start it somewhere else first"
                                              : "It is open: open another scene first");
                }
                ImGui::EndPopup();
            }
            // Its name, and what it is
            ImGui::SameLine(ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextUnformatted(name.c_str());
            if (isStart)
            {
                ImGui::SameLine();
                ImGui::TextColored(Theme::kSuccessText, "START");
            }
            if (isOpen)
            {
                ImGui::SameLine();
                ImGui::TextDisabled(HasUnsavedChanges() ? "open, not saved" : "open");
            }
            ImGui::PopID();
            if (openNow)
            {
                AskToSave([this, path] { LoadSceneFile(path); });
            }
        }
        if (scenes.empty())
        {
            ImGui::TextDisabled("No scenes yet: save this one, or make a new one.");
        }
        DrawScenePopups();
    }

    void SceneEditor::DrawScenePopups()
    {
        if (openSceneNaming_)
        {
            ImGui::OpenPopup("##SceneName");
            openSceneNaming_ = false;
            sceneError_.clear();
        }
        if (ImGui::BeginPopupModal("##SceneName", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
        {
            const std::string from = std::filesystem::path(sceneTarget_).stem().string();
            Theme::Text(sceneNaming_ == SceneNaming::New      ? "New scene"
                        : sceneNaming_ == SceneNaming::Rename ? std::format("Rename {}", from).c_str()
                                                              : std::format("Copy {}", from).c_str(),
                        18.0f, Theme::kText, true);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(260.0f * uiScale_);
            const bool enter = ImGui::InputTextWithHint("##name", "Name, e.g. level2", sceneName_.data(),
                                                        sceneName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::TextDisabled("Letters, digits, '_' and '-'");
            if (!sceneError_.empty())
            {
                ImGui::TextColored(Theme::kErrorText, "%s", sceneError_.c_str());
            }
            const bool ok = ImGui::Button("OK") || enter;
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                ImGui::CloseCurrentPopup();
            }
            else if (ok)
            {
                const std::string name = sceneName_.data();
                const std::filesystem::path target = projectDir_ / kScenesFolder / (name + ".json");
                std::error_code ec;
                if (!Engine::Game::GameProject::IsValidName(name))
                {
                    sceneError_ = "That name cannot be used";
                }
                else if (std::filesystem::exists(target, ec))
                {
                    sceneError_ = "There is a scene called that already";
                }
                else
                {
                    try
                    {
                        std::filesystem::create_directories(target.parent_path());
                        const std::string relative = std::filesystem::relative(target, projectDir_).generic_string();
                        if (sceneNaming_ == SceneNaming::New)
                        {
                            Engine::Scene::SaveSceneFile(target, Engine::Scene::SceneDescription{});
                            AskToSave([this, target] { LoadSceneFile(target); });
                        }
                        else if (sceneNaming_ == SceneNaming::Duplicate)
                        {
                            std::filesystem::copy_file(sceneTarget_, target);
                            SetStatus("Copied " + from + " to " + name);
                        }
                        else
                        {
                            std::filesystem::rename(sceneTarget_, target);
                            // The game's start scene and the open one follow it
                            Engine::Game::GameProject game(projectDir_);
                            Engine::Game::GameSettings settings = game.Settings();
                            if (projectDir_ / settings.startScene == sceneTarget_)
                            {
                                settings.startScene = relative;
                                game.SaveSettings(settings);
                            }
                            if (scenePath_ == sceneTarget_)
                            {
                                scenePath_ = target;
                            }
                            SetStatus(std::format("Renamed {} to {} (\"go to scene\" blocks that use the old name "
                                                  "need the new one)",
                                                  from, name));
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    catch (const std::exception& e)
                    {
                        sceneError_ = e.what();
                    }
                }
            }
            ImGui::EndPopup();
        }

        if (openSceneDelete_)
        {
            ImGui::OpenPopup("##DeleteScene");
            openSceneDelete_ = false;
        }
        if (ImGui::BeginPopupModal("##DeleteScene", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
        {
            const std::string name = std::filesystem::path(sceneTarget_).stem().string();
            Theme::Text(std::format("Delete the scene {}?", name).c_str(), 18.0f, Theme::kText, true);
            ImGui::TextDisabled("It goes to the Recycle Bin, where you can get it back.");
            if (ImGui::Button("Delete"))
            {
                std::string error;
                if (MoveToRecycleBin(sceneTarget_, DeletedFolder(projectDir_), error))
                {
                    SetStatus("Deleted the scene " + name);
                }
                else
                {
                    SetStatus("Could not delete " + name + ": " + error, true);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // The Home page's (or Project menu's) Delete Game...: asks first, then deletes the whole game folder
    void SceneEditor::DrawDeleteGamePopup()
    {
        if (openDeleteGame_)
        {
            ImGui::OpenPopup("##DeleteGame");
            openDeleteGame_ = false;
        }
        if (!ImGui::BeginPopupModal("##DeleteGame", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
        {
            return;
        }
        const std::string name = deleteGame_.filename().string();
        Theme::Text(std::format("Delete the game {}?", name).c_str(), 20.0f, Theme::kText, true);
        ImGui::Spacing();
        ImGui::TextDisabled("All of it goes to the Recycle Bin: its scenes, blocks, pictures and sounds.");
        ImGui::TextDisabled("You can get it back from there.");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kError);
        const bool remove = ImGui::Button("Delete");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            ImGui::CloseCurrentPopup();
        }
        if (remove)
        {
            ImGui::CloseCurrentPopup();
            DeleteGame(deleteGame_);
        }
        ImGui::EndPopup();
    }

    void SceneEditor::DeleteGame(const std::filesystem::path& dir)
    {
        const std::string name = dir.filename().string();
        if (dir == projectDir_)
        {
            // Let go of the game completely first: nothing may write into its folder again (switching games saves
            // the old one's lesson notes, which would make the folder again), and its sounds and pictures are closed
            // so the folder can go
            StopPlay();
            if (InPrefabMode())
            {
                LeavePrefabMode();
            }
            runtime_.reset();
            runtimeProject_.clear();
            projectDir_.clear();
            prefabs_.SetProjectDir({});
            prefabList_.clear();
            NewScene();
            saved_ = scene_.CaptureState();
        }
        FreeThumbnails();

        // Into the Recycle Bin; if it is still there after that, deleted for good
        std::string error;
        bool recycled = MoveToRecycleBin(dir, {}, error);
        std::error_code ec;
        if (std::filesystem::exists(dir, ec))
        {
            recycled = false;
            std::filesystem::remove_all(dir, ec);
            if (ec || std::filesystem::exists(dir))
            {
                error = ec ? ec.message() : std::string("some of its files are in use");
            }
        }
        const bool gone = !std::filesystem::exists(dir, ec);

        RefreshProjects(); // the Home page's list, without it (and another game opens if it was the open one)
        // Said last, so the game that opens instead does not replace it
        if (!gone)
        {
            SetStatus(std::format("Could not delete {}: {}", name, error), true);
        }
        else if (recycled)
        {
            SetStatus(std::format("Deleted {}. It is in the Recycle Bin if you want it back.", name));
        }
        else
        {
            SetStatus(std::format("Deleted {}.", name));
        }
    }
}
