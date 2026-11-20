/* !
@file    	    TimerSystem.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		03/11/2026
@brief
    Declares TimerSystem, the countdown timers of a running game: started by
    name (the "start timer" block sends Events::TimerStarted on the event bus),
    and each one that runs out sends Events::TimerElapsed.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/messenger/EventBus.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Game
{
    // Countdown timers, by name. Names ignore case, like block messages ("Alarm" and "alarm" are one timer); starting
    // one that is already running starts it over. GameRuntime counts them down while the game simulates (not while
    // paused) and clears them when the game stops.
    class TimerSystem
    {
    public:
        TimerSystem() = default;
        // Its subscription points back at it, so it stays where it was made
        TimerSystem(const TimerSystem&) = delete;
        TimerSystem& operator=(const TimerSystem&) = delete;

        // Starts timers for the Events::TimerStarted sent on events, and sends Events::TimerElapsed there
        // (nullptr: neither)
        void Connect(Core::EventBus* events);

        // A repeating timer starts over each time it runs out (one of 0 seconds or less runs out once, as there would
        // be no time between its rounds)
        void Start(std::string name, double seconds, bool repeat = false);
        // Counts every timer down; each that runs out is sent as TimerElapsed, the first to run out first, and removed
        // unless it repeats
        void Update(double deltaSeconds);
        void Clear();

        std::size_t Count() const { return timers_.size(); }
        // Seconds until the timer runs out; nullopt if no timer has that name
        std::optional<double> SecondsLeft(std::string_view name) const;

    private:
        struct Timer
        {
            std::string name;
            double secondsLeft = 0.0;
            double interval = 0.0; // above 0: it repeats after this long
        };

        std::vector<Timer> timers_; // in the order they were started
        Core::EventBus* events_ = nullptr;
        Core::Subscription onStarted_;
    };
}
