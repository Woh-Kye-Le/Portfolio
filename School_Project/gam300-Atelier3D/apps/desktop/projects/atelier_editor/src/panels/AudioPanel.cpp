/* !
@file    	    AudioPanel.cpp
@author  	    k.thantwin@digipen.edu [68%]
                sanni.go@digipen.edu [20%]
         	    c.kojat@digipen.edu [12%]
@date    		29/09/2026
@brief
    Implements AudioPanel: the sound bank rows, the master and bus mixer, the
    play-any-file test, and the 3D emitter with its top-down radar.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/audio/AudioPanel.hpp"

#include "editor/Theme.hpp"
#include "engine/platform/Paths.hpp"

#include <imgui/imgui.h>

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace Editor::Panels
{
    namespace
    {
        // On a dev build, read the config out of the source tree: that is the
        // file you actually edit and commit, so editing it and pressing reload
        // needs no rebuild. ATELIER_EDITOR_ASSETS_DIR is baked in by CMake and
        // simply will not exist on another machine, where the copy next to the
        // exe takes over.
        std::filesystem::path AudioDir()
        {
#ifdef ATELIER_EDITOR_ASSETS_DIR
            std::filesystem::path fromSource = std::filesystem::path(ATELIER_EDITOR_ASSETS_DIR) / "audio";
            if (std::filesystem::exists(fromSource))
            {
                return fromSource;
            }
#endif
            return Engine::Platform::ExecutableDir() / "assets" / "audio";
        }

        std::string BankPath()
        {
            return (AudioDir() / "sounds.json").string();
        }

        // A sound file that exists on most machines, so "Play file" works without adding any assets
        std::string DefaultSoundFile()
        {
            for (const char* candidate : {"C:/Windows/Media/tada.wav", "C:/Windows/Media/chimes.wav",
                                          "/usr/share/sounds/alsa/Front_Center.wav"})
            {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec))
                {
                    return candidate;
                }
            }
            return {};
        }
    }

    /*!
    \brief   Creates the Audio window.
    \param   audio The audio system the window shows and controls.
    */
    AudioPanel::AudioPanel(Engine::Audio::AudioSystem& audio) : Panel("Audio"), audio_(audio)
    {
        std::snprintf(filePath_.data(), filePath_.size(), "%s", DefaultSoundFile().c_str());
    }

    /*!
    \brief   Draws the file test: plays any sound file by its path on the SFX bus, outside the sound bank.
    */
    void AudioPanel::DrawFileTest()
    {
        ImGui::TextUnformatted("Any sound file (WAV, MP3, OGG, FLAC, ...), on the SFX bus:");
        ImGui::InputText("File", filePath_.data(), filePath_.size());
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("File volume", &fileVolume_, 0.0f, 1.0f, "%.2f"))
        {
            audio_.SetVoiceVolume(fileVoice_, fileVolume_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play file"))
        {
            // One file at a time; the clip stays loaded (AudioSystem caches it by path)
            audio_.Stop(fileVoice_);
            const Engine::Audio::SoundId sound = audio_.LoadSound(filePath_.data());
            fileVoice_ = sound.IsValid() ? audio_.Play(sound) : Engine::Audio::VoiceId{};
            audio_.SetVoiceVolume(fileVoice_, fileVolume_);
            fileError_ = fileVoice_.IsValid() ? std::string{}
                                              : std::string("Could not play ") + filePath_.data() + " (see Console)";
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop##file"))
        {
            audio_.Stop(fileVoice_);
        }
        ImGui::SameLine();
        ImGui::Text("playing: %s", audio_.IsPlaying(fileVoice_) ? "yes" : "no");
        if (!fileError_.empty())
        {
            ImGui::TextColored(Theme::kErrorText, "%s", fileError_.c_str());
        }
    }

    /*!
    \brief   Loads the sound bank from sounds.json and makes one voice slot per entry.
    */
    void AudioPanel::LoadBank()
    {
        loaded_ = true;
        lastEntryCount_ = audio_.LoadSoundBank(BankPath());
        voices_.assign(audio_.SoundBank().size(), {});
    }

    /*!
    \brief   Draws the mute tickbox and volume slider for one bus.
    \param   bus The bus to draw.
    */
    void AudioPanel::DrawBus(Engine::Audio::Bus bus)
    {
        ImGui::PushID(static_cast<int>(bus));

        bool muted = audio_.IsBusMuted(bus);
        if (ImGui::Checkbox("##mute", &muted))
        {
            audio_.SetBusMuted(bus, muted);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Mute this bus");
        }

        ImGui::SameLine();

        float volume = audio_.GetBusVolume(bus);
        if (ImGui::SliderFloat(Engine::Audio::BusName(bus), &volume, 0.0f, 1.0f))
        {
            audio_.SetBusVolume(bus, volume);
        }

        ImGui::PopID();
    }

    /*!
    \brief   Draws a top-down view of the listener and the 3D emitter.
    */
    void AudioPanel::DrawRadar()
    {
        const glm::vec3 listener = audio_.ListenerPosition();
        const glm::vec3 forward = audio_.ListenerForward();

        const float size = 180.0f;
        const float half = size * 0.5f;
        const float range = 20.0f; // world units across half the radar
        const float scale = half / range;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 centre(origin.x + half, origin.y + half);
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(18, 20, 26, 255));
        for (int ring = 1; ring <= 3; ++ring)
        {
            draw->AddCircle(centre, half * (static_cast<float>(ring) / 3.0f), IM_COL32(60, 66, 78, 255), 48);
        }

        // Listener at the centre, with a stub showing where it is facing.
        const float len = glm::length(glm::vec2(forward.x, forward.z));
        const glm::vec2 facing = len > 0.0001f ? glm::vec2(forward.x, forward.z) / len : glm::vec2(0.0f, -1.0f);
        draw->AddLine(centre, ImVec2(centre.x + facing.x * 26.0f, centre.y + facing.y * 26.0f),
                      IM_COL32(120, 200, 255, 255), 2.0f);
        draw->AddCircleFilled(centre, 5.0f, IM_COL32(120, 200, 255, 255));

        // Emitter, clamped to the edge so it never disappears off the radar.
        glm::vec2 offset((emitterPosition_.x - listener.x) * scale, (emitterPosition_.z - listener.z) * scale);
        if (glm::length(offset) > half - 6.0f)
        {
            offset = glm::normalize(offset) * (half - 6.0f);
        }
        draw->AddCircleFilled(ImVec2(centre.x + offset.x, centre.y + offset.y), 6.0f, IM_COL32(240, 170, 60, 255));

        ImGui::Dummy(ImVec2(size, size));
    }

    /*!
    \brief   Draws the 3D emitter section: plays the sound bank's "emitter" entry and moves it around the
             listener, either in orbit or by hand, and shows its distance and left/right balance.
    */
    void AudioPanel::DrawSpatial()
    {
        ImGui::SeparatorText("3D emitter");

        const Engine::Audio::SoundId emitter = audio_.FindSound("emitter");
        if (!emitter.IsValid())
        {
            ImGui::TextDisabled("No \"emitter\" entry in the sound bank.");
            return;
        }

        if (orbit_)
        {
            orbitAngle_ += ImGui::GetIO().DeltaTime * orbitSpeed_ * 2.0f * 3.14159265f;
            const glm::vec3 listener = audio_.ListenerPosition();
            emitterPosition_ =
                listener + glm::vec3(std::cos(orbitAngle_) * orbitRadius_, 0.0f, std::sin(orbitAngle_) * orbitRadius_);
        }

        audio_.SetVoicePosition(emitterVoice_, emitterPosition_);

        const bool playing = audio_.IsPlaying(emitterVoice_);
        if (ImGui::Button(playing ? "Stop emitter" : "Play emitter", ImVec2(130.0f, 0.0f)))
        {
            if (playing)
            {
                audio_.Stop(emitterVoice_);
            }
            else
            {
                emitterVoice_ = audio_.PlayNamedAt("emitter", emitterPosition_);
            }
        }

        ImGui::SameLine();
        ImGui::Checkbox("Orbit the listener", &orbit_);

        if (orbit_)
        {
            ImGui::SliderFloat("Radius", &orbitRadius_, 1.0f, 20.0f);
            ImGui::SliderFloat("Turns/sec", &orbitSpeed_, 0.05f, 1.5f);
        }
        else
        {
            ImGui::SliderFloat("X", &emitterPosition_.x, -20.0f, 20.0f);
            ImGui::SliderFloat("Z", &emitterPosition_.z, -20.0f, 20.0f);
        }

        // Distance and left/right balance, both computed the same way FMOD does
        // it: relative to the listener's own right vector.
        const glm::vec3 listener = audio_.ListenerPosition();
        const glm::vec3 toEmitter = emitterPosition_ - listener;
        const float distance = glm::length(toEmitter);

        const glm::vec3 right = glm::normalize(glm::cross(audio_.ListenerForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
        const float balance = distance > 0.001f ? glm::dot(glm::normalize(toEmitter), right) : 0.0f;

        DrawRadar();

        ImGui::Text("Distance: %.1f m", static_cast<double>(distance));
        ImGui::Text("Balance:  %s", balance < -0.15f ? "<<< left" : (balance > 0.15f ? "right >>>" : "centre"));
        ImGui::ProgressBar((balance + 1.0f) * 0.5f, ImVec2(220.0f, 0.0f), "L / R");
    }

    /*!
    \brief   Draws one sound bank entry: its name and file, how far through it is, and its Play and Stop
             buttons.
    \param   index Index of the entry in AudioSystem::SoundBank().
    */
    void AudioPanel::DrawRow(std::size_t index)
    {
        const Engine::Audio::SoundDef& def = audio_.SoundBank()[index];

        ImGui::PushID(static_cast<int>(index));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(def.name.c_str());
        ImGui::SameLine(80.0f);
        ImGui::TextDisabled("%-22s", def.file.c_str());
        ImGui::SameLine(270.0f);

        if (!def.sound.IsValid())
        {
            ImGui::TextDisabled("missing: %s", def.file.c_str());
            ImGui::PopID();
            return;
        }

        const Engine::Audio::VoiceId voice = voices_[index];
        const bool playing = audio_.IsPlaying(voice);

        // The bar is the point of this panel: several filling at once is what
        // shows nothing got cut off.
        ImGui::ProgressBar(playing ? audio_.GetVoiceProgress(voice) : 0.0f, ImVec2(120.0f, 0.0f),
                           playing ? nullptr : "idle");

        ImGui::SameLine();
        if (ImGui::Button("Play"))
        {
            voices_[index] = audio_.PlayNamed(def.name);
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            audio_.Stop(voice);
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s%s%s", def.file.c_str(), def.loop ? "  (loop)" : "", def.stream ? "  (stream)" : "");
        }

        ImGui::PopID();
    }

    /*!
    \brief   Draws the whole Audio window: the backend and voice counts, one row per sound bank entry, the
             mixer, and the file test and 3D emitter sections.
    */
    void AudioPanel::DrawContents()
    {
        ImGui::Text("Backend: %s", Engine::Audio::AudioSystem::BackendName());

        if (!audio_.IsAvailable())
        {
            ImGui::TextUnformatted("Unavailable - the app is running silently.");
            return;
        }

        if (!loaded_)
        {
            LoadBank();
        }

        ImGui::SameLine();
        ImGui::Text("   Voices: %d / %d   (music %d, sfx %d)", audio_.ActiveVoiceCount(),
                    Engine::Audio::AudioSystem::MaxVoices, audio_.BusVoiceCount(Engine::Audio::Bus::Music),
                    audio_.BusVoiceCount(Engine::Audio::Bus::Sfx));

        const auto& bank = audio_.SoundBank();

        ImGui::Separator();
        ImGui::TextUnformatted("Music");
        for (std::size_t i = 0; i < bank.size(); ++i)
        {
            if (bank[i].bus == "music")
            {
                DrawRow(i);
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Sound effects");
        for (std::size_t i = 0; i < bank.size(); ++i)
        {
            if (bank[i].bus != "music")
            {
                DrawRow(i);
            }
        }

        ImGui::Separator();

        if (ImGui::Button("FIRE ALL SFX", ImVec2(150.0f, 30.0f)))
        {
            for (std::size_t i = 0; i < bank.size(); ++i)
            {
                if (bank[i].bus != "music")
                {
                    voices_[i] = audio_.PlayNamed(bank[i].name);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop all", ImVec2(90.0f, 30.0f)))
        {
            audio_.StopAll();
        }

        // Mixer. Muting one bus leaves the other playing - that is the whole
        // point of the groups, and the quickest way to show it on camera.
        if (ImGui::SliderFloat("Master", &masterVolume_, 0.0f, 1.0f))
        {
            audio_.SetMasterVolume(masterVolume_);
        }

        DrawBus(Engine::Audio::Bus::Music);
        DrawBus(Engine::Audio::Bus::Sfx);

        DrawSpatial();

        ImGui::Separator();

        // The hot-swap half of the M1 requirement: edit sounds.json or replace
        // a wav on disk, press this, play again. No rebuild.
        if (ImGui::Button("Reload sounds.json", ImVec2(160.0f, 0.0f)))
        {
            lastEntryCount_ = audio_.ReloadSoundBank();
            voices_.assign(audio_.SoundBank().size(), {});
            ++reloadCount_;
        }

        ImGui::SameLine();
        ImGui::Text("reloads: %d   entries: %d", reloadCount_, lastEntryCount_);

        ImGui::Separator();
        DrawFileTest();
    }
}
