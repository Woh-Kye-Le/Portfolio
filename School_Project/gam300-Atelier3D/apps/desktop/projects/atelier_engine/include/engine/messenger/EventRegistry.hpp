/* !
@file    	    EventRegistry.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		03/11/2026
@brief
    Declares the event registry: every event type of Events.hpp by name, with
    who sends it, its fields and what it means, for tools that list events
    (logs, the debugger) and EventName<E>() for an event type's name.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/messenger/Events.hpp"

#include <span>
#include <string_view>

namespace Engine::Events
{
    // What the registry says about one event type
    struct EventInfo
    {
        std::string_view name;   // the struct's name, e.g. "CollisionBegan"
        std::string_view sender; // who sends it
        std::string_view fields; // its fields, as a short list
        std::string_view help;   // what it means, in a sentence
    };

    // Every event type, in the order of Events.hpp
    std::span<const EventInfo> AllEvents();
    // The event called name (exact), or nullptr
    const EventInfo* FindEvent(std::string_view name);

    // The name of an event type, e.g. EventName<CollisionBegan>() == "CollisionBegan". Each event type of Events.hpp
    // has one; a type without one does not compile.
    template <class E> constexpr std::string_view EventName() = delete;
    template <> constexpr std::string_view EventName<WindowResized>()
    {
        return "WindowResized";
    }
    template <> constexpr std::string_view EventName<GameStarted>()
    {
        return "GameStarted";
    }
    template <> constexpr std::string_view EventName<GameStopped>()
    {
        return "GameStopped";
    }
    template <> constexpr std::string_view EventName<SimulationChanged>()
    {
        return "SimulationChanged";
    }
    template <> constexpr std::string_view EventName<GameUpdate>()
    {
        return "GameUpdate";
    }
    template <> constexpr std::string_view EventName<ActionPressed>()
    {
        return "ActionPressed";
    }
    template <> constexpr std::string_view EventName<ActionReleased>()
    {
        return "ActionReleased";
    }
    template <> constexpr std::string_view EventName<KeyPressed>()
    {
        return "KeyPressed";
    }
    template <> constexpr std::string_view EventName<CollisionBegan>()
    {
        return "CollisionBegan";
    }
    template <> constexpr std::string_view EventName<CollisionEnded>()
    {
        return "CollisionEnded";
    }
    template <> constexpr std::string_view EventName<MessageBroadcast>()
    {
        return "MessageBroadcast";
    }
    template <> constexpr std::string_view EventName<TimerStarted>()
    {
        return "TimerStarted";
    }
    template <> constexpr std::string_view EventName<TimerElapsed>()
    {
        return "TimerElapsed";
    }
}
