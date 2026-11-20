/* !
@file    	    SceneEditorSounds.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		03/11/2026
@brief
    Implements the editor's own sounds: a click for every mouse click on the
    editor (but not on the game while it plays, which makes its own), a snap
    when blocks are let go where they attach to other blocks, a pluck when a
    block is pulled out of the palette in the Simple interface, and a success
    sound when the learner gets a step of a lesson done. The clips are copied
    next to the exe (assets/sounds) and played on the SFX bus.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "engine/core/Log.hpp"
#include "engine/platform/Paths.hpp"

#include <imgui/imgui.h>

#include <filesystem>

namespace Editor
{
    void SceneEditor::LoadEditorSounds()
    {
        if (audioSystem_ == nullptr)
        {
            return;
        }
        const std::filesystem::path folder = Engine::Platform::ExecutableDir() / "assets" / "sounds";
        const auto load = [this, &folder](const char* file)
        {
            const std::filesystem::path path = folder / file;
            if (!std::filesystem::exists(path))
            {
                LOG_WARN(Editor, "No editor sound at \"{}\": it stays silent", path.string());
                return Engine::Audio::SoundId{};
            }
            return audioSystem_->LoadSound(path.string());
        };
        clickSound_ = load("Mouse_Click.wav");
        attachSound_ = load("Block_Attached_SFX.wav");
        pluckSound_ = load("Plucking.wav");
        successSound_ = load("Success_clear.wav");
    }

    // Called every frame once ImGui has this frame's input
    void SceneEditor::PlayClickSounds()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.MouseClicked[ImGuiMouseButton_Left] && !io.MouseClicked[ImGuiMouseButton_Right])
        {
            return;
        }
        // While the game plays, a click on the Viewport (or with the pointer locked to it) belongs to the game
        if (playing_ && (pointerLocked_ || viewportHovered_))
        {
            return;
        }
        PlayEditorSound(clickSound_);
    }

    void SceneEditor::PlayEditorSound(Engine::Audio::SoundId sound) const
    {
        if (audioSystem_ != nullptr && sound.IsValid())
        {
            audioSystem_->Play(sound);
        }
    }

    // A lesson's steps are checked every frame. Steps found done in the frame a lesson opens, or right after its
    // ticks were cleared (Previous, Next lesson, Reset), were done before: only steps done while the learner works
    // on the lesson play the sound, once a frame however many tick off together.
    void SceneEditor::LessonStepsDone(std::size_t newlyDone)
    {
        const std::pair lesson{activeMode_, openTutorial_};
        const std::size_t ticksBefore = lessonTicked_.size() - newlyDone;
        if (newlyDone > 0 && heardLesson_ == lesson && ticksBefore >= heardTicks_)
        {
            PlayEditorSound(successSound_);
        }
        heardLesson_ = lesson;
        heardTicks_ = lessonTicked_.size();
    }
}
