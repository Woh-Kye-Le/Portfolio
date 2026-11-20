// The event registry: every event type of Events.hpp has a name, and the registry lists each once

#include "engine/messenger/EventRegistry.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string_view>
#include <vector>

namespace Events = Engine::Events;

// Each event type's name is in the registry, with who sends it and what it means; no name is listed twice
TEST(EventRegistry, EveryEventTypeIsListedOnce)
{
    const std::vector<std::string_view> names{
        Events::EventName<Events::WindowResized>(),     Events::EventName<Events::GameStarted>(),
        Events::EventName<Events::GameStopped>(),       Events::EventName<Events::SimulationChanged>(),
        Events::EventName<Events::GameUpdate>(),        Events::EventName<Events::ActionPressed>(),
        Events::EventName<Events::ActionReleased>(),    Events::EventName<Events::KeyPressed>(),
        Events::EventName<Events::CollisionBegan>(),    Events::EventName<Events::CollisionEnded>(),
        Events::EventName<Events::MessageBroadcast>(),  Events::EventName<Events::TimerStarted>(),
        Events::EventName<Events::TimerElapsed>(),
    };

    EXPECT_EQ(Events::AllEvents().size(), names.size()) << "an event type was added without a registry entry";
    std::set<std::string_view> seen;
    for (const Events::EventInfo& info : Events::AllEvents())
    {
        EXPECT_TRUE(seen.insert(info.name).second) << info.name << " is listed twice";
        EXPECT_FALSE(info.sender.empty()) << info.name;
        EXPECT_FALSE(info.help.empty()) << info.name;
    }
    for (const std::string_view name : names)
    {
        EXPECT_NE(Events::FindEvent(name), nullptr) << name << " is missing from the registry";
    }
}

// FindEvent finds by exact name, and nothing for a name no event has
TEST(EventRegistry, FindEventByName)
{
    const Events::EventInfo* found = Events::FindEvent("CollisionBegan");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "CollisionBegan");
    EXPECT_EQ(Events::FindEvent("Teleported"), nullptr);
}
