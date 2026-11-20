/* !
@file    	    AudioSystem.hpp
@author  	    sung.h@digipen.edu [79%]
         	    c.kojat@digipen.edu [21%]
@date    		01/10/2026
@brief
    Declares the ECS AudioSystem, which plays each entity's AudioSource through
    FMOD and keeps the listener on the camera.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// The ECS system for AudioSourceComponents: starts, updates and stops each entity's voice through Audio::AudioSystem
// (the FMOD playback backend), and keeps the listener on the camera

#include <filesystem>
#include <span>

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Audio
{
    class AudioSystem;
}

namespace Engine::Graphics
{
    class Camera3D;
}

namespace Engine::Systems
{
    class AudioSystem
    {
    public:
        // Every frame: starts/pauses each AudioSource's voice and pushes its volume, position and range to FMOD.
        // clips is the runtime's clip table (AudioSourceComponent::clip is a 1-based index into it).
        // playing = false (edit mode / paused) pauses everything instead of stopping it.
        static void Update(Scene::Scene& scene, Audio::AudioSystem& audio, std::span<const std::filesystem::path> clips,
                           bool playing);

        // The camera is the "ears", so 3D sounds get quieter/pan based on where it is and which way it faces
        static void UpdateListener(Audio::AudioSystem& audio, const Graphics::Camera3D& camera);

        // Stops every voice (used when the scene is stopped or rebuilt)
        static void StopAll(Scene::Scene& scene, Audio::AudioSystem& audio);
    };
}
