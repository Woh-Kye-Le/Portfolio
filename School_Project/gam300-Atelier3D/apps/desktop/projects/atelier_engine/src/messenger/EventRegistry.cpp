/* !
@file    	    EventRegistry.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		03/11/2026
@brief
    Implements the event registry: the table of every event type, and finding
    one by name.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/messenger/EventRegistry.hpp"

#include <algorithm>
#include <array>

namespace Engine::Events
{
    namespace
    {
        constexpr std::array kEvents{
            EventInfo{"WindowResized", "Core::Application", "width, height",
                      "The window's drawable size changed (0 x 0 while minimized)"},
            EventInfo{"GameStarted", "Game::GameRuntime", "projectName, objectCount", "A Play run started"},
            EventInfo{"GameStopped", "Game::GameRuntime", "",
                      "The Play run ended (sent before its scene is torn down)"},
            EventInfo{"SimulationChanged", "Game::GameRuntime", "simulating", "The Play run was paused or resumed"},
            EventInfo{"GameUpdate", "Game::GameRuntime", "deltaSeconds", "Once per frame while playing (not paused)"},
            EventInfo{"ActionPressed", "Game::GameRuntime", "action",
                      "A named action of input_actions.json went down this frame"},
            EventInfo{"ActionReleased", "Game::GameRuntime", "action",
                      "A named action of input_actions.json went up this frame"},
            EventInfo{"KeyPressed", "Game::GameRuntime", "key, name", "A key went down this frame, while playing"},
            EventInfo{"CollisionBegan", "Game::GameRuntime", "objectA, objectB, impactSpeed, point, normal",
                      "Two scene objects started touching (from the physics)"},
            EventInfo{"CollisionEnded", "Game::GameRuntime", "objectA, objectB", "Two scene objects stopped touching"},
            EventInfo{"MessageBroadcast", "Logic::LogicRunner, engine code",
                      "message, value, senderObjectId, fromBlocks, data",
                      "A message for the \"when I receive\" scripts, with a number"},
            EventInfo{"TimerStarted", "Logic::LogicRunner (\"start timer\")", "name, seconds, objectId, repeat",
                      "A countdown timer was started, or started over"},
            EventInfo{"TimerElapsed", "Game::TimerSystem", "name", "A countdown timer ran out"},
        };
    }

    std::span<const EventInfo> AllEvents()
    {
        return kEvents;
    }

    const EventInfo* FindEvent(std::string_view name)
    {
        const auto found =
            std::find_if(kEvents.begin(), kEvents.end(), [name](const EventInfo& info) { return info.name == name; });
        return found != kEvents.end() ? &*found : nullptr;
    }
}
