/* !
@file    	    AudioSystem.cpp
@author  	    sung.h@digipen.edu [89%]
         	    k.thantwin@digipen.edu [11%]
@date    		01/10/2026
@brief
    Implements the ECS AudioSystem: loading clips, starting/pausing voices,
    pushing volume, position and range each frame, and stopping all voices.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/AudioSystem.hpp"

#include "engine/audio/AudioSystem.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/scene/Scene.hpp"

namespace Engine::Systems
{
    void AudioSystem::Update(Scene::Scene& scene, Audio::AudioSystem& audio,
                             std::span<const std::filesystem::path> clips, bool playing)
    {
        scene.ForEach<Scene::AudioSourceComponent>(
            [&](Scene::Entity entity, Scene::AudioSourceComponent& source)
            {
                // Missing Transform excludes playback, but still nd pausing
                const auto* transform = scene.TryGetComponent<Scene::TransformComponent>(entity);

                // Play On Start off pauses rather than skips, so unticking it mid-game silences a voice that is
                // already running instead of leaving it playing with nothing to stop it
                if (!playing || transform == nullptr || !source.playOnStart)
                {
                    audio.SetPaused(source.voice, true);
                    return;
                }

                // Wait until audio is ready before consuming the loading attmt
                if (!audio.IsAvailable())
                {
                    return;
                }

                if (!source.sound.IsValid() && !source.loadAttempted && source.clip != 0 && source.clip <= clips.size())
                {
                    source.loadAttempted = true;

                    source.sound = audio.LoadSound(clips[source.clip - 1].string(),
                                                   Audio::SoundLoadOptions{.spatial = source.spatial,
                                                                           .minDistance = source.minDistance,
                                                                           .maxDistance = source.maxDistance});
                }

                if (!source.sound.IsValid())
                {
                    return;
                }

                const bool wantsVoice = source.loop || !source.started;

                if (wantsVoice && !audio.IsPlaying(source.voice))
                {
                    source.voice = audio.Play(source.sound, source.loop, source.bus);

                    if (source.voice.IsValid())
                    {
                        source.started = true;
                    }
                }

                audio.SetPaused(source.voice, false);
                audio.SetVoiceVolume(source.voice, source.volume);

                if (source.spatial)
                {
                    audio.SetVoicePosition(source.voice, glm::vec3(transform->x, transform->y, transform->z));
                    // Per voice and every frame, so the Inspector's distance sliders are heard live and two
                    // entities sharing a clip can have different ranges
                    audio.SetVoiceMinMaxDistance(source.voice, source.minDistance, source.maxDistance);
                }
            });
    }

    // The listener copies the camera every frame
    void AudioSystem::UpdateListener(Audio::AudioSystem& audio, const Graphics::Camera3D& camera)
    {
        audio.SetListener(camera.GetPosition(), camera.GetForward(), camera.GetUp());
    }

    void AudioSystem::StopAll(Scene::Scene& scene, Audio::AudioSystem& audio)
    {
        scene.ForEach<Scene::AudioSourceComponent>(
            [&audio](Scene::Entity, Scene::AudioSourceComponent& source)
            {
                audio.Stop(source.voice);
                source.voice = {};
                source.started = false;
            });
    }
}
