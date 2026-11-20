/* !
@file    	    AudioSystem.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements AudioSystem twice behind one interface: the FMOD Core backend,
    and a silent backend for toolchains FMOD does not support. The sound bank
    parsing is shared by both.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/audio/AudioSystem.hpp"

#include "engine/core/Log.hpp"

#include "engine/serializer/JsonSerializer.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

#ifdef ATELIER_HAS_FMOD
#include <array>
#include <unordered_map>
#include <vector>

#include <fmod.hpp>
#include <fmod_errors.h>
#endif

namespace Engine::Audio
{
#ifdef ATELIER_HAS_FMOD

    struct AudioSystem::Impl
    {
        struct Voice
        {
            FMOD::Channel* channel = nullptr;
            std::uint16_t generation = 0; // 0 means the slot is free
            Bus bus = Bus::Sfx;
        };

        FMOD::System* system = nullptr;
        FMOD::ChannelGroup* master = nullptr;

        // One child group per bus, hanging off master.
        std::array<FMOD::ChannelGroup*, BusCount> buses{};

        // Index + 1 is the SoundId, so 0 stays free as the invalid id.
        // Released entries are left null and reused by the next load.
        std::vector<FMOD::Sound*> sounds;
        std::unordered_map<std::string, std::uint32_t> byPath;

        std::array<Voice, AudioSystem::MaxVoices> voices{};
        std::uint16_t nextGeneration = 1;
        bool warnedVoicesFull = false;
    };

    namespace
    {
        // Returns false on failure so callers can bail out.
        bool Check(FMOD_RESULT result, const char* what)
        {
            if (result == FMOD_OK)
            {
                return true;
            }

            LOG_ERROR(Audio, "{} failed - {}", what, FMOD_ErrorString(result));
            return false;
        }

        // FMOD invalidates a Channel* the moment its sound ends or the voice is
        // stolen. Both are routine, so neither is worth logging.
        bool VoiceGone(FMOD_RESULT result)
        {
            return result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN;
        }

        FMOD_VECTOR ToFmod(const glm::vec3& v)
        {
            return FMOD_VECTOR{v.x, v.y, v.z};
        }

        // FMOD packs its version as 0xaaaabbcc -> aaaa.bb.cc
        std::string VersionString(unsigned int version)
        {
            const unsigned int product = (version >> 16) & 0xFFFFu;
            const unsigned int major = (version >> 8) & 0xFFu;
            const unsigned int minor = version & 0xFFu;

            std::string text = std::to_string(product) + ".";
            if (major < 10)
            {
                text += "0";
            }
            text += std::to_string(major) + ".";
            if (minor < 10)
            {
                text += "0";
            }
            text += std::to_string(minor);
            return text;
        }
    }

    AudioSystem::AudioSystem()
        : impl_(std::make_unique<Impl>())
    {
    }

    AudioSystem::~AudioSystem()
    {
        Shutdown();
    }

    /*!
    \brief   Starts FMOD and sets up the master, SFX and music groups. Does nothing if audio is already running.
    \param   maxChannels The most channels FMOD may mix at once.
    \return  True if audio is available. False means no audio, which is not fatal: the engine runs silently.
    */
    bool AudioSystem::Init(int maxChannels)
    {
        if (available_)
        {
            return true;
        }

        if (!Check(FMOD::System_Create(&impl_->system), "System_Create"))
        {
            return false;
        }

        // An old DLL against newer headers crashes later, somewhere worse.
        unsigned int version = 0;
        if (!Check(impl_->system->getVersion(&version), "getVersion"))
        {
            Shutdown();
            return false;
        }

        if (version < FMOD_VERSION)
        {
            LOG_ERROR(Audio,
                      "fmod.dll is version {} but the headers are {} - copy the matching DLL next to the executable",
                      VersionString(version), VersionString(FMOD_VERSION));
            Shutdown();
            return false;
        }

        // FMOD is left-handed by default; this engine is right-handed (yaw 0
        // looks down -Z, right is +X). Without this flag left and right come
        // out mirrored, which sounds plausible enough to miss.
        const FMOD_INITFLAGS initFlags = FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED;

        FMOD_RESULT result = impl_->system->init(maxChannels, initFlags, nullptr);

        // No sound device: CI box, lab PC with audio disabled. Drop to silent
        // output instead of taking the whole app down.
        if (result == FMOD_ERR_OUTPUT_INIT || result == FMOD_ERR_OUTPUT_DRIVERCALL)
        {
            LOG_WARN(Audio, "No usable sound device - continuing with silent output");

            if (!Check(impl_->system->setOutput(FMOD_OUTPUTTYPE_NOSOUND), "setOutput(NOSOUND)"))
            {
                Shutdown();
                return false;
            }

            result = impl_->system->init(maxChannels, initFlags, nullptr);
        }

        if (!Check(result, "System::init"))
        {
            Shutdown();
            return false;
        }

        // Doppler off: it sounds awful on beginner-made movement and nothing
        // asks for it. 1 world unit = 1 metre, default rolloff.
        impl_->system->set3DSettings(0.0f, 1.0f, 1.0f);

        // Everything plays through this group, so master volume is one call and
        // ATE3D-48 can slot Music/SFX groups underneath without touching Play().
        if (!Check(impl_->system->getMasterChannelGroup(&impl_->master), "getMasterChannelGroup"))
        {
            Shutdown();
            return false;
        }

        impl_->master->setVolume(masterVolume_);

        // Child groups under master. Routing a voice here instead of straight
        // to master is what lets a whole bus be faded or muted at once.
        for (int i = 0; i < BusCount; ++i)
        {
            const Bus bus = static_cast<Bus>(i);
            if (!Check(impl_->system->createChannelGroup(BusName(bus), &impl_->buses[i]),
                    "createChannelGroup"))
            {
                Shutdown();
                return false;
            }

            if (!Check(impl_->master->addGroup(impl_->buses[i]), "ChannelGroup::addGroup"))
            {
                Shutdown();
                return false;
            }

            // Apply whatever was set before audio came up.
            impl_->buses[i]->setVolume(busVolume_[static_cast<std::size_t>(i)]);
            impl_->buses[i]->setMute(busMuted_[static_cast<std::size_t>(i)]);
        }

        int driverCount = 0;
        if (!Check(impl_->system->getNumDrivers(&driverCount), "getNumDrivers"))
        {
            driverCount = 0;
        }

        available_ = true;

        LOG_INFO(Audio, "FMOD {} initialised ({} channels, {} driver(s))", VersionString(version), maxChannels,
                 driverCount);

        return true;
    }

    /*!
    \brief   Runs FMOD's mixer for this frame and recycles the voices that have finished. Call once every frame.
    */
    void AudioSystem::Update()
    {
        if (!available_)
        {
            return;
        }

        // Reclaim slots whose sound has finished, otherwise MaxVoices would be
        // a one-way budget that runs out after 64 sounds ever.
        for (auto& voice : impl_->voices)
        {
            if (voice.generation == 0)
            {
                continue;
            }

            bool playing = false;
            const FMOD_RESULT result = voice.channel->isPlaying(&playing);
            if (VoiceGone(result) || !playing)
            {
                voice.channel = nullptr;
                voice.generation = 0;
            }
        }

        // Not through Check() - this runs every frame and would flood the log.
        const FMOD_RESULT result = impl_->system->update();
        if (result != FMOD_OK)
        {
            LOG_ERROR(Audio, "System::update failed - {} - disabling audio for the rest of this run",
                      FMOD_ErrorString(result));
            available_ = false;
        }
    }

    /*!
    \brief   Releases everything the audio system holds and closes FMOD. Safe to call twice.
    */
    void AudioSystem::Shutdown()
    {
        available_ = false;

        if (impl_ == nullptr || impl_->system == nullptr)
        {
            return;
        }

        UnloadAllSounds();

        for (FMOD::ChannelGroup*& group : impl_->buses)
        {
            if (group != nullptr)
            {
                group->release();
                group = nullptr;
            }
        }

        Check(impl_->system->close(), "System::close");
        Check(impl_->system->release(), "System::release");
        impl_->system = nullptr;
        impl_->master = nullptr;

        LOG_INFO(Audio, "FMOD shut down");
    }

    /*!
    \brief   Loads a clip from disk. Asking for the same file the same way again returns the same clip.
    \param   path    Path to the sound file.
    \param   options How the clip is created: streamed or held in memory, 2D or 3D, and its 3D distances.
                     Streaming suits music; a clip that is fired repeatedly should stay in memory.
    \return  The clip's id, or an invalid id if the file is missing or unreadable (logged).
    */
    SoundId AudioSystem::LoadSound(std::string_view path, SoundLoadOptions options)
    {
        if (!available_)
        {
            return {};
        }

        const std::string file(path);

        FMOD_MODE mode = options.stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        // Linear rolloff: full volume up to min, fading evenly to silent at max. FMOD's inverse rolloff is closer to
        // real sound but never reaches zero - past max it just holds at min/max, which is still clearly audible and
        // makes "Max Distance" read as broken in the editor.
        mode |= options.spatial ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D;

        // Keyed on the mode as well as the path. Streaming and 3D are fixed when the sound is created, so the same
        // file wanted flat in one place and positioned in another is two sounds, not one. Keying on the path alone
        // silently handed back whichever was asked for first.
        const std::string key = file + '|' + std::to_string(mode);

        const auto cached = impl_->byPath.find(key);
        if (cached != impl_->byPath.end())
        {
            return SoundId{cached->second};
        }

        FMOD::Sound* sound = nullptr;

        const FMOD_RESULT result = impl_->system->createSound(file.c_str(), mode, nullptr, &sound);

        if (result != FMOD_OK)
        {
            LOG_ERROR(Audio, "Could not load \"{}\" - {}", file, FMOD_ErrorString(result));
            return {};
        }

        if (options.spatial)
        {
            // Full volume inside min, silent from max. Get these wrong and 3D
            // audio appears to do nothing at all.
            Check(sound->set3DMinMaxDistance(options.minDistance, options.maxDistance),
                "Sound::set3DMinMaxDistance");

            int channels = 0;
            sound->getFormat(nullptr, nullptr, &channels, nullptr);
            if (channels > 1)
            {
                LOG_WARN(Audio, "\"{}\" is not mono, so it will not be spatialised", file);
            }
        }

        // Reuse a hole left by UnloadSound before growing the table.
        std::uint32_t index = static_cast<std::uint32_t>(impl_->sounds.size());
        for (std::uint32_t i = 0; i < impl_->sounds.size(); ++i)
        {
            if (impl_->sounds[i] == nullptr)
            {
                index = i;
                break;
            }
        }

        if (index == impl_->sounds.size())
        {
            impl_->sounds.push_back(sound);
        }
        else
        {
            impl_->sounds[index] = sound;
        }

        const SoundId id{index + 1};
        impl_->byPath[key] = id.value;
        return id;
    }

    /*!
    \brief   Releases one clip and forgets its path, so the next LoadSound reads the file again.
    \param   sound The clip to release. An invalid or already released id is ignored.
    */
    void AudioSystem::UnloadSound(SoundId sound)
    {
        if (!available_ || !sound.IsValid() || sound.value > impl_->sounds.size())
        {
            return;
        }

        FMOD::Sound*& slot = impl_->sounds[sound.value - 1];
        if (slot == nullptr)
        {
            return;
        }

        Check(slot->release(), "Sound::release");
        slot = nullptr;

        for (auto it = impl_->byPath.begin(); it != impl_->byPath.end(); ++it)
        {
            if (it->second == sound.value)
            {
                impl_->byPath.erase(it);
                break;
            }
        }
    }

    /*!
    \brief   Stops every voice and releases every loaded clip.
    */
    void AudioSystem::UnloadAllSounds()
    {
        if (impl_ == nullptr)
        {
            return;
        }

        StopAll();

        for (FMOD::Sound*& sound : impl_->sounds)
        {
            if (sound != nullptr)
            {
                Check(sound->release(), "Sound::release");
                sound = nullptr;
            }
        }

        impl_->sounds.clear();
        impl_->byPath.clear();
    }

    /*!
    \brief   Starts a new voice playing a clip.
    \param   sound The clip to play.
    \param   loop  True to repeat until stopped.
    \param   bus   The mixer bus the voice plays on.
    \return  The new voice, or an invalid id if the clip is invalid or all MaxVoices are busy.
    */
    VoiceId AudioSystem::Play(SoundId sound, bool loop, Bus bus)
    {
        if (!available_ || !sound.IsValid() || sound.value > impl_->sounds.size())
        {
            return {};
        }

        FMOD::Sound* clip = impl_->sounds[sound.value - 1];
        if (clip == nullptr)
        {
            return {};
        }

        int slot = -1;
        for (int i = 0; i < MaxVoices; ++i)
        {
            if (impl_->voices[static_cast<std::size_t>(i)].generation == 0)
            {
                slot = i;
                break;
            }
        }

        if (slot < 0)
        {
            // Once, not every frame: whatever caused this is still going.
            if (!impl_->warnedVoicesFull)
            {
                impl_->warnedVoicesFull = true;
                LOG_WARN(Audio,
                         "All {} voices busy, dropping this one - a sound is probably playing in a loop with no wait",
                         MaxVoices);
            }
            return {};
        }

        // Start paused so loop and volume are set before anything is audible,
        // otherwise a looping clip can get one unlooped frame out first.
        FMOD::Channel* channel = nullptr;
        FMOD::ChannelGroup* group = impl_->buses[static_cast<std::size_t>(bus)];
        if (group == nullptr)
        {
            group = impl_->master;
        }

        const FMOD_RESULT result = impl_->system->playSound(clip, group, true, &channel);
        if (!Check(result, "System::playSound") || channel == nullptr)
        {
            return {};
        }

        channel->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        if (loop)
        {
            channel->setLoopCount(-1);
        }
        channel->setVolume(1.0f);

        // A 3D channel with no position defaults to the origin, which is fine
        // as a starting point - callers move it straight after.
        FMOD_MODE mode = 0;
        if (clip->getMode(&mode) == FMOD_OK && (mode & FMOD_3D) != 0)
        {
            FMOD_VECTOR position = ToFmod(glm::vec3(0.0f));
            FMOD_VECTOR velocity{0.0f, 0.0f, 0.0f};
            channel->set3DAttributes(&position, &velocity);
        }

        channel->setPaused(false);

        Impl::Voice& voice = impl_->voices[static_cast<std::size_t>(slot)];
        voice.channel = channel;
        voice.generation = impl_->nextGeneration;
        voice.bus = bus;

        // Generation 0 means "free", so it must never be handed out.
        impl_->nextGeneration = static_cast<std::uint16_t>(impl_->nextGeneration + 1);
        if (impl_->nextGeneration == 0)
        {
            impl_->nextGeneration = 1;
        }

        impl_->warnedVoicesFull = false;

        return VoiceId{static_cast<std::uint16_t>(slot), voice.generation};
    }

    /*!
    \brief   Stops a voice and frees its slot.
    \param   voice The voice. A voice that has already ended, or an invalid id, is ignored.
    */
    void AudioSystem::Stop(VoiceId voice)
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return;
        }

        Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return;
        }

        const FMOD_RESULT result = slot.channel->stop();
        if (result != FMOD_OK && !VoiceGone(result))
        {
            Check(result, "Channel::stop");
        }

        slot.channel = nullptr;
        slot.generation = 0;
    }

    /*!
    \brief   Stops every playing voice.
    */
    void AudioSystem::StopAll()
    {
        if (impl_ == nullptr)
        {
            return;
        }

        for (auto& voice : impl_->voices)
        {
            if (voice.generation != 0 && voice.channel != nullptr)
            {
                voice.channel->stop();
            }
            voice.channel = nullptr;
            voice.generation = 0;
        }
    }

    /*!
    \brief   Pauses or resumes a voice without losing its place in the clip.
    \param   voice  The voice. A voice that has already ended, or an invalid id, is ignored.
    \param   paused True to pause, false to resume.
    */
    void AudioSystem::SetPaused(VoiceId voice, bool paused)
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return;
        }

        slot.channel->setPaused(paused);
    }

    /*!
    \brief   Gets how loud a voice actually is right now: its own volume after the 3D distance fade, as FMOD
             computes it (Channel::getAudibility).
    \param   voice The voice to check.
    \return  0 to 1; 0 for a voice that is not playing, and always 0 without FMOD.
    */
    float AudioSystem::GetVoiceAudibility(VoiceId voice) const
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return 0.0f;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return 0.0f;
        }

        float audibility = 0.0f;
        if (slot.channel->getAudibility(&audibility) != FMOD_OK)
        {
            return 0.0f;
        }
        return std::clamp(audibility, 0.0f, 1.0f);
    }

    /*!
    \brief   Checks whether a voice is still alive.
    \param   voice The voice to check.
    \return  True while the voice exists; false once it has ended or been stopped, or for an invalid id.
    */
    bool AudioSystem::IsPlaying(VoiceId voice) const
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return false;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return false;
        }

        bool playing = false;
        if (slot.channel->isPlaying(&playing) != FMOD_OK)
        {
            return false;
        }

        return playing;
    }

    /*!
    \brief   Gets how far through its clip a voice is. A looping voice wraps back to 0.
    \param   voice The voice to check.
    \return  0 to 1; 0 for a voice that is not playing.
    */
    float AudioSystem::GetVoiceProgress(VoiceId voice) const
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return 0.0f;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return 0.0f;
        }

        unsigned int position = 0;
        if (slot.channel->getPosition(&position, FMOD_TIMEUNIT_MS) != FMOD_OK)
        {
            return 0.0f;
        }

        // Length comes from the sound, not the channel, so ask the channel what
        // it is currently playing rather than tracking it ourselves.
        FMOD::Sound* clip = nullptr;
        unsigned int length = 0;
        if (slot.channel->getCurrentSound(&clip) != FMOD_OK || clip == nullptr ||
            clip->getLength(&length, FMOD_TIMEUNIT_MS) != FMOD_OK || length == 0)
        {
            return 0.0f;
        }

        return std::min(1.0f, static_cast<float>(position) / static_cast<float>(length));
    }

    /*!
    \brief   Counts the voices currently alive. For debug readouts.
    \return  The number of live voices, at most MaxVoices.
    */
    int AudioSystem::ActiveVoiceCount() const
    {
        if (!available_)
        {
            return 0;
        }

        int count = 0;
        for (const auto& voice : impl_->voices)
        {
            if (voice.generation != 0)
            {
                ++count;
            }
        }

        return count;
    }

    /*!
    \brief   Sets one voice's volume.
    \param   voice  The voice. A voice that has already ended, or an invalid id, is ignored.
    \param   volume 0 is silent, 1 is the clip as authored. Above 1 amplifies and may clip; negative values are
                    clamped to 0.
    */
    void AudioSystem::SetVoiceVolume(VoiceId voice, float volume)
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return;
        }

        slot.channel->setVolume(std::max(0.0f, volume));
    }

    /*!
    \brief   Sets the volume applied to everything that plays.
    \param   volume 0 is silent, 1 is full volume. Negative values are clamped to 0.
    */
    void AudioSystem::SetMasterVolume(float volume)
    {
        masterVolume_ = std::max(0.0f, volume);

        if (available_ && impl_->master != nullptr)
        {
            impl_->master->setVolume(masterVolume_);
        }
    }

    /*!
    \brief   Sets the volume of one bus. It multiplies with each voice's own volume and the master volume.
    \param   bus    The bus to change.
    \param   volume 0 is silent, 1 is full volume. Negative values are clamped to 0.
    */
    void AudioSystem::SetBusVolume(Bus bus, float volume)
    {
        const std::size_t i = static_cast<std::size_t>(bus);
        busVolume_[i] = std::max(0.0f, volume);

        if (available_ && impl_->buses[i] != nullptr)
        {
            impl_->buses[i]->setVolume(busVolume_[i]);
        }
    }

    /*!
    \brief   Mutes or unmutes one bus. Its voices keep playing silently, and the other bus is not affected.
    \param   bus   The bus to change.
    \param   muted True to mute, false to unmute.
    */
    void AudioSystem::SetBusMuted(Bus bus, bool muted)
    {
        const std::size_t i = static_cast<std::size_t>(bus);
        busMuted_[i] = muted;

        if (available_ && impl_->buses[i] != nullptr)
        {
            impl_->buses[i]->setMute(muted);
        }
    }

    /*!
    \brief   Sets where the listener is and which way it faces. Call every frame, before Update(), with the
             active camera.
    \param   position The listener's position in the world.
    \param   forward  The direction the listener faces.
    \param   up       The listener's up direction.
    */
    void AudioSystem::SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
    {
        listenerPosition_ = position;
        listenerForward_ = forward;
        listenerUp_ = up;

        if (!available_)
        {
            return;
        }

        FMOD_VECTOR pos = ToFmod(position);
        FMOD_VECTOR vel{0.0f, 0.0f, 0.0f}; // no doppler, so velocity stays zero
        FMOD_VECTOR fwd = ToFmod(glm::normalize(forward));
        FMOD_VECTOR upv = ToFmod(glm::normalize(up));

        impl_->system->set3DListenerAttributes(0, &pos, &vel, &fwd, &upv);
    }

    /*!
    \brief   Moves a playing voice in the world. Harmless on a 2D voice.
    \param   voice    The voice. A voice that has already ended, or an invalid id, is ignored.
    \param   position The voice's new position in the world.
    */
    void AudioSystem::SetVoicePosition(VoiceId voice, const glm::vec3& position)
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return;
        }

        FMOD_VECTOR pos = ToFmod(position);
        FMOD_VECTOR vel{0.0f, 0.0f, 0.0f};
        slot.channel->set3DAttributes(&pos, &vel);
    }

    /*!
    \brief   Sets a voice's 3D distances, overriding the ones its clip was loaded with. Two objects can share a
             clip and still be heard at different ranges, and an editor slider takes effect without reloading
             the sound.
    \param   voice       The voice. A voice that has already ended, or an invalid id, is ignored.
    \param   minDistance Full volume within this distance.
    \param   maxDistance Silent from this distance. Kept above minDistance.
    */
    void AudioSystem::SetVoiceMinMaxDistance(VoiceId voice, float minDistance, float maxDistance)
    {
        if (!available_ || !voice.IsValid() || voice.slot >= MaxVoices)
        {
            return;
        }

        const Impl::Voice& slot = impl_->voices[voice.slot];
        if (slot.generation != voice.generation || slot.channel == nullptr)
        {
            return;
        }

        // FMOD stops attenuating when max is at or below min, which sounds exactly like broken 3D audio, so keep
        // them apart whatever the caller passed.
        const float min = std::max(0.01f, minDistance);
        slot.channel->set3DMinMaxDistance(min, std::max(min + 0.01f, maxDistance));
    }

    /*!
    \brief   Counts the live voices on one bus. For debug readouts.
    \param   bus The bus to count.
    \return  The number of live voices playing on that bus.
    */
    int AudioSystem::BusVoiceCount(Bus bus) const
    {
        if (!available_)
        {
            return 0;
        }

        int count = 0;
        for (const auto& voice : impl_->voices)
        {
            if (voice.generation != 0 && voice.bus == bus)
            {
                ++count;
            }
        }

        return count;
    }

#else // no FMOD on this toolchain

    struct AudioSystem::Impl
    {
    };

    AudioSystem::AudioSystem()
        : impl_(std::make_unique<Impl>())
    {
    }

    AudioSystem::~AudioSystem()
    {
        Shutdown();
    }

    bool AudioSystem::Init(int)
    {
        LOG_WARN(Audio, "Built without FMOD on this toolchain - running silently");
        available_ = false;
        return false;
    }

    void AudioSystem::Update()
    {
    }

    void AudioSystem::Shutdown()
    {
        available_ = false;
    }

    SoundId AudioSystem::LoadSound(std::string_view, SoundLoadOptions)
    {
        return {};
    }

    void AudioSystem::SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
    {
        listenerPosition_ = position;
        listenerForward_ = forward;
        listenerUp_ = up;
    }

    void AudioSystem::SetVoicePosition(VoiceId, const glm::vec3&)
    {
    }

    void AudioSystem::SetVoiceMinMaxDistance(VoiceId, float, float)
    {
    }

    void AudioSystem::UnloadSound(SoundId)
    {
    }

    void AudioSystem::UnloadAllSounds()
    {
    }

    VoiceId AudioSystem::Play(SoundId, bool, Bus)
    {
        return {};
    }

    void AudioSystem::Stop(VoiceId)
    {
    }

    void AudioSystem::StopAll()
    {
    }

    void AudioSystem::SetPaused(VoiceId, bool)
    {
    }

    float AudioSystem::GetVoiceAudibility(VoiceId) const
    {
        return 0.0f;
    }

    bool AudioSystem::IsPlaying(VoiceId) const
    {
        return false;
    }

    float AudioSystem::GetVoiceProgress(VoiceId) const
    {
        return 0.0f;
    }

    int AudioSystem::ActiveVoiceCount() const
    {
        return 0;
    }

    void AudioSystem::SetVoiceVolume(VoiceId, float)
    {
    }

    void AudioSystem::SetMasterVolume(float volume)
    {
        masterVolume_ = std::max(0.0f, volume);
    }

    void AudioSystem::SetBusVolume(Bus bus, float volume)
    {
        busVolume_[static_cast<std::size_t>(bus)] = std::max(0.0f, volume);
    }

    void AudioSystem::SetBusMuted(Bus bus, bool muted)
    {
        busMuted_[static_cast<std::size_t>(bus)] = muted;
    }

    int AudioSystem::BusVoiceCount(Bus) const
    {
        return 0;
    }

#endif


    // Sound bank -------------------------------------------------------
    //
    // Parsed the same way with or without FMOD: a build with no audio still
    // knows what the config declares, it just cannot load the clips.

    /*!
    \brief   Reads the sound bank from a JSON config and loads every clip listed in it. A clip whose file is
             missing is logged and left with an invalid SoundId rather than failing the whole bank.
    \param   configPath Path to the config file (sounds.json).
    \return  How many entries were read.
    */
    int AudioSystem::LoadSoundBank(const std::string& configPath)
    {
        bankPath_ = configPath;
        bank_.clear();

        if (!std::filesystem::exists(configPath))
        {
            LOG_ERROR(Audio, "No sound bank at \"{}\"", configPath);
            return 0;
        }

        // JSONSerializer::LoadFile throws on a missing or malformed file - it
        // does not set getError() - and EnterArrayElement throws too. A typo in
        // the config while the app is running must not take it down.
        Serializer::JSONSerializer json;
        try
        {
            json.LoadFile(configPath);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Audio, "Could not read \"{}\" - {} (bank left empty; fix the file and reload)", configPath,
                      e.what());
            return 0;
        }

        if (!json.HasKey("sounds"))
        {
            LOG_ERROR(Audio, "\"{}\" has no \"sounds\" array", configPath);
            return 0;
        }

        json.EnterArray("sounds");
        if (!json.GetCurrentNode().is_array())
        {
            LOG_ERROR(Audio, "\"sounds\" in \"{}\" is not an array", configPath);
            return 0;
        }

        const std::size_t count = json.GetCurrentNode().size();

        for (std::size_t i = 0; i < count; ++i)
        {
            try
            {
                json.EnterArrayElement(i);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(Audio, "Entry {} unreadable - {}", i, e.what());
                break;
            }

            SoundDef def;
            if (json.HasKey("name"))
            {
                json.ReadString("name", def.name);
            }
            if (json.HasKey("file"))
            {
                json.ReadString("file", def.file);
            }
            if (json.HasKey("bus"))
            {
                json.ReadString("bus", def.bus);
            }
            if (json.HasKey("stream"))
            {
                json.ReadBool("stream", def.stream);
            }
            if (json.HasKey("loop"))
            {
                json.ReadBool("loop", def.loop);
            }
            if (json.HasKey("volume"))
            {
                json.ReadFloat("volume", def.volume);
            }
            if (json.HasKey("spatial"))
            {
                json.ReadBool("spatial", def.spatial);
            }
            if (json.HasKey("min_distance"))
            {
                json.ReadFloat("min_distance", def.minDistance);
            }
            if (json.HasKey("max_distance"))
            {
                json.ReadFloat("max_distance", def.maxDistance);
            }

            json.LeaveObject(); // leaves the array element

            def.busId = BusFromString(def.bus);

            if (def.name.empty() || def.file.empty())
            {
                LOG_WARN(Audio, "Entry {} has no name or file - skipped", i);
                continue;
            }

            bank_.push_back(std::move(def));
        }

        // Paths in the config are relative to the config itself, so moving the
        // whole audio folder does not break every entry.
        const std::filesystem::path root = std::filesystem::path(configPath).parent_path();

        int loaded = 0;
        for (SoundDef& def : bank_)
        {
            def.sound = LoadSound((root / def.file).string(),
                SoundLoadOptions{.stream = def.stream,
                                 .spatial = def.spatial,
                                 .minDistance = def.minDistance,
                                 .maxDistance = def.maxDistance});
            if (def.sound.IsValid())
            {
                ++loaded;
            }

            // Logged per entry: when a swap "does not take", this is what tells
            // you whether the file on disk was the one you edited.
            LOG_INFO(Audio, "  {} -> {}  [{}{}]{}", def.name, def.file, BusName(def.busId), def.spatial ? " 3D" : "",
                        def.sound.IsValid() ? "" : "  (MISSING)");
        }

        LOG_INFO(Audio, "Sound bank loaded - {} of {} entries from \"{}\"", loaded, bank_.size(), configPath);

        return static_cast<int>(bank_.size());
    }

    /*!
    \brief   Re-reads the config last given to LoadSoundBank. Every loaded clip is dropped first, so a file that
             was replaced on disk is picked up without rebuilding.
    \return  How many entries were read; 0 if no bank has been loaded yet.
    */
    int AudioSystem::ReloadSoundBank()
    {
        if (bankPath_.empty())
        {
            return 0;
        }

        // Everything goes first. LoadSound caches by path, so without this a
        // file replaced on disk would quietly keep playing the old clip.
        UnloadAllSounds();

        return LoadSoundBank(bankPath_);
    }

    /*!
    \brief   Looks up a sound bank entry's clip by name.
    \param   name The entry's name in the config.
    \return  The clip's id, or an invalid id if there is no such entry or its file failed to load.
    */
    SoundId AudioSystem::FindSound(std::string_view name) const
    {
        for (const SoundDef& def : bank_)
        {
            if (def.name == name)
            {
                return def.sound;
            }
        }

        return {};
    }

    /*!
    \brief   Plays a sound bank entry by name at a position in the world.
    \param   name     The entry's name in the config.
    \param   position Where the sound comes from.
    \return  The new voice, or an invalid id if there is no such entry or it could not be played.
    */
    VoiceId AudioSystem::PlayNamedAt(std::string_view name, const glm::vec3& position)
    {
        const VoiceId voice = PlayNamed(name);
        SetVoicePosition(voice, position);
        return voice;
    }

    /*!
    \brief   Plays a sound bank entry by name, using its loop, volume and bus settings.
    \param   name The entry's name in the config.
    \return  The new voice, or an invalid id if there is no such entry (logged) or it could not be played.
    */
    VoiceId AudioSystem::PlayNamed(std::string_view name)
    {
        for (const SoundDef& def : bank_)
        {
            if (def.name != name)
            {
                continue;
            }

            const VoiceId voice = Play(def.sound, def.loop, def.busId);
            SetVoiceVolume(voice, def.volume);
            return voice;
        }

        LOG_WARN(Audio, "No sound named \"{}\" in the bank", name);
        return {};
    }

    /*!
    \brief   Parses the "bus" field from the sound bank config.
    \param   name The bus name as written in the config ("sfx" or "music").
    \return  The matching bus; anything unrecognised is Sfx.
    */
    Bus BusFromString(std::string_view name) noexcept
    {
        return name == "music" ? Bus::Music : Bus::Sfx;
    }

    /*!
    \brief   Gets the display name of a bus.
    \param   bus The bus to name.
    \return  "SFX" or "Music".
    */
    const char* BusName(Bus bus) noexcept
    {
        return bus == Bus::Music ? "Music" : "SFX";
    }

    /*!
    \brief   Gets the volume of one bus.
    \param   bus The bus to read.
    \return  The value last set with SetBusVolume.
    */
    float AudioSystem::GetBusVolume(Bus bus) const noexcept
    {
        return busVolume_[static_cast<std::size_t>(bus)];
    }

    /*!
    \brief   Checks whether a bus is muted.
    \param   bus The bus to read.
    \return  True if the bus is muted.
    */
    bool AudioSystem::IsBusMuted(Bus bus) const noexcept
    {
        return busMuted_[static_cast<std::size_t>(bus)];
    }

    /*!
    \brief   Names the audio backend this build was compiled with.
    \return  "FMOD", or "null" for the silent build used where FMOD is not available.
    */
    const char* AudioSystem::BackendName() noexcept
    {
#ifdef ATELIER_HAS_FMOD
        return "FMOD";
#else
        return "null";
#endif
    }
}
