/* !
@file    	    AudioSystem.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the engine's audio API: the SoundId and VoiceId handles, the SFX
    and music buses, the data-driven sound bank, and AudioSystem, which wraps
    FMOD Core for loading clips, playback, bus mixing and 3D positional audio.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Audio
{
    // A loaded clip. Default-constructed is invalid.
    struct SoundId
    {
        std::uint32_t value = 0;

        bool IsValid() const noexcept { return value != 0; }
        friend bool operator==(SoundId a, SoundId b) noexcept { return a.value == b.value; }
    };

    // One playing instance. These go stale on their own when the sound ends,
    // which is normal - calls against a dead voice quietly do nothing, so you
    // never have to check before using one.
    struct VoiceId
    {
        std::uint16_t slot = 0;
        std::uint16_t generation = 0;

        bool IsValid() const noexcept { return generation != 0; }
        friend bool operator==(VoiceId a, VoiceId b) noexcept
        {
            return a.slot == b.slot && a.generation == b.generation;
        }
    };

    // Mixer buses. Every voice goes through one of these, and each hangs off
    // the master group, so a bus can be faded or muted without touching the
    // voices playing on it.
    enum class Bus
    {
        Sfx,
        Music,
    };

    inline constexpr int BusCount = 2;

    /*!
    \brief   Parses the "bus" field from the sound bank config.
    \param   name The bus name as written in the config ("sfx" or "music").
    \return  The matching bus; anything unrecognised is Sfx.
    */
    Bus BusFromString(std::string_view name) noexcept;

    /*!
    \brief   Gets the display name of a bus.
    \param   bus The bus to name.
    \return  "SFX" or "Music".
    */
    const char* BusName(Bus bus) noexcept;

    // How a clip is created. Spatial clips must be MONO - FMOD will not pan a
    // stereo source, it just plays it in both ears.
    struct SoundLoadOptions
    {
        bool stream = false;  // decode while playing: music, not one-shots
        bool spatial = false; // 3D positioned rather than straight to the bus
        float minDistance = 1.0f;  // full volume within this radius
        float maxDistance = 30.0f; // silent from here (linear fade from minDistance)
    };

    // One entry from the sound bank config. Everything about a sound - which
    // file, whether it loops, how loud, which bus - lives in data, not code.
    struct SoundDef
    {
        std::string name;    // logical name the rest of the engine refers to
        std::string file;    // path as written in the config, relative to it
        std::string bus = "sfx"; // as authored, kept for display
        bool stream = false; // decode while playing - right for music, wrong for one-shots
        bool loop = false;
        float volume = 1.0f;
        bool spatial = false;      // positioned in the world; mono files only
        float minDistance = 1.0f;
        float maxDistance = 30.0f;

        Bus busId = Bus::Sfx; // resolved from `bus` on load
        SoundId sound{};     // resolved on load; invalid if the file was missing
    };

    // Owns the FMOD system object, the loaded clips and the playing voices.
    //
    // Pimpl'd on purpose: the vendored SDK only ships MSVC import libs and no
    // Linux build, so non-MSVC toolchains compile a silent stub instead and
    // nothing outside AudioSystem.cpp needs to know which one it got.
    class AudioSystem
    {
    public:
        static constexpr int DefaultMaxChannels = 512;

        // Voice budget. Blocks the obvious beginner mistake - play a sound in a
        // forever loop with no wait - from exhausting FMOD or our own memory.
        static constexpr int MaxVoices = 64;

        AudioSystem();
        ~AudioSystem();

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        /*!
        \brief   Starts FMOD and sets up the master, SFX and music groups. Does nothing if audio is already
                 running.
        \param   maxChannels The most channels FMOD may mix at once.
        \return  True if audio is available. False means no audio, which is not fatal: the engine runs silently.
        */
        bool Init(int maxChannels = DefaultMaxChannels);

        /*!
        \brief   Runs FMOD's mixer for this frame and recycles the voices that have finished. Call once every
                 frame.
        */
        void Update();

        /*!
        \brief   Releases everything the audio system holds and closes FMOD. Safe to call twice.
        */
        void Shutdown();

        /*!
        \brief   Reports whether audio started successfully.
        \return  True if sounds can be loaded and played.
        */
        bool IsAvailable() const noexcept { return available_; }

        /*!
        \brief   Names the audio backend this build was compiled with.
        \return  "FMOD", or "null" for the silent build used where FMOD is not available.
        */
        static const char* BackendName() noexcept;

        // Clips ---------------------------------------------------------

        /*!
        \brief   Loads a clip from disk. Asking for the same file the same way again returns the same clip.
        \param   path    Path to the sound file.
        \param   options How the clip is created: streamed or held in memory, 2D or 3D, and its 3D distances.
                         Streaming suits music; a clip that is fired repeatedly should stay in memory.
        \return  The clip's id, or an invalid id if the file is missing or unreadable (logged).
        */
        SoundId LoadSound(std::string_view path, SoundLoadOptions options = {});

        /*!
        \brief   Releases one clip and forgets its path, so the next LoadSound reads the file again.
        \param   sound The clip to release. An invalid or already released id is ignored.
        */
        void UnloadSound(SoundId sound);

        /*!
        \brief   Stops every voice and releases every loaded clip.
        */
        void UnloadAllSounds();

        // Playback ------------------------------------------------------

        /*!
        \brief   Starts a new voice playing a clip.
        \param   sound The clip to play.
        \param   loop  True to repeat until stopped.
        \param   bus   The mixer bus the voice plays on.
        \return  The new voice, or an invalid id if the clip is invalid or all MaxVoices are busy.
        */
        VoiceId Play(SoundId sound, bool loop = false, Bus bus = Bus::Sfx);

        /*!
        \brief   Stops a voice and frees its slot.
        \param   voice The voice. A voice that has already ended, or an invalid id, is ignored.
        */
        void Stop(VoiceId voice);

        /*!
        \brief   Stops every playing voice.
        */
        void StopAll();

        /*!
        \brief   Pauses or resumes a voice without losing its place in the clip.
        \param   voice  The voice. A voice that has already ended, or an invalid id, is ignored.
        \param   paused True to pause, false to resume.
        */
        void SetPaused(VoiceId voice, bool paused);

        /*!
        \brief   Checks whether a voice is still alive.
        \param   voice The voice to check.
        \return  True while the voice exists; false once it has ended or been stopped, or for an invalid id.
        */
        bool IsPlaying(VoiceId voice) const;

        /*!
        \brief   Gets how far through its clip a voice is. A looping voice wraps back to 0.
        \param   voice The voice to check.
        \return  0 to 1; 0 for a voice that is not playing.
        */
        float GetVoiceProgress(VoiceId voice) const;

        /*!
        \brief   Gets how loud a voice actually is right now: its own volume after the 3D distance fade, as FMOD
                 computes it (Channel::getAudibility).
        \param   voice The voice to check.
        \return  0 to 1; 0 for a voice that is not playing, and always 0 without FMOD.
        */
        float GetVoiceAudibility(VoiceId voice) const;

        /*!
        \brief   Counts the voices currently alive. For debug readouts.
        \return  The number of live voices, at most MaxVoices.
        */
        int ActiveVoiceCount() const;

        /*!
        \brief   Sets one voice's volume.
        \param   voice  The voice. A voice that has already ended, or an invalid id, is ignored.
        \param   volume 0 is silent, 1 is the clip as authored. Above 1 amplifies and may clip; negative values
                        are clamped to 0.
        */
        void SetVoiceVolume(VoiceId voice, float volume);

        /*!
        \brief   Sets the volume applied to everything that plays.
        \param   volume 0 is silent, 1 is full volume. Negative values are clamped to 0.
        */
        void SetMasterVolume(float volume);

        /*!
        \brief   Gets the master volume.
        \return  The value last set with SetMasterVolume.
        */
        float GetMasterVolume() const noexcept { return masterVolume_; }

        // Buses ---------------------------------------------------------
        //
        // Bus volume multiplies with the voice's own volume and the master, so
        // muting Music leaves every SFX voice untouched and still playing.

        /*!
        \brief   Sets the volume of one bus. It multiplies with each voice's own volume and the master volume.
        \param   bus    The bus to change.
        \param   volume 0 is silent, 1 is full volume. Negative values are clamped to 0.
        */
        void SetBusVolume(Bus bus, float volume);

        /*!
        \brief   Gets the volume of one bus.
        \param   bus The bus to read.
        \return  The value last set with SetBusVolume.
        */
        float GetBusVolume(Bus bus) const noexcept;

        /*!
        \brief   Mutes or unmutes one bus. Its voices keep playing silently, and the other bus is not affected.
        \param   bus   The bus to change.
        \param   muted True to mute, false to unmute.
        */
        void SetBusMuted(Bus bus, bool muted);

        /*!
        \brief   Checks whether a bus is muted.
        \param   bus The bus to read.
        \return  True if the bus is muted.
        */
        bool IsBusMuted(Bus bus) const noexcept;

        /*!
        \brief   Counts the live voices on one bus. For debug readouts.
        \param   bus The bus to count.
        \return  The number of live voices playing on that bus.
        */
        int BusVoiceCount(Bus bus) const;

        // Sound bank ----------------------------------------------------

        /*!
        \brief   Reads the sound bank from a JSON config and loads every clip listed in it. A clip whose file is
                 missing is logged and left with an invalid SoundId rather than failing the whole bank.
        \param   configPath Path to the config file (sounds.json).
        \return  How many entries were read.
        */
        int LoadSoundBank(const std::string& configPath);

        /*!
        \brief   Re-reads the config last given to LoadSoundBank. Every loaded clip is dropped first, so a file
                 that was replaced on disk is picked up without rebuilding.
        \return  How many entries were read; 0 if no bank has been loaded yet.
        */
        int ReloadSoundBank();

        /*!
        \brief   Gets the entries of the loaded sound bank.
        \return  One SoundDef per entry in the config.
        */
        const std::vector<SoundDef>& SoundBank() const noexcept { return bank_; }

        /*!
        \brief   Gets the path of the loaded sound bank config.
        \return  The path last given to LoadSoundBank; empty if none has been loaded.
        */
        const std::string& SoundBankPath() const noexcept { return bankPath_; }

        /*!
        \brief   Looks up a sound bank entry's clip by name.
        \param   name The entry's name in the config.
        \return  The clip's id, or an invalid id if there is no such entry or its file failed to load.
        */
        SoundId FindSound(std::string_view name) const;

        /*!
        \brief   Plays a sound bank entry by name, using its loop, volume and bus settings.
        \param   name The entry's name in the config.
        \return  The new voice, or an invalid id if there is no such entry (logged) or it could not be played.
        */
        VoiceId PlayNamed(std::string_view name);

        // 3D ------------------------------------------------------------
        //
        // The listener is the active camera. Push it every frame before
        // Update(); nothing spatial is correct until you do.

        /*!
        \brief   Sets where the listener is and which way it faces. Call every frame, before Update(), with the
                 active camera.
        \param   position The listener's position in the world.
        \param   forward  The direction the listener faces.
        \param   up       The listener's up direction.
        */
        void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

        /*!
        \brief   Gets the listener's position.
        \return  The position last given to SetListener.
        */
        const glm::vec3& ListenerPosition() const noexcept { return listenerPosition_; }

        /*!
        \brief   Gets the direction the listener faces.
        \return  The forward direction last given to SetListener.
        */
        const glm::vec3& ListenerForward() const noexcept { return listenerForward_; }

        /*!
        \brief   Moves a playing voice in the world. Harmless on a 2D voice.
        \param   voice    The voice. A voice that has already ended, or an invalid id, is ignored.
        \param   position The voice's new position in the world.
        */
        void SetVoicePosition(VoiceId voice, const glm::vec3& position);

        /*!
        \brief   Sets a voice's 3D distances, overriding the ones its clip was loaded with. Two objects can
                 share a clip and still be heard at different ranges, and an editor slider takes effect without
                 reloading the sound.
        \param   voice       The voice. A voice that has already ended, or an invalid id, is ignored.
        \param   minDistance Full volume within this distance.
        \param   maxDistance Silent from this distance. Kept above minDistance.
        */
        void SetVoiceMinMaxDistance(VoiceId voice, float minDistance, float maxDistance);

        /*!
        \brief   Plays a sound bank entry by name at a position in the world.
        \param   name     The entry's name in the config.
        \param   position Where the sound comes from.
        \return  The new voice, or an invalid id if there is no such entry or it could not be played.
        */
        VoiceId PlayNamedAt(std::string_view name, const glm::vec3& position);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        bool available_ = false;
        float masterVolume_ = 1.0f;

        // Held outside Impl so they survive a backend that never starts, and
        // so Init() can apply whatever was set before audio came up.
        std::array<float, BusCount> busVolume_{1.0f, 1.0f};
        std::array<bool, BusCount> busMuted_{false, false};

        // Kept here so tools can draw the listener even with no audio backend.
        glm::vec3 listenerPosition_{0.0f};
        glm::vec3 listenerForward_{0.0f, 0.0f, -1.0f};
        glm::vec3 listenerUp_{0.0f, 1.0f, 0.0f};

        // Backend-independent: the bank is parsed even in builds without FMOD,
        // so tools can still show what the config declares.
        std::vector<SoundDef> bank_;
        std::string bankPath_;
    };
}
