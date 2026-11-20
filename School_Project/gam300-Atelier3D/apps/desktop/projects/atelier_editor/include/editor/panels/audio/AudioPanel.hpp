/* !
@file    	    AudioPanel.hpp
@author  	    k.thantwin@digipen.edu [68%]
                sanni.go@digipen.edu [20%]
         	    c.kojat@digipen.edu [12%]
@date    		29/09/2026
@brief
    Declares AudioPanel, the editor's Audio window: a debug menu that lists the
    sound bank, plays and stops its entries, shows voice counts and progress,
    mixes the buses, and tests 3D audio.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"

#include "engine/audio/AudioSystem.hpp"

#include <array>
#include <glm/glm.hpp>
#include <string>

#include <vector>

namespace Editor::Panels
{
    // Audio debug menu: what the sound bank declares, what is playing, how far
    // through it is, and how many voices are live, plus playing any sound file.
    //
    // Every row comes from sounds.json. Nothing here knows the names of any
    // sounds - add an entry to the config and a row appears.
    class AudioPanel final : public Panel
    {
    public:
        /*!
        \brief   Creates the Audio window.
        \param   audio The audio system the window shows and controls.
        */
        explicit AudioPanel(Engine::Audio::AudioSystem& audio);

        /*!
        \brief   Draws the whole Audio window: the backend and voice counts, one row per sound bank entry, the
                 mixer, and the file test and 3D emitter sections.
        */
        void DrawContents() override;

    private:
        /*!
        \brief   Loads the sound bank from sounds.json and makes one voice slot per entry.
        */
        void LoadBank();

        /*!
        \brief   Draws one sound bank entry: its name and file, how far through it is, and its Play and Stop
                 buttons.
        \param   index Index of the entry in AudioSystem::SoundBank().
        */
        void DrawRow(std::size_t index);

        /*!
        \brief   Draws the mute tickbox and volume slider for one bus.
        \param   bus The bus to draw.
        */
        void DrawBus(Engine::Audio::Bus bus);

        /*!
        \brief   Draws the file test: plays any sound file by its path on the SFX bus, outside the sound bank.
        */
        void DrawFileTest();

        /*!
        \brief   Draws the 3D emitter section: plays the sound bank's "emitter" entry and moves it around the
                 listener, either in orbit or by hand, and shows its distance and left/right balance.
        */
        void DrawSpatial();

        /*!
        \brief   Draws a top-down view of the listener and the 3D emitter.
        */
        void DrawRadar();

        Engine::Audio::AudioSystem& audio_;

        // One voice per bank entry, so a row knows what it last started.
        std::vector<Engine::Audio::VoiceId> voices_;

        float masterVolume_ = 1.0f;
        bool loaded_ = false;

        // So a reload is visibly distinct from nothing happening.
        int reloadCount_ = 0;
        int lastEntryCount_ = 0;

        // Any sound file, played outside the bank
        std::array<char, 512> filePath_{};
        float fileVolume_ = 0.8f;
        Engine::Audio::VoiceId fileVoice_;
        std::string fileError_;
        // Standalone 3D emitter, independent of the scene. Lets the listener /
        // panning work be shown without a game project loaded.
        Engine::Audio::VoiceId emitterVoice_{};
        glm::vec3 emitterPosition_{5.0f, 0.0f, 0.0f};
        bool orbit_ = true;
        float orbitRadius_ = 6.0f;
        float orbitSpeed_ = 0.4f; // turns per second
        float orbitAngle_ = 0.0f;
    };
}
