/* !
@file    	    Events.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		31/10/2026
@brief
    The engine's event types, sent through Core::EventBus: window, game
    runtime and (planned) block-logic events.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include <glm/vec3.hpp>

// Events are plain structs: no base class, no virtuals, copyable. Add new ones here, grouped by who sends them.
namespace Engine::Events
{
    // ---- Application / window (sent by Core::Application) ----

    // The window's drawable size changed (0 x 0 while minimized)
    struct WindowResized
    {
        int width = 0;
        int height = 0;
    };

    // ---- Game runtime (sent by Game::GameRuntime) ----

    // A scene finished loading and its entities exist
    struct GameStarted
    {
        std::string projectName;
        std::size_t objectCount = 0;
    };

    // The running scene was torn down (sent before its entities are destroyed)
    struct GameStopped
    {
    };

    // Play / Pause in the editor: systems that change the world run only while simulating
    struct SimulationChanged
    {
        bool simulating = false;
    };

    // Once per frame while the game is playing (not while paused), after the built-in systems and input events
    struct GameUpdate
    {
        double deltaSeconds = 0.0;
    };

    // ---- Input (sent by Game::GameRuntime while playing) ----

    // A named action from input_actions.json went down this frame
    struct ActionPressed
    {
        std::string action;
    };

    // A named action from input_actions.json went up this frame
    struct ActionReleased
    {
        std::string action;
    };

    // A key went down this frame (any key, bound to an action or not; not while the editor UI has the keyboard)
    struct KeyPressed
    {
        int key = 0;      // Input::KeyCode
        std::string name; // as in input_actions.json: "Space", "W", "LeftShift", ...
    };

    // ---- Physics (sent by Game::GameRuntime while playing, from what the physics reports) ----

    // Two scene objects started touching (object ids, as in the scene file; 0 for an entity that is not a scene
    // object, such as a spawned copy)
    struct CollisionBegan
    {
        std::uint32_t objectA = 0;
        std::uint32_t objectB = 0;
        float impactSpeed = 0.0f; // how fast they were closing in, in units per second (how hard they hit)
        glm::vec3 point{0.0f};    // where they touch, in world space
        glm::vec3 normal{0.0f};   // unit length, pointing from objectA to objectB
    };

    // Two scene objects stopped touching (or one of them was removed)
    struct CollisionEnded
    {
        std::uint32_t objectA = 0;
        std::uint32_t objectB = 0;
    };

    // ---- Block messages (sent by Logic::LogicRunner, or by engine code) ----

    // A message for the "when I receive" scripts. A Broadcast block sends one (fromBlocks, with the object whose block
    // it was); engine code can send one too, and the blocks hear it on their next update.
    struct MessageBroadcast
    {
        std::string message;
        std::uint32_t senderObjectId = 0; // 0: sent by engine code
        bool fromBlocks = false;
        double value = 0.0; // a number sent with it ("broadcast ... with ..."): the receivers' "message value"
        std::map<std::string, std::string> data; // anything else engine code sends with it, by name
    };

    // ---- Timers (the "start timer" and "when timer ends" blocks; Game::TimerSystem counts them down) ----

    // A countdown timer was started (a "start timer" block), or started over if one with this name was running
    struct TimerStarted
    {
        std::string name;
        double seconds = 0.0;
        std::uint32_t objectId = 0; // the object whose block started it; 0: engine code
        bool repeat = false;        // starts over each time it runs out, until cleared
    };

    // A countdown timer ran out
    struct TimerElapsed
    {
        std::string name;
    };
}
