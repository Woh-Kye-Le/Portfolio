#include "engine/messenger/EventBus.hpp"
#include "engine/messenger/Events.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Engine::Core::EventBus;
using Engine::Core::Subscription;
namespace Events = Engine::Events;

// A handler gets the event it subscribed to, with its data.
TEST(EventBus, EmitCallsSubscribedHandler)
{
    EventBus bus;
    int width = 0;
    const Subscription sub =
        bus.Subscribe<Events::WindowResized>([&](const Events::WindowResized& e) { width = e.width; });

    bus.Emit(Events::WindowResized{1920, 1080});

    EXPECT_EQ(width, 1920);
}

// Handlers of other event types are not called.
TEST(EventBus, OnlyMatchingTypeIsCalled)
{
    EventBus bus;
    int calls = 0;
    const Subscription sub = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++calls; });

    bus.Emit(Events::WindowResized{});

    EXPECT_EQ(calls, 0);
}

// Handlers run in the order they subscribed.
TEST(EventBus, HandlersRunInSubscribeOrder)
{
    EventBus bus;
    std::vector<int> order;
    const Subscription a = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(1); });
    const Subscription b = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(2); });

    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

// Destroying the Subscription unsubscribes.
TEST(EventBus, SubscriptionUnsubscribesWhenDestroyed)
{
    EventBus bus;
    int calls = 0;
    {
        const Subscription sub = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++calls; });
        bus.Emit(Events::GameStopped{});
    }
    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(bus.ListenerCount<Events::GameStopped>(), 0u);
}

// Moving a Subscription keeps one handler subscribed, not two or zero.
TEST(EventBus, MovedSubscriptionStaysSubscribed)
{
    EventBus bus;
    int calls = 0;
    Subscription moved;
    {
        Subscription original = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++calls; });
        moved = std::move(original);
    }
    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(moved.IsActive());
}

// Queued events wait for DispatchQueued, and are delivered once.
TEST(EventBus, EnqueueWaitsForDispatch)
{
    EventBus bus;
    std::string action;
    const Subscription sub =
        bus.Subscribe<Events::ActionPressed>([&](const Events::ActionPressed& e) { action = e.action; });

    bus.Enqueue(Events::ActionPressed{"Jump"});
    EXPECT_TRUE(action.empty());
    EXPECT_EQ(bus.QueuedCount(), 1u);

    bus.DispatchQueued();
    EXPECT_EQ(action, "Jump");
    EXPECT_EQ(bus.QueuedCount(), 0u);
}

// An event enqueued by a handler during DispatchQueued waits for the next call (no infinite loop in one frame).
TEST(EventBus, EnqueueDuringDispatchWaitsForNextFrame)
{
    EventBus bus;
    int calls = 0;
    const Subscription sub = bus.Subscribe<Events::TimerElapsed>(
        [&](const Events::TimerElapsed& e)
        {
            ++calls;
            bus.Enqueue(Events::TimerElapsed{e.name + "+"});
        });

    bus.Enqueue(Events::TimerElapsed{"0"});
    bus.DispatchQueued();
    EXPECT_EQ(calls, 1);
    bus.DispatchQueued();
    EXPECT_EQ(calls, 2);
}

// A handler can unsubscribe itself, and another handler, while the event is being sent.
TEST(EventBus, UnsubscribeDuringEmitIsSafe)
{
    EventBus bus;
    int firstCalls = 0;
    int secondCalls = 0;
    Subscription second;
    Subscription first = bus.Subscribe<Events::GameStopped>(
        [&](const Events::GameStopped&)
        {
            ++firstCalls;
            first.Reset();
            second.Reset();
        });
    second = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++secondCalls; });

    bus.Emit(Events::GameStopped{});
    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 0); // removed before its turn
}

// A handler added during an Emit is first called by the next Emit.
TEST(EventBus, SubscribeDuringEmitStartsNextTime)
{
    EventBus bus;
    int lateCalls = 0;
    Subscription late;
    const Subscription sub = bus.Subscribe<Events::GameStopped>(
        [&](const Events::GameStopped&)
        {
            if (!late.IsActive())
            {
                late = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++lateCalls; });
            }
        });

    bus.Emit(Events::GameStopped{});
    EXPECT_EQ(lateCalls, 0);
    bus.Emit(Events::GameStopped{});
    EXPECT_EQ(lateCalls, 1);
}

// A Subscription that outlives its bus is harmless.
TEST(EventBus, SubscriptionOutlivingBusIsSafe)
{
    Subscription sub;
    {
        EventBus bus;
        sub = bus.Subscribe<Events::GameStopped>([](const Events::GameStopped&) {});
    }
    EXPECT_FALSE(sub.IsActive());
    sub.Reset(); // must not crash
}

// ---- Execution order (subtask 11) ----

// Queued events are delivered first in, first out - also across different event types.
TEST(EventBus, QueuedEventsDeliverInEnqueueOrder)
{
    EventBus bus;
    std::vector<std::string> order;
    const Subscription timers = bus.Subscribe<Events::TimerElapsed>(
        [&](const Events::TimerElapsed& e) { order.push_back("Timer" + e.name); });
    const Subscription actions = bus.Subscribe<Events::ActionPressed>(
        [&](const Events::ActionPressed& e) { order.push_back(e.action); });

    bus.Enqueue(Events::TimerElapsed{"1"});
    bus.Enqueue(Events::ActionPressed{"Jump"});
    bus.Enqueue(Events::TimerElapsed{"2"});
    bus.DispatchQueued();

    EXPECT_EQ(order, (std::vector<std::string>{"Timer1", "Jump", "Timer2"}));
}

// Emit inside a handler runs the inner event's handlers straight away, before the outer handler carries on.
TEST(EventBus, EmitInsideHandlerRunsImmediately)
{
    EventBus bus;
    std::vector<std::string> order;
    const Subscription inner = bus.Subscribe<Events::ActionPressed>(
        [&](const Events::ActionPressed&) { order.push_back("inner"); });
    const Subscription outer = bus.Subscribe<Events::GameStarted>(
        [&](const Events::GameStarted&)
        {
            order.push_back("outer begin");
            bus.Emit(Events::ActionPressed{"Jump"});
            order.push_back("outer end");
        });

    bus.Emit(Events::GameStarted{});

    EXPECT_EQ(order, (std::vector<std::string>{"outer begin", "inner", "outer end"}));
}

// Unsubscribing and subscribing again puts a handler at the end of the order.
TEST(EventBus, ResubscribedHandlerRunsLast)
{
    EventBus bus;
    std::vector<int> order;
    Subscription first = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(1); });
    const Subscription second =
        bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(2); });

    first.Reset();
    first = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(1); });
    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(order, (std::vector<int>{2, 1}));
}

// ---- Multiple listeners on the same event (subtask 12) ----

// Every listener is called, once each, and they all get the same event with the same data.
TEST(EventBus, AllListenersReceiveTheSameEvent)
{
    EventBus bus;
    std::vector<const Events::ActionPressed*> received;
    std::vector<std::string> actions;
    const auto record = [&](const Events::ActionPressed& e)
    {
        received.push_back(&e);
        actions.push_back(e.action);
    };
    const Subscription a = bus.Subscribe<Events::ActionPressed>(record);
    const Subscription b = bus.Subscribe<Events::ActionPressed>(record);
    const Subscription c = bus.Subscribe<Events::ActionPressed>(record);

    bus.Emit(Events::ActionPressed{"Jump"});

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], received[1]); // the same object, not copies
    EXPECT_EQ(received[1], received[2]);
    EXPECT_EQ(actions, (std::vector<std::string>{"Jump", "Jump", "Jump"}));
}

// Removing one listener leaves the others subscribed, still in their original order.
TEST(EventBus, RemovingOneListenerKeepsTheOthers)
{
    EventBus bus;
    std::vector<int> order;
    const Subscription a = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(1); });
    Subscription b = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(2); });
    const Subscription c = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(3); });

    b.Reset();
    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(order, (std::vector<int>{1, 3}));
    EXPECT_EQ(bus.ListenerCount<Events::GameStopped>(), 2u);
}

// The same handler subscribed twice is two listeners: it runs twice (the bus does not merge duplicates).
TEST(EventBus, SameHandlerSubscribedTwiceRunsTwice)
{
    EventBus bus;
    int calls = 0;
    const auto handler = [&](const Events::GameStopped&) { ++calls; };
    const Subscription first = bus.Subscribe<Events::GameStopped>(handler);
    const Subscription second = bus.Subscribe<Events::GameStopped>(handler);

    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(calls, 2);
}

// Listeners of one event type are not affected by listeners of another subscribing or leaving.
TEST(EventBus, ListenersOfDifferentTypesAreIndependent)
{
    EventBus bus;
    int stopped = 0;
    int started = 0;
    const Subscription onStop = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++stopped; });
    {
        const Subscription onStart =
            bus.Subscribe<Events::GameStarted>([&](const Events::GameStarted&) { ++started; });
        bus.Emit(Events::GameStopped{});
    }
    bus.Emit(Events::GameStopped{});
    bus.Emit(Events::GameStarted{});

    EXPECT_EQ(stopped, 2);
    EXPECT_EQ(started, 0); // its listener left before GameStarted was sent
    EXPECT_EQ(bus.ListenerCount<Events::GameStopped>(), 1u);
}

// Handlers with a higher priority run first, however they subscribed.
TEST(EventBus, HigherPrioritiesRunFirst)
{
    EventBus bus;
    std::vector<int> order;
    const auto note = [&order](int priority)
    { return [&order, priority](const Events::GameStopped&) { order.push_back(priority); }; };
    const Subscription low = bus.Subscribe<Events::GameStopped>(note(0), 0);
    const Subscription high = bus.Subscribe<Events::GameStopped>(note(10), 10);
    const Subscription middle = bus.Subscribe<Events::GameStopped>(note(5), 5);

    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(order, (std::vector<int>{10, 5, 0}));
}

// The same priority keeps the order of subscribing.
TEST(EventBus, EqualPrioritiesRunInSubscribeOrder)
{
    EventBus bus;
    std::vector<int> order;
    const Subscription first =
        bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(1); }, 5);
    const Subscription second =
        bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(2); }, 5);
    const Subscription third =
        bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(3); }, 5);

    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

// A handler can stop the event: the ones after it are not called, and the next Emit reaches everyone again.
TEST(EventBus, StopPropagationSkipsTheRest)
{
    EventBus bus;
    std::vector<int> order;
    bool stop = true;
    const Subscription first = bus.Subscribe<Events::GameStopped>(
        [&](const Events::GameStopped&)
        {
            order.push_back(1);
            if (stop)
            {
                bus.StopPropagation();
            }
        },
        10);
    const Subscription second =
        bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { order.push_back(2); });

    bus.Emit(Events::GameStopped{});
    EXPECT_EQ(order, std::vector<int>{1});

    stop = false;
    order.clear();
    bus.Emit(Events::GameStopped{});
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

// Stopping an event sent from inside a handler does not stop the event that handler is running for.
TEST(EventBus, StoppingAnInnerEmitLeavesTheOuterOneGoing)
{
    EventBus bus;
    int outerCalls = 0;
    const Subscription stops =
        bus.Subscribe<Events::GameUpdate>([&](const Events::GameUpdate&) { bus.StopPropagation(); });
    const Subscription first = bus.Subscribe<Events::GameStopped>(
        [&](const Events::GameStopped&)
        {
            ++outerCalls;
            bus.Emit(Events::GameUpdate{});
        });
    const Subscription second = bus.Subscribe<Events::GameStopped>([&](const Events::GameStopped&) { ++outerCalls; });

    bus.Emit(Events::GameStopped{});

    EXPECT_EQ(outerCalls, 2);
}

// Custom events carry their own data, by name.
TEST(EventBus, MessagesCarryTheirData)
{
    EventBus bus;
    std::string door;
    const Subscription sub =
        bus.Subscribe<Events::MessageBroadcast>([&](const Events::MessageBroadcast& e) { door = e.data.at("door"); });

    bus.Emit(Events::MessageBroadcast{.message = "opened", .data = {{"door", "north"}}});

    EXPECT_EQ(door, "north");
}
