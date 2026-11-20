/* !
@file    	    FmodMinGWBridge.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		03/10/2026
@brief
    Lets MinGW builds use FMOD. FMOD's Windows DLL exports its C++ API with
    Microsoft-only symbol names, which MinGW cannot link, but its C API links
    fine. This file defines the FMOD C++ functions AudioSystem.cpp calls, each
    forwarding to its C counterpart (the C++ objects are the C handles).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Only MinGW builds with FMOD need this; MSVC links FMOD's own C++ functions
#if defined(ATELIER_HAS_FMOD) && defined(__MINGW32__)

#include <fmod.h>
#include <fmod.hpp>

#include <mutex>
#include <unordered_set>

// Adding an FMOD call to AudioSystem.cpp? If the MinGW build then fails with "undefined reference to
// FMOD::...", add a forward for that function here, next to the others of its class.

namespace
{
    // Channels and channel groups share the ChannelControl functions in C++, but the C API has separate
    // FMOD_Channel_* and FMOD_ChannelGroup_* functions, so each group is remembered as it is created
    std::mutex g_groupsMutex;
    std::unordered_set<const void*> g_groups;

    void RememberGroup(const FMOD::ChannelGroup* group)
    {
        if (group != nullptr)
        {
            const std::lock_guard lock(g_groupsMutex);
            g_groups.insert(group);
        }
    }

    void ForgetGroup(const FMOD::ChannelGroup* group)
    {
        const std::lock_guard lock(g_groupsMutex);
        g_groups.erase(group);
    }

    bool IsGroup(const FMOD::ChannelControl* control)
    {
        const std::lock_guard lock(g_groupsMutex);
        return g_groups.contains(control);
    }

    FMOD_SYSTEM* C(FMOD::System* system)
    {
        return reinterpret_cast<FMOD_SYSTEM*>(system);
    }
    FMOD_SOUND* C(FMOD::Sound* sound)
    {
        return reinterpret_cast<FMOD_SOUND*>(sound);
    }
    FMOD_CHANNEL* AsChannel(FMOD::ChannelControl* control)
    {
        return reinterpret_cast<FMOD_CHANNEL*>(control);
    }
    FMOD_CHANNELGROUP* AsGroup(FMOD::ChannelControl* control)
    {
        return reinterpret_cast<FMOD_CHANNELGROUP*>(control);
    }
    FMOD_CHANNELGROUP* C(FMOD::ChannelGroup* group)
    {
        return reinterpret_cast<FMOD_CHANNELGROUP*>(group);
    }

    FMOD_BOOL ToFmodBool(bool value)
    {
        return value ? 1 : 0;
    }
}

namespace FMOD
{
    // System

    FMOD_RESULT F_API System::release()
    {
        return FMOD_System_Release(C(this));
    }

    FMOD_RESULT F_API System::setOutput(FMOD_OUTPUTTYPE output)
    {
        return FMOD_System_SetOutput(C(this), output);
    }

    FMOD_RESULT F_API System::getNumDrivers(int* numdrivers)
    {
        return FMOD_System_GetNumDrivers(C(this), numdrivers);
    }

    FMOD_RESULT F_API System::init(int maxchannels, FMOD_INITFLAGS flags, void* extradriverdata)
    {
        return FMOD_System_Init(C(this), maxchannels, flags, extradriverdata);
    }

    FMOD_RESULT F_API System::close()
    {
        return FMOD_System_Close(C(this));
    }

    FMOD_RESULT F_API System::update()
    {
        return FMOD_System_Update(C(this));
    }

    FMOD_RESULT F_API System::set3DSettings(float dopplerscale, float distancefactor, float rolloffscale)
    {
        return FMOD_System_Set3DSettings(C(this), dopplerscale, distancefactor, rolloffscale);
    }

    FMOD_RESULT F_API System::set3DListenerAttributes(int listener, const FMOD_VECTOR* pos, const FMOD_VECTOR* vel,
                                                      const FMOD_VECTOR* forward, const FMOD_VECTOR* up)
    {
        return FMOD_System_Set3DListenerAttributes(C(this), listener, pos, vel, forward, up);
    }

    FMOD_RESULT F_API System::getVersion(unsigned int* version, unsigned int* buildnumber)
    {
        return FMOD_System_GetVersion(C(this), version, buildnumber);
    }

    FMOD_RESULT F_API System::createSound(const char* name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO* exinfo,
                                          Sound** sound)
    {
        return FMOD_System_CreateSound(C(this), name_or_data, mode, exinfo, reinterpret_cast<FMOD_SOUND**>(sound));
    }

    FMOD_RESULT F_API System::createChannelGroup(const char* name, ChannelGroup** channelgroup)
    {
        const FMOD_RESULT result =
            FMOD_System_CreateChannelGroup(C(this), name, reinterpret_cast<FMOD_CHANNELGROUP**>(channelgroup));
        if (result == FMOD_OK && channelgroup != nullptr)
        {
            RememberGroup(*channelgroup);
        }
        return result;
    }

    FMOD_RESULT F_API System::playSound(Sound* sound, ChannelGroup* channelgroup, bool paused, Channel** channel)
    {
        return FMOD_System_PlaySound(C(this), C(sound), C(channelgroup), ToFmodBool(paused),
                                     reinterpret_cast<FMOD_CHANNEL**>(channel));
    }

    FMOD_RESULT F_API System::getMasterChannelGroup(ChannelGroup** channelgroup)
    {
        const FMOD_RESULT result =
            FMOD_System_GetMasterChannelGroup(C(this), reinterpret_cast<FMOD_CHANNELGROUP**>(channelgroup));
        if (result == FMOD_OK && channelgroup != nullptr)
        {
            RememberGroup(*channelgroup);
        }
        return result;
    }

    // Sound

    FMOD_RESULT F_API Sound::release()
    {
        return FMOD_Sound_Release(C(this));
    }

    FMOD_RESULT F_API Sound::set3DMinMaxDistance(float min, float max)
    {
        return FMOD_Sound_Set3DMinMaxDistance(C(this), min, max);
    }

    FMOD_RESULT F_API Sound::getLength(unsigned int* length, FMOD_TIMEUNIT lengthtype)
    {
        return FMOD_Sound_GetLength(C(this), length, lengthtype);
    }

    FMOD_RESULT F_API Sound::getFormat(FMOD_SOUND_TYPE* type, FMOD_SOUND_FORMAT* format, int* channels, int* bits)
    {
        return FMOD_Sound_GetFormat(C(this), type, format, channels, bits);
    }

    FMOD_RESULT F_API Sound::getMode(FMOD_MODE* mode)
    {
        return FMOD_Sound_GetMode(C(this), mode);
    }

    // ChannelControl (shared by Channel and ChannelGroup)

    FMOD_RESULT F_API ChannelControl::stop()
    {
        return IsGroup(this) ? FMOD_ChannelGroup_Stop(AsGroup(this)) : FMOD_Channel_Stop(AsChannel(this));
    }

    FMOD_RESULT F_API ChannelControl::setPaused(bool paused)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_SetPaused(AsGroup(this), ToFmodBool(paused))
                             : FMOD_Channel_SetPaused(AsChannel(this), ToFmodBool(paused));
    }

    FMOD_RESULT F_API ChannelControl::setVolume(float volume)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_SetVolume(AsGroup(this), volume)
                             : FMOD_Channel_SetVolume(AsChannel(this), volume);
    }

    FMOD_RESULT F_API ChannelControl::setMute(bool mute)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_SetMute(AsGroup(this), ToFmodBool(mute))
                             : FMOD_Channel_SetMute(AsChannel(this), ToFmodBool(mute));
    }

    FMOD_RESULT F_API ChannelControl::getAudibility(float* audibility)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_GetAudibility(AsGroup(this), audibility)
                             : FMOD_Channel_GetAudibility(AsChannel(this), audibility);
    }

    FMOD_RESULT F_API ChannelControl::setMode(FMOD_MODE mode)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_SetMode(AsGroup(this), mode)
                             : FMOD_Channel_SetMode(AsChannel(this), mode);
    }

    FMOD_RESULT F_API ChannelControl::isPlaying(bool* isplaying)
    {
        FMOD_BOOL playing = 0;
        const FMOD_RESULT result = IsGroup(this) ? FMOD_ChannelGroup_IsPlaying(AsGroup(this), &playing)
                                                 : FMOD_Channel_IsPlaying(AsChannel(this), &playing);
        if (isplaying != nullptr)
        {
            *isplaying = playing != 0;
        }
        return result;
    }

    FMOD_RESULT F_API ChannelControl::set3DAttributes(const FMOD_VECTOR* pos, const FMOD_VECTOR* vel)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_Set3DAttributes(AsGroup(this), pos, vel)
                             : FMOD_Channel_Set3DAttributes(AsChannel(this), pos, vel);
    }

    FMOD_RESULT F_API ChannelControl::set3DMinMaxDistance(float mindistance, float maxdistance)
    {
        return IsGroup(this) ? FMOD_ChannelGroup_Set3DMinMaxDistance(AsGroup(this), mindistance, maxdistance)
                             : FMOD_Channel_Set3DMinMaxDistance(AsChannel(this), mindistance, maxdistance);
    }

    // Channel

    FMOD_RESULT F_API Channel::setLoopCount(int loopcount)
    {
        return FMOD_Channel_SetLoopCount(AsChannel(this), loopcount);
    }

    FMOD_RESULT F_API Channel::getPosition(unsigned int* position, FMOD_TIMEUNIT postype)
    {
        return FMOD_Channel_GetPosition(AsChannel(this), position, postype);
    }

    FMOD_RESULT F_API Channel::getCurrentSound(Sound** sound)
    {
        return FMOD_Channel_GetCurrentSound(AsChannel(this), reinterpret_cast<FMOD_SOUND**>(sound));
    }

    // ChannelGroup

    FMOD_RESULT F_API ChannelGroup::release()
    {
        const FMOD_RESULT result = FMOD_ChannelGroup_Release(C(this));
        if (result == FMOD_OK)
        {
            ForgetGroup(this);
        }
        return result;
    }

    FMOD_RESULT F_API ChannelGroup::addGroup(ChannelGroup* group, bool propagatedspclock, DSPConnection** connection)
    {
        return FMOD_ChannelGroup_AddGroup(C(this), C(group), ToFmodBool(propagatedspclock),
                                          reinterpret_cast<FMOD_DSPCONNECTION**>(connection));
    }
}

#endif
