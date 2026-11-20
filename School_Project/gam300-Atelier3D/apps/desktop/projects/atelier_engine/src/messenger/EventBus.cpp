/* !
@file    	    EventBus.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		31/10/2026
@brief
    Implements the non-template parts of EventBus: unsubscribing, the
    Subscription handle, and delivering queued events once per frame.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/messenger/EventBus.hpp"

#include <algorithm>

namespace Engine::Core
{
    namespace Detail
    {
        void EventBusState::Remove(std::type_index type, std::uint64_t id)
        {
            const auto channel = channels.find(type);
            if (channel == channels.end())
            {
                return;
            }

            auto& listeners = channel->second;
            const auto it = std::find_if(listeners.begin(), listeners.end(),
                                         [id](const std::shared_ptr<EventListener>& l) { return l->id == id; });
            if (it != listeners.end())
            {
                (*it)->active = false; // an Emit already walking its copy of the list skips it
                listeners.erase(it);
            }
        }
    }

    // ---- Subscription ----

    Subscription::~Subscription()
    {
        Reset();
    }

    Subscription::Subscription(Subscription&& other) noexcept
        : state_(std::move(other.state_)), type_(other.type_), id_(other.id_)
    {
        other.type_.reset();
        other.id_ = 0;
    }

    Subscription& Subscription::operator=(Subscription&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            state_ = std::move(other.state_);
            type_ = other.type_;
            id_ = other.id_;
            other.type_.reset();
            other.id_ = 0;
        }
        return *this;
    }

    void Subscription::Reset()
    {
        if (type_)
        {
            if (const auto state = state_.lock()) // the bus may already be gone
            {
                state->Remove(*type_, id_);
            }
        }
        state_.reset();
        type_.reset();
        id_ = 0;
    }

    bool Subscription::IsActive() const
    {
        return type_.has_value() && !state_.expired();
    }

    // EventBus

    EventBus::EventBus() : state_(std::make_shared<Detail::EventBusState>()) {}

    EventBus::~EventBus() = default;

    void EventBus::DispatchQueued()
    {
        // Swap the queue out first: events enqueued by these handlers go to the next call, not this one
        std::vector<std::function<void()>> pending;
        pending.swap(state_->queue);
        for (auto& deliver : pending)
        {
            deliver();
        }
    }

    void EventBus::ClearQueue()
    {
        state_->queue.clear();
    }

    std::size_t EventBus::QueuedCount() const
    {
        return state_->queue.size();
    }
}
