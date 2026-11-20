// Game::TimerSystem: countdown timers by name, started by TimerStarted on the event bus, sending TimerElapsed

#include "engine/game/TimerSystem.hpp"
#include "engine/messenger/EventBus.hpp"
#include "engine/messenger/Events.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Engine::Core::EventBus;
using Engine::Core::Subscription;
using Engine::Game::TimerSystem;
namespace Events = Engine::Events;

namespace
{
    // A timer system on a bus, and the names of the timers that ran out, in order
    class TimerSystemTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            timers_.Connect(&bus_);
            heard_ = bus_.Subscribe<Events::TimerElapsed>([this](const Events::TimerElapsed& e)
                                                          { elapsed_.push_back(e.name); });
        }

        EventBus bus_;
        TimerSystem timers_;
        std::vector<std::string> elapsed_;
        Subscription heard_;
    };
}

// A timer runs out once its seconds have gone by, and is sent once
TEST_F(TimerSystemTest, RunsOutOnceAfterItsSeconds)
{
    timers_.Start("alarm", 1.0);

    timers_.Update(0.6);
    EXPECT_TRUE(elapsed_.empty());
    timers_.Update(0.6);
    EXPECT_EQ(elapsed_, (std::vector<std::string>{"alarm"}));
    timers_.Update(5.0);
    EXPECT_EQ(elapsed_.size(), 1u) << "once";
    EXPECT_EQ(timers_.Count(), 0u);
}

// Starting a running timer again (any case) starts it over instead of making a second one
TEST_F(TimerSystemTest, StartingAgainStartsItOver)
{
    timers_.Start("Alarm", 1.0);
    timers_.Update(0.8);
    timers_.Start("alarm", 1.0);
    timers_.Update(0.8);

    EXPECT_TRUE(elapsed_.empty());
    EXPECT_EQ(timers_.Count(), 1u);
    ASSERT_TRUE(timers_.SecondsLeft("ALARM").has_value());
    EXPECT_NEAR(*timers_.SecondsLeft("ALARM"), 0.2, 1e-9);
}

// Timers that run out in the same update are sent first-to-run-out first
TEST_F(TimerSystemTest, SentInTheOrderTheyRunOut)
{
    timers_.Start("late", 0.5);
    timers_.Start("early", 0.1);

    timers_.Update(1.0);

    EXPECT_EQ(elapsed_, (std::vector<std::string>{"early", "late"}));
}

// TimerStarted on the bus (what a "start timer" block sends) starts a timer
TEST_F(TimerSystemTest, TimerStartedOnTheBusStartsOne)
{
    bus_.Emit(Events::TimerStarted{.name = "door", .seconds = 2.0});

    EXPECT_EQ(timers_.Count(), 1u);
    timers_.Update(2.5);
    EXPECT_EQ(elapsed_, (std::vector<std::string>{"door"}));
}

// A handler can start the timer that just ran out again (a repeating timer)
TEST_F(TimerSystemTest, HandlerCanStartTheSameTimerAgain)
{
    const Subscription again =
        bus_.Subscribe<Events::TimerElapsed>([this](const Events::TimerElapsed& e) { timers_.Start(e.name, 1.0); });
    timers_.Start("tick", 1.0);

    timers_.Update(1.0);
    timers_.Update(1.0);

    EXPECT_EQ(elapsed_, (std::vector<std::string>{"tick", "tick"}));
    EXPECT_EQ(timers_.Count(), 1u);
}

// Clear drops every timer: none of them is sent
TEST_F(TimerSystemTest, ClearDropsTimers)
{
    timers_.Start("a", 1.0);
    timers_.Start("b", 1.0);

    timers_.Clear();
    timers_.Update(5.0);

    EXPECT_TRUE(elapsed_.empty());
}

// A repeating timer runs out again and again, without drifting, until it is cleared
TEST_F(TimerSystemTest, ARepeatingTimerKeepsRunningOut)
{
    timers_.Start("tick", 1.0, true);
    for (int i = 0; i < 5; ++i)
    {
        timers_.Update(0.75);
    }
    EXPECT_EQ(elapsed_.size(), 3u) << "3.75 seconds is three rounds of one second";
    EXPECT_NEAR(*timers_.SecondsLeft("tick"), 0.25, 1e-9);

    timers_.Clear();
    timers_.Update(5.0);
    EXPECT_EQ(elapsed_.size(), 3u);
}

// Sent on the bus, the same
TEST_F(TimerSystemTest, TimerStartedCanRepeat)
{
    bus_.Emit(Events::TimerStarted{.name = "tick", .seconds = 1.0, .repeat = true});
    timers_.Update(1.0);
    timers_.Update(1.0);
    EXPECT_EQ(elapsed_.size(), 2u);
}

// Without a length between its rounds it runs out once, rather than every frame for ever
TEST_F(TimerSystemTest, ARepeatingTimerOfNoLengthRunsOutOnce)
{
    timers_.Start("now", 0.0, true);
    timers_.Update(0.1);
    timers_.Update(0.1);
    EXPECT_EQ(elapsed_.size(), 1u);
    EXPECT_EQ(timers_.Count(), 0u);
}

// One update far longer than a round sends it once, and the next round is a whole one
TEST_F(TimerSystemTest, ALongFrameDoesNotPileUpRounds)
{
    timers_.Start("tick", 1.0, true);
    timers_.Update(10.0);
    EXPECT_EQ(elapsed_.size(), 1u);
    EXPECT_NEAR(*timers_.SecondsLeft("tick"), 1.0, 1e-9);
}
