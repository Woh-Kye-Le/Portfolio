/* !
@file    	    EventBus.hpp
@author  	    beh.s@digipen.edu [100%]
@date    		31/10/2026
@brief
    Declares EventBus, a type-safe publish/subscribe event system: systems
    Subscribe to an event type and get a Subscription back (unsubscribes when
    destroyed), and Emit an event now or Enqueue it for the next frame.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Core
{
    namespace Detail
    {
        // One subscribed handler. Shared so Emit can keep it alive while it runs, even if it unsubscribes itself.
        struct EventListener
        {
            std::uint64_t id = 0;
            int priority = 0;
            bool active = true;
            std::function<void(const void*)> call; // gets a pointer to the event, already the right type
        };

        // Everything the bus owns. Subscriptions hold a weak_ptr to it, so a Subscription that outlives its bus
        // does nothing instead of crashing.
        struct EventBusState
        {
            std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventListener>>> channels;
            std::vector<std::function<void()>> queue;
            std::uint64_t nextId = 1;
            bool stopped = false; // StopPropagation was called in the Emit that is running

            void Remove(std::type_index type, std::uint64_t id);
        };
    }

    // Returned by EventBus::Subscribe. The handler stays subscribed while this object lives, so keep it as a member
    // of whatever owns the handler. Move-only.
    class Subscription
    {
    public:
        Subscription() = default;
        ~Subscription();

        Subscription(Subscription&& other) noexcept;
        Subscription& operator=(Subscription&& other) noexcept;
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        // Unsubscribes now (safe to call more than once, and from inside the handler itself)
        void Reset();
        bool IsActive() const;

    private:
        friend class EventBus;

        Subscription(std::weak_ptr<Detail::EventBusState> state, std::type_index type, std::uint64_t id)
            : state_(std::move(state)), type_(type), id_(id)
        {
        }

        std::weak_ptr<Detail::EventBusState> state_;
        std::optional<std::type_index> type_;
        std::uint64_t id_ = 0;
    };

    // Type-safe publish/subscribe. An event is any plain struct (see engine/core/Events.hpp).
    //
    //     Subscription sub = bus.Subscribe<Events::WindowResized>([](const Events::WindowResized& e) { ... });
    //     bus.Emit(Events::WindowResized{1920, 1080});    // handlers run now
    //     bus.Enqueue(Events::GameStarted{});             // handlers run at the next DispatchQueued()
    //
    // Main thread only (not thread-safe). Handlers may subscribe, unsubscribe and emit from inside a handler:
    // a handler added during an Emit is first called by the next Emit of that type, and events enqueued during
    // DispatchQueued() wait for the next call, so an event can never loop forever inside one frame.
    class EventBus
    {
    public:
        EventBus();
        ~EventBus();

        // Subscriptions and queued events point back at this bus, so it stays where it was made
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) = delete;
        EventBus& operator=(EventBus&&) = delete;

        // Higher priorities run first; handlers of the same priority run in the order they subscribed
        template <class E, class Fn> [[nodiscard]] Subscription Subscribe(Fn&& handler, int priority = 0);

        // Calls every handler of E right away, highest priority first
        template <class E> void Emit(const E& event);
        // From inside a handler: the handlers still to come in this Emit are not called
        void StopPropagation() { state_->stopped = true; }

        // Stores a copy of the event; its handlers run at the next DispatchQueued()
        template <class E> void Enqueue(E event);

        // Delivers everything enqueued so far, oldest first. Application::Run calls this once per frame.
        void DispatchQueued();
        // Throws away queued events without delivering them (e.g. when a scene stops)
        void ClearQueue();

        std::size_t QueuedCount() const;
        template <class E> std::size_t ListenerCount() const;

    private:
        std::shared_ptr<Detail::EventBusState> state_;
    };

    template <class E, class Fn> Subscription EventBus::Subscribe(Fn&& handler, int priority)
    {
        static_assert(std::is_class_v<E>, "Events are structs (see engine/core/Events.hpp)");
        static_assert(std::is_invocable_v<std::decay_t<Fn>&, const E&>,
                      "The handler must be callable as handler(const E&)");

        auto listener = std::make_shared<Detail::EventListener>();
        listener->id = state_->nextId++;
        listener->priority = priority;
        listener->call = [fn = std::forward<Fn>(handler)](const void* event) mutable
        { fn(*static_cast<const E*>(event)); };

        const std::type_index type(typeid(E));
        auto& listeners = state_->channels[type];
        const auto place = std::upper_bound(listeners.begin(), listeners.end(), priority,
                                            [](int value, const std::shared_ptr<Detail::EventListener>& each)
                                            { return value > each->priority; });
        listeners.insert(place, listener);
        return Subscription(state_, type, listener->id);
    }

    template <class E> void EventBus::Emit(const E& event)
    {
        const auto channel = state_->channels.find(std::type_index(typeid(E)));
        if (channel == state_->channels.end() || channel->second.empty())
        {
            return;
        }

        // A copy of the list: handlers can subscribe / unsubscribe while we walk it. Someone removed earlier in
        // this Emit is skipped through the active flag.
        const std::vector<std::shared_ptr<Detail::EventListener>> listeners = channel->second;
        // Kept alive and put back: a handler can Emit, and stopping that inner event must not stop this one
        const std::shared_ptr<Detail::EventBusState> state = state_;
        const bool outerStopped = state->stopped;
        state->stopped = false;
        for (const auto& listener : listeners)
        {
            if (listener->active)
            {
                listener->call(&event);
            }
            if (state->stopped)
            {
                break;
            }
        }
        state->stopped = outerStopped;
    }

    template <class E> void EventBus::Enqueue(E event)
    {
        static_assert(std::is_copy_constructible_v<E>, "Queued events are stored by copy");
        state_->queue.emplace_back([this, stored = std::move(event)] { Emit(stored); });
    }

    template <class E> std::size_t EventBus::ListenerCount() const
    {
        const auto channel = state_->channels.find(std::type_index(typeid(E)));
        return channel == state_->channels.end() ? 0 : channel->second.size();
    }
}
