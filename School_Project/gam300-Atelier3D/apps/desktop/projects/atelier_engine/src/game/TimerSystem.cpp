/* !
@file    	    TimerSystem.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		03/11/2026
@brief
    Implements TimerSystem: starting countdown timers by name, counting them
    down each frame, and sending Events::TimerElapsed when one runs out.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/TimerSystem.hpp"

#include "engine/messenger/Events.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Engine::Game
{
    namespace
    {
        bool SameName(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(),
                              [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }
    }

    void TimerSystem::Connect(Core::EventBus* events)
    {
        events_ = events;
        onStarted_ = {};
        if (events != nullptr)
        {
            onStarted_ = events->Subscribe<Events::TimerStarted>([this](const Events::TimerStarted& e)
                                                                 { Start(e.name, e.seconds, e.repeat); });
        }
    }

    void TimerSystem::Start(std::string name, double seconds, bool repeat)
    {
        seconds = std::max(seconds, 0.0); // 0 or less: runs out on the next update
        const double interval = repeat ? seconds : 0.0;
        const auto found = std::find_if(timers_.begin(), timers_.end(),
                                        [&name](const Timer& timer) { return SameName(timer.name, name); });
        if (found != timers_.end())
        {
            found->secondsLeft = seconds;
            found->interval = interval;
            return;
        }
        timers_.push_back(Timer{std::move(name), seconds, interval});
    }

    void TimerSystem::Update(double deltaSeconds)
    {
        std::vector<Timer> elapsed;
        for (Timer& timer : timers_)
        {
            timer.secondsLeft -= deltaSeconds;
        }
        // Taken out before any is sent, so a handler can start the same timer again
        const auto done = std::stable_partition(timers_.begin(), timers_.end(),
                                                [](const Timer& timer) { return timer.secondsLeft > 0.0; });
        elapsed.assign(std::make_move_iterator(done), std::make_move_iterator(timers_.end()));
        timers_.erase(done, timers_.end());
        std::stable_sort(elapsed.begin(), elapsed.end(),
                         [](const Timer& a, const Timer& b) { return a.secondsLeft < b.secondsLeft; });
        for (const Timer& timer : elapsed)
        {
            if (timer.interval > 0.0)
            {
                // What the last round ran over comes off the next, so a repeating timer does not drift
                const double next = timer.secondsLeft + timer.interval;
                timers_.push_back(Timer{timer.name, next > 0.0 ? next : timer.interval, timer.interval});
            }
        }

        if (events_ != nullptr)
        {
            for (Timer& timer : elapsed)
            {
                events_->Emit(Events::TimerElapsed{std::move(timer.name)});
            }
        }
    }

    void TimerSystem::Clear()
    {
        timers_.clear();
    }

    std::optional<double> TimerSystem::SecondsLeft(std::string_view name) const
    {
        const auto found = std::find_if(timers_.begin(), timers_.end(),
                                        [name](const Timer& timer) { return SameName(timer.name, name); });
        return found != timers_.end() ? std::optional(found->secondsLeft) : std::nullopt;
    }
}
