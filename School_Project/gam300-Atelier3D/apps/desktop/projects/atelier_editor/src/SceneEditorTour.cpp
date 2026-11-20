/* !
@file    	    SceneEditorTour.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		05/11/2026
@brief
    Implements the first-time tour: the screen darkened around what a step is
    about, with its words beside it. Also the lesson game, which lessons always
    happen in, and ticking a lesson's steps off as their checks come true.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "editor/Theme.hpp"
#include "editor/Tour.hpp"
#include "engine/editor/LessonGoals.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/logic/LogicRunner.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <span>
#include <string>

namespace Editor
{
    namespace
    {
        // The game every lesson happens in, so a lesson never shows a game of the learner's own
        constexpr const char* kLessonGame = "My_Lessons";

        std::span<const Tour::Step> StepsOf(bool advanced)
        {
            return advanced ? std::span<const Tour::Step>(Tour::kAdvancedSteps)
                            : std::span<const Tour::Step>(Tour::kSteps);
        }
    }

    // ---------------------------------------------------------------- The tour

    void SceneEditor::TourMark(const char* target, ImVec2 min, ImVec2 max)
    {
        if (tourStep_ >= 0)
        {
            tourTargets_[target] = {min, max};
        }
    }

    void SceneEditor::StartTour()
    {
        tourStep_ = -1;
        tourAdvanced_ = false;
        GoToTourStep(0);
    }

    void SceneEditor::StartAdvancedTour()
    {
        tourStep_ = -1;
        tourAdvanced_ = true;
        GoToTourStep(0);
    }

    void SceneEditor::EndTour()
    {
        tourStep_ = -1;
        tourTargets_.clear();
        if (tourAdvanced_)
        {
            advancedTourDone_ = true; // and the workspace stays open, on the same game
        }
        else
        {
            tourDone_ = true;
            view_ = EditorView::Home;
        }
        tourAdvanced_ = false;
        ImGui::MarkIniSettingsDirty();
    }

    void SceneEditor::GoToTourStep(int step)
    {
        tourStep_ = step;
        if (tourAdvanced_)
        {
            // The window this step is about, in front of the others in its tab group
            view_ = EditorView::Workspace;
            viewportMaximized_ = false;
            const char* target = Tour::kAdvancedSteps[static_cast<std::size_t>(step)].target;
            if (target && ImGui::FindWindowByName(target))
            {
                ImGui::SetWindowFocus(target);
            }
            return;
        }
        if (!Tour::kSteps[static_cast<std::size_t>(step)].workspace)
        {
            view_ = EditorView::Home;
            return;
        }
        // The workspace, on the empty lesson game when that can be done without asking anything
        StopPlay();
        if (!HasUnsavedChanges())
        {
            OpenLessonGame();
        }
        view_ = EditorView::Workspace;
        viewportMaximized_ = false;
        simpleLeftTab_ = 0;
    }

    void SceneEditor::DrawTour()
    {
        if (tourStep_ < 0)
        {
            if (!tourDone_ && interface_ == InterfaceMode::Simple)
            {
                StartTour();
            }
            else if (!advancedTourDone_ && interface_ == InterfaceMode::Advanced && view_ == EditorView::Workspace &&
                     !playing_)
            {
                StartAdvancedTour();
            }
            return;
        }
        const float s = uiScale_;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const std::span<const Tour::Step> steps = StepsOf(tourAdvanced_);
        const int lastStep = static_cast<int>(steps.size()) - 1;
        const Tour::Step& step = steps[static_cast<std::size_t>(tourStep_)];
        const auto found = step.target ? tourTargets_.find(step.target) : tourTargets_.end();
        const bool lit = found != tourTargets_.end();

        // A window over everything: it takes the clicks, and is what the screen is darkened on
        constexpr ImGuiWindowFlags kCover = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                            ImGuiWindowFlags_NoBackground;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(display);
        ImGui::Begin("##TourCover", nullptr, kCover);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::InvisibleButton("##cover", display);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 dark = IM_COL32(20, 20, 35, 170);
        if (lit)
        {
            // Four strips round the hole, and a soft pulsing edge so the eye goes to it
            const float pad = 6.0f * s;
            const ImVec2 min(found->second.first.x - pad, found->second.first.y - pad);
            const ImVec2 max(found->second.second.x + pad, found->second.second.y + pad);
            drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x, min.y), dark);
            drawList->AddRectFilled(ImVec2(0.0f, max.y), display, dark);
            drawList->AddRectFilled(ImVec2(0.0f, min.y), ImVec2(min.x, max.y), dark);
            drawList->AddRectFilled(ImVec2(max.x, min.y), ImVec2(display.x, max.y), dark);
            const float pulse =
                Theme::AnimationsOn() ? 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) : 1.0f;
            const ImVec4 ring = Theme::kEvents;
            drawList->AddRect(min, max, ImGui::GetColorU32(ImVec4(ring.x, ring.y, ring.z, 0.55f + 0.45f * pulse)),
                              10.0f * s, ImDrawFlags_None, 3.0f * s);
        }
        else
        {
            drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, dark);
        }
        ImGui::End();

        // Its words, beside what it points at. As tall as they were last frame: they are only measured once drawn.
        const float width = 340.0f * s;
        const float gap = 16.0f * s;
        Tour::Box box{(display.x - width) * 0.5f, (display.y - tourHeight_) * 0.5f, width, tourHeight_};
        if (lit)
        {
            const ImVec2 min = found->second.first;
            const ImVec2 max = found->second.second;
            box = Tour::PlaceBeside({min.x, min.y, max.x - min.x, max.y - min.y}, width, tourHeight_, display.x,
                                    display.y, gap);
        }
        ImGui::SetNextWindowPos(ImVec2(box.x, box.y));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kPanel);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f * s, 18.0f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * s);
        ImGui::Begin("##TourWords", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoDocking);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        Theme::Text(std::format("{} OF {}", tourStep_ + 1, lastStep + 1).c_str(), 12.0f, Theme::kTextMuted, true);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX());
        Theme::Text(step.title, 22.0f, Theme::kText, true);
        ImGui::Spacing();
        Theme::TextWrapped(step.text, 15.0f, Theme::kTextSoft);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, 8.0f * s));

        const float height = 40.0f * s;
        const bool last = tourStep_ == lastStep;
        const char* forward = !last ? "Next" : tourAdvanced_ ? "Start building" : "Start learning";
        if (!last && Theme::PillButton("Skip", Theme::kGround, Theme::kTextSoft, height))
        {
            EndTour();
        }
        else
        {
            if (!last)
            {
                ImGui::SameLine();
            }
            const float next = Theme::PillWidth(forward, height, true);
            const float back =
                tourStep_ > 0 ? Theme::PillWidth("Back", height) + ImGui::GetStyle().ItemSpacing.x : 0.0f;
            const float room = ImGui::GetContentRegionAvail().x - next - back;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(room, 0.0f));
            if (tourStep_ > 0 && Theme::PillButton("Back", Theme::kRaised, Theme::kText, height))
            {
                GoToTourStep(tourStep_ - 1);
            }
            else
            {
                if (tourStep_ > 0)
                {
                    ImGui::SameLine();
                }
                if (Theme::PillButton(forward, Theme::kEvents, Theme::kInk, height, 0.0f, true, Theme::Icon::Arrow) ||
                    ImGui::IsKeyPressed(ImGuiKey_Enter))
                {
                    if (last)
                    {
                        EndTour();
                    }
                    else
                    {
                        GoToTourStep(tourStep_ + 1);
                    }
                }
            }
        }
        if (tourStep_ >= 0)
        {
            tourHeight_ = ImGui::GetWindowHeight();
        }
        ImGui::End();
    }

    // ---------------------------------------------------------------- The lesson game

    std::filesystem::path SceneEditor::LessonGameDir() const
    {
        return Engine::Game::GameProject::ProjectsRoot() / kLessonGame;
    }

    bool SceneEditor::InLessonGame() const
    {
        return projectDir_.filename() == kLessonGame;
    }

    bool SceneEditor::OpenLessonGame()
    {
        if (InLessonGame())
        {
            return true;
        }
        try
        {
            const std::filesystem::path root = Engine::Game::GameProject::ProjectsRoot();
            std::error_code ec;
            std::filesystem::path dir = LessonGameDir();
            if (!std::filesystem::is_directory(dir, ec))
            {
                dir = Engine::Game::GameProject::Create(root, kLessonGame, Engine::Game::GameTemplate::Empty)
                          .ProjectDir();
            }
            projects_ = Engine::Game::GameProject::FindAll(root);
            OpenProject(dir);
            return InLessonGame();
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Could not make the lesson game: ") + e.what(), true);
            return false;
        }
    }

    // The first game that is the learner's own: the lesson game is reached from Learn, and is not one of them
    std::filesystem::path SceneEditor::FirstOwnGame() const
    {
        const auto found = std::find_if(projects_.begin(), projects_.end(), [](const std::filesystem::path& each)
                                        { return each.filename() != kLessonGame; });
        return found != projects_.end() ? *found : std::filesystem::path{};
    }

    bool SceneEditor::IsLessonGame(const std::filesystem::path& dir) const
    {
        return dir.filename() == kLessonGame;
    }

    void SceneEditor::OpenLesson(int mode, int index)
    {
        // Every way in comes through here. A teacher's own lessons are not locked: they are opened in any order.
        if (modesShipped_ && !Engine::Editor::TutorialNotes::IsUnlocked(modes_, mode, index))
        {
            return;
        }
        // The editor's own lessons happen in the lesson game. A teacher's notes belong to the game they are in.
        if (modesShipped_ && !InLessonGame())
        {
            if (HasUnsavedChanges())
            {
                AskToSave([this, mode, index] { OpenLesson(mode, index); });
                return;
            }
            if (!OpenLessonGame())
            {
                return;
            }
        }
        activeMode_ = mode;
        openTutorial_ = index;
        simpleLeftTab_ = 3;
        view_ = EditorView::Workspace;
        lessonTicked_.clear();
    }

    void SceneEditor::ResetLessons()
    {
        lessonTicked_.clear();
        if (modesShipped_)
        {
            // How far the learner got is kept in the lesson game, so reading the lessons again finds none
            DeleteGame(LessonGameDir());
            SaveModes();
            LoadModes();
            return;
        }
        for (Engine::Editor::TutorialMode& mode : modes_)
        {
            mode.done = 0;
        }
        openTutorial_ = -1;
        modesDirty_ = true;
    }

    // ---------------------------------------------------------------- A lesson's steps

    bool SceneEditor::UpdateLesson()
    {
        const auto& tutorials = modes_[activeMode_].tutorials;
        if (openTutorial_ < 0 || openTutorial_ >= static_cast<int>(tutorials.size()))
        {
            heardLesson_.reset();
            return false;
        }
        // How long this run of the game has lasted, for what takes a moment to see
        const double now = ImGui::GetTime();
        if (!playing_)
        {
            lessonRunStart_ = -1.0;
        }
        else if (lessonRunStart_ < 0.0)
        {
            lessonRunStart_ = now;
        }

        Engine::Editor::GoalWorld world;
        Engine::Scene::Scene* ecs = playing_ && runtime_ ? runtime_->Ecs() : nullptr;
        if (ecs)
        {
            world.seconds = now - lessonRunStart_;
            const auto& objects = scene_.Description().objects;
            const auto find = [&objects](std::string_view name) -> const Engine::Scene::GameObject*
            {
                const auto found =
                    std::find_if(objects.begin(), objects.end(),
                                 [name](const Engine::Scene::GameObject& each) { return each.name == name; });
                return found != objects.end() ? &*found : nullptr;
            };
            const Engine::Logic::LogicRunner* blocks = &runtime_->Blocks();
            world.place = [this, ecs, find](std::string_view name) -> std::optional<std::array<float, 3>>
            {
                const Engine::Scene::GameObject* object = find(name);
                const Engine::Scene::Entity entity = object ? runtime_->EntityOf(object->id) : Engine::Scene::Entity{};
                const auto where = entity ? ecs->GetTransform(entity) : std::nullopt;
                return where ? std::optional(std::array{where->x, where->y, where->z}) : std::nullopt;
            };
            world.variable = [blocks](std::string_view object, std::string_view name)
            { return blocks->VariableOf(object, name); };
            world.ran = [blocks, find](std::string_view name, Engine::Logic::Op op)
            {
                const Engine::Scene::GameObject* object = find(name);
                if (!object || !object->logic)
                {
                    return false;
                }
                const auto active = blocks->ActiveBlocks(object->id);
                return std::any_of(active.begin(), active.end(),
                                   [&](std::uint32_t id)
                                   {
                                       const Engine::Logic::Block* block = Engine::Logic::FindBlock(*object->logic, id);
                                       return block && block->op == op;
                                   });
            };
            world.finished = [blocks, find](std::string_view name)
            {
                const Engine::Scene::GameObject* object = find(name);
                if (!object)
                {
                    return false;
                }
                const auto scripts = blocks->Scripts(object->id);
                return std::any_of(scripts.begin(), scripts.end(),
                                   [](const auto& each) { return each.last != 0 && !each.running; });
            };
        }

        // In order, one thing at a time: a step only counts once those before it do. A step done stays done, though
        // the game stops or a later step takes its blocks away ("delete the repeat", then "add a new repeat").
        const auto& steps = tutorials[static_cast<std::size_t>(openTutorial_)].steps;
        lessonMet_.assign(steps.size(), false);
        bool before = true;
        std::size_t newlyDone = 0;
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            lessonMet_[i] = lessonTicked_.contains(i) ||
                            (before && Engine::Editor::CheckGoal(scene_.Description(), steps[i].check, world));
            if (lessonMet_[i] && lessonTicked_.insert(i).second)
            {
                ++newlyDone;
            }
            before = lessonMet_[i];
        }
        LessonStepsDone(newlyDone);
        return before;
    }
}
