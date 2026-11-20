/* !
@file    	    AudioSourceComponent.hpp
@author  	    sung.h@digipen.edu [70%]
         	    c.kojat@digipen.edu [26%]
         	    k.thantwin@digipen.edu [4%]
@date    		01/10/2026
@brief
    AudioSourceComponent: an entity's sound clip, volume, range, loop/3D
    settings and playback state.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/audio/AudioSystem.hpp"

#include <cstdint>

namespace Engine::Scene
{
    // Runtime version of an object's Audio Source. The scene file stores the clip as a path; GameRuntime turns
    // that into an index into its clip table when it builds the entity, and Systems::AudioSystem does the playing.
    struct AudioSourceComponent
    {

        // One-based index into the runtime's audio clip table.
        // Zero means no clip.
        std::uint32_t clip = 0;

        float volume = 1.0f;
        float minDistance = 2.0f;
        float maxDistance = 50.0f;

        bool loop = true;
        bool spatial = true;
        bool playOnStart = true;

        // Which group's volume controls it (sfx or music). Set from the scene object when the entity is built.
        Audio::Bus bus = Audio::Bus::Sfx;

        // Playback state managed by Systems::AudioSystem.
        Audio::SoundId sound;
        Audio::VoiceId voice;

        bool loadAttempted = false; // so a missing clip is only tried (and logged) once, not every frame
        bool started = false;       // a non-looping sound plays once, this stops it restarting when it ends
    };
}
