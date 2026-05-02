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
#include "ai_core/communication_system.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// CommunicationSystem Implementation
// ============================================================================

CommunicationSystem::CommunicationSystem(const CommunicationConfig& config)
    : config_(config) {
}

std::chrono::microseconds CommunicationSystem::ComputeEffectiveDelay(
    const FormationOrder& order,
    float distance_m,
    float interference) const {
    
    return ComputeDelayComponents(
        interference,
        distance_m,
        order.GetPriorityMultiplier()
    );
}

std::chrono::microseconds CommunicationSystem::ComputeDelayComponents(
    float interference,
    float distance_m,
    float priority_mult) const {
    
    // Base formula from ТЗ:
    // effective_delay = base_delay * (1.0 + interference) * priority_mult
    
    float base_ms = config_.base_delay_ms;
    float distance_penalty = distance_m * config_.distance_multiplier;
    float total_multiplier = (1.0f + interference + distance_penalty) * priority_mult;
    
    float delay_ms = base_ms * total_multiplier;
    
    // Clamp to valid range
    delay_ms = std::clamp(delay_ms, config_.min_delay_ms, config_.max_delay_ms);
    
    // Convert to microseconds
    auto result = std::chrono::microseconds(static_cast<int64_t>(delay_ms * 1000.0f));
    return result;
}

void CommunicationSystem::IssueOrder(
    EntityId issuer_id,
    EntityId recipient_id,
    const std::vector<float>& parameters,
    OrderPriority priority,
    float distance_m,
    float interference,
    std::chrono::microseconds current_time,
    std::vector<SubordinateState>& subordinates) {
    
    FormationOrder order;
    order.order_id = next_order_id_++;
    order.issuer_id = issuer_id;
    order.recipient_id = recipient_id;
    order.priority = priority;
    order.issue_time = current_time;
    order.effective_delay = ComputeEffectiveDelay(order, distance_m, interference);
    
    // Copy parameters
    if (!parameters.empty()) {
        for (size_t i = 0; i < std::min(parameters.size(), size_t(8)); ++i) {
            order.parameters[i] = parameters[i];
        }
    }
    
    // Find recipient(s)
    if (recipient_id == 0) {
        // Broadcast
        for (auto& sub : subordinates) {
            sub.AcceptOrder(order);
        }
    } else {
        // Unicast
        for (auto& sub : subordinates) {
            if (sub.unit_id == recipient_id) {
                sub.AcceptOrder(order);
                break;
            }
        }
    }
}

std::vector<std::pair<EntityId, FormationOrder>>
CommunicationSystem::UpdateSubordinates(
    std::span<SubordinateState> subordinates,
    std::chrono::microseconds current_time) {
    
    std::vector<std::pair<EntityId, FormationOrder>> completed;
    
    for (auto& sub : subordinates) {
        if (sub.IsOrderReady(current_time)) {
            if (sub.pending_order) {
                completed.push_back({sub.unit_id, *sub.pending_order});
            }
            sub.ExecuteOrder(current_time);
        }
    }
    
    return completed;
}

void CommunicationSystem::BroadcastOrder(
    EntityId issuer_id,
    const std::vector<float>& parameters,
    OrderPriority priority,
    float max_distance_m,
    float interference,
    std::chrono::microseconds current_time,
    std::span<SubordinateState> subordinates,
    std::function<float(EntityId)> get_distance) {
    
    for (auto& sub : subordinates) {
        float distance = (get_distance) ? get_distance(sub.unit_id) : max_distance_m;
        
        FormationOrder order;
        order.order_id = next_order_id_++;
        order.issuer_id = issuer_id;
        order.recipient_id = sub.unit_id;
        order.priority = priority;
        order.issue_time = current_time;
        order.effective_delay = ComputeEffectiveDelay(order, distance, interference);
        if (!parameters.empty()) {
            for (size_t i = 0; i < std::min(parameters.size(), size_t(8)); ++i) {
                order.parameters[i] = parameters[i];
            }
        }
        sub.AcceptOrder(order);
    }
}

CommunicationSystem::Diagnostics CommunicationSystem::GetDiagnostics(
    std::span<const SubordinateState> subordinates) const {
    
    Diagnostics diag;
    
    for (const auto& sub : subordinates) {
        diag.total_orders_issued += sub.orders_received;
        diag.total_orders_executed += sub.orders_executed;
        if (sub.pending_order) {
            diag.pending_orders++;
        }
    }
    
    return diag;
}

// ============================================================================
// CommandQueue Implementation
// ============================================================================

void CommandQueue::EnqueueCommand(const FormationOrder& order) {
    queue_.push(order);
}

std::optional<FormationOrder> CommandQueue::GetNextReadyCommand(
    std::chrono::microseconds current_time) {
    
    while (!queue_.empty()) {
        auto& next = queue_.front();
        if (next.IsReady(current_time)) {
            FormationOrder result = next;
            queue_.pop();
            return result;
        }
        break;  // Not ready, don't dequeue
    }
    
    return std::nullopt;
}

const std::optional<FormationOrder>& CommandQueue::PeekNextCommand() const {
    if (queue_.empty()) {
        static const std::optional<FormationOrder> empty = std::nullopt;
        return empty;
    }
    return queue_.front();  // This is problematic, need to fix signature
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

namespace CommunicationUtils {

FormationOrder CreateMoveOrder(
    EntityId issuer,
    EntityId recipient,
    float destination_x,
    float destination_y,
    float destination_z,
    std::chrono::microseconds current_time) {
    
    FormationOrder order;
    order.issuer_id = issuer;
    order.recipient_id = recipient;
    order.priority = OrderPriority::TACTICAL;
    order.issue_time = current_time;
    order.parameters[0] = destination_x;
    order.parameters[1] = destination_y;
    order.parameters[2] = destination_z;
    
    return order;
}

FormationOrder CreateAttackOrder(
    EntityId issuer,
    EntityId recipient,
    EntityId target_id,
    std::chrono::microseconds current_time) {
    
    FormationOrder order;
    order.issuer_id = issuer;
    order.recipient_id = recipient;
    order.priority = OrderPriority::TACTICAL;
    order.issue_time = current_time;
    order.parameters[0] = static_cast<float>(target_id);
    
    return order;
}

FormationOrder CreateSeekCoverOrder(
    EntityId issuer,
    EntityId recipient,
    float cover_x,
    float cover_y,
    float cover_z,
    std::chrono::microseconds current_time) {
    
    FormationOrder order;
    order.issuer_id = issuer;
    order.recipient_id = recipient;
    order.priority = OrderPriority::EMERGENCY;  // Cover seeking is urgent
    order.issue_time = current_time;
    order.parameters[0] = cover_x;
    order.parameters[1] = cover_y;
    order.parameters[2] = cover_z;
    
    return order;
}

}  // namespace CommunicationUtils
