#include "engine/core/FixedSteps.hpp"
#include "engine/core/Time.hpp"

#include <gtest/gtest.h>

#include <chrono>

using Engine::Core::Time;

// 1.5 s apart reads as 1.5, and the other way round as -1.5.
// One second of frames makes the same number of steps at any frame rate: what runs in steps goes at the same speed
TEST(FixedSteps, TheFrameRateDoesNotChangeHowManyStepsASecondMakes)
{
    for (const int fps : {30, 60, 115, 144, 240})
    {
        double seconds = 0.0;
        int steps = 0;
        for (int frame = 0; frame < fps; ++frame)
        {
            steps += Engine::Core::TakeFixedSteps(seconds, 1.0 / fps, 1.0 / 60.0, 4);
        }
        EXPECT_NEAR(steps, 60, 1) << fps << " FPS";
    }
}

// A frame longer than the most steps allowed drops the rest, so a hitch does not make the next frames longer still
TEST(FixedSteps, ALongFrameDropsWhatItCannotStep)
{
    double seconds = 0.0;
    EXPECT_EQ(Engine::Core::TakeFixedSteps(seconds, 5.0, 1.0 / 60.0, 4), 4);
    EXPECT_DOUBLE_EQ(seconds, 0.0);
    EXPECT_EQ(Engine::Core::TakeFixedSteps(seconds, -1.0, 1.0 / 60.0, 4), 0);
}

TEST(Time, SecondsBetweenMeasuresDuration)
{
    const auto start = Time::Now();
    const auto end = start + std::chrono::milliseconds(1500);

    EXPECT_DOUBLE_EQ(Time::SecondsBetween(start, end), 1.5);
    EXPECT_DOUBLE_EQ(Time::SecondsBetween(end, start), -1.5);
}

// steady_clock should never give a time earlier than the last one.
TEST(Time, NowNeverGoesBackwards)
{
    const auto first = Time::Now();
    const auto second = Time::Now();
    EXPECT_GE(Time::SecondsBetween(first, second), 0.0);
}
