/*
 Copyright (C) 2026 Jocer S. <patcex@proton.me>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 SPDX-License-Identifier: AGPL-3.0 OR Commercial
*/
#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "physics_core/types.h"

namespace common {

struct CollisionEvent {
    physics_core::EntityID entity_a;
    physics_core::EntityID entity_b;
    physics_core::Vec3 point;
    physics_core::Vec3 normal;
    float impulse;
    float relative_speed;
};

struct DestructionEvent {
    physics_core::EntityID entity;
    physics_core::Vec3 center;
    float radius;
    float explosive_mass;
};

class EventBus {
public:
    using EventCallback = std::function<void(const std::any&)>;

    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    template<typename Event>
    static void Publish(const Event& event) {
        instance().publish_impl(std::any(event));
    }

    template<typename Event>
    static void Subscribe(std::function<void(const Event&)> callback) {
        instance().subscribe_impl(typeid(Event), [callback = std::move(callback)](const std::any& any_event) {
            callback(std::any_cast<const Event&>(any_event));
        });
    }

    static void DispatchAll() {
        instance().dispatch_all_impl();
    }

private:
    struct EventNode {
        std::any event;
        std::atomic<EventNode*> next{nullptr};

        EventNode() = default;
        explicit EventNode(std::any&& payload)
            : event(std::move(payload)) {}
    };

    EventBus()
        : tail_(&stub_) {
        head_.store(&stub_, std::memory_order_relaxed);
    }

    void publish_impl(std::any&& event) {
        EventNode* node = new EventNode(std::move(event));
        EventNode* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    void subscribe_impl(const std::type_index& type, EventCallback callback) {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_[type].emplace_back(std::move(callback));
    }

    void dispatch_all_impl() {
        EventNode* node = tail_->next.load(std::memory_order_acquire);
        while (node != nullptr) {
            const std::type_index event_type{node->event.type()};
            std::vector<EventCallback> callbacks;
            {
                std::lock_guard<std::mutex> lock(subscribers_mutex_);
                auto it = subscribers_.find(event_type);
                if (it != subscribers_.end()) {
                    callbacks = it->second;
                }
            }

            for (auto& callback : callbacks) {
                callback(node->event);
            }

            EventNode* next = node->next.load(std::memory_order_acquire);
            delete node;
            node = next;
        }

        tail_ = head_.load(std::memory_order_acquire);
    }

    std::atomic<EventNode*> head_;
    EventNode* tail_;
    EventNode stub_;

    std::mutex subscribers_mutex_;
    std::unordered_map<std::type_index, std::vector<EventCallback>> subscribers_;
};

} // namespace common
