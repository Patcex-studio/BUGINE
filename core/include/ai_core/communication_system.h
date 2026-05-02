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

#include <cstdint>
#include <vector>
#include <span>
#include <chrono>
#include <optional>
#include <queue>
#include <functional>

using EntityId = uint64_t;

// ============================================================================
// Communication System – Phase 3: Communication Delay
// ============================================================================

/**
 * CommunicationConfig
 * 
 * Configuration for communication delays and order priorities.
 */
struct CommunicationConfig {
    // Base delays
    float base_delay_ms = 100.0f;       // Baseline delay in ideal conditions
    float max_delay_ms = 500.0f;        // Max delay even with degradation
    
    // Interference factors
    float distance_multiplier = 0.01f;  // Delay increase per meter distance
    float interference_base = 0.0f;     // Base interference (0..1)
    float interference_max = 1.0f;      // Max interference (usually 1.0)
    
    // Safety
    float min_delay_ms = 10.0f;         // Minimum possible delay
};

/**
 * OrderPriority
 * 
 * Priority of orders affects effective delay.
 * Emergency orders bypass some of the delay.
 */
enum class OrderPriority : uint8_t {
    EMERGENCY = 0,      // Fire! Retreat! (20% delay multiplier)
    TACTICAL = 1,       // Standard maneuvers (100% delay)
    ADMINISTRATIVE = 2  // Resupply, regroup (150% delay)
};

/**
 * FormationOrder
 * 
 * Base order issued by commander to subordinates.
 * Contains payload data and timing info.
 */
struct FormationOrder {
    uint32_t order_id = 0;              // Unique identifier
    OrderPriority priority = OrderPriority::TACTICAL;
    std::chrono::microseconds issue_time{0};  // When issued
    EntityId issuer_id = 0;             // Commander who issued
    EntityId recipient_id = 0;          // Unit receiving order (0 = broadcast)
    
    // Payload (generic)
std::array<float, 8> parameters{};    // Command-specific parameters
    
    // Derived
    std::chrono::microseconds effective_delay{0};  // Computed delay for this order
    
    /**
     * Check if this order should be executed now
     */
    bool IsReady(std::chrono::microseconds current_time) const {
        auto elapsed = current_time - issue_time;
        return elapsed >= effective_delay;
    }
    
    /**
     * Get priority multiplier for this order
     * Determines how much of delay applies
     */
    float GetPriorityMultiplier() const {
        switch (priority) {
            case OrderPriority::EMERGENCY:
                return 0.2f;  // Emergency orders bypass 80% of delay
            case OrderPriority::TACTICAL:
                return 1.0f;  // Full delay
            case OrderPriority::ADMINISTRATIVE:
                return 1.5f;  // Slower
            default:
                return 1.0f;
        }
    }
};

/**
 * SubordinateState
 * 
 * State of a single subordinate unit regarding received orders.
 */
struct SubordinateState {
    EntityId unit_id = 0;
    
    // Current pending order
    std::optional<FormationOrder> pending_order;
    
    // Order execution history (diagnostics)
    uint32_t last_executed_order_id = 0;
    std::chrono::microseconds last_execute_time{0};
    
    // Diagnostics
    int orders_received = 0;
    int orders_executed = 0;
    
    /**
     * Check if current order is ready to execute
     */
    bool IsOrderReady(std::chrono::microseconds current_time) const {
        if (!pending_order) return false;
        return pending_order->IsReady(current_time);
    }
    
    /**
     * Accept a new order, replacing pending one
     */
    void AcceptOrder(const FormationOrder& order) {
        pending_order = order;
        orders_received++;
    }
    
    /**
     * Mark current order as executed
     */
    void ExecuteOrder(std::chrono::microseconds current_time) {
        if (pending_order) {
            last_executed_order_id = pending_order->order_id;
            last_execute_time = current_time;
            orders_executed++;
            pending_order.reset();
        }
    }
};

// ============================================================================
// CommunicationSystem
// 
// Manages order queuing and delay calculation for subordinate units.
// ============================================================================

class CommunicationSystem {
public:
    explicit CommunicationSystem(const CommunicationConfig& config);
    
    ~CommunicationSystem() = default;
    
    /**
     * Compute effective delay for an order
     * Based on distance, interference, priority.
     */
    std::chrono::microseconds ComputeEffectiveDelay(
        const FormationOrder& order,
        float distance_m,
        float interference
    ) const;
    
    /**
     * Issue an order to a subordinate or broadcast to multiple
     */
    void IssueOrder(
        EntityId issuer_id,
        EntityId recipient_id,  // 0 = broadcast
        const std::vector<float>& parameters,
        OrderPriority priority,
        float distance_m,
        float interference,
        std::chrono::microseconds current_time,
        std::vector<SubordinateState>& subordinates
    );
    
    /**
     * Update all subordinates: check if pending orders are ready
     * Returns list of (subordinate_id, order) for completed orders
     */
    std::vector<std::pair<EntityId, FormationOrder>>
    UpdateSubordinates(
        std::span<SubordinateState> subordinates,
        std::chrono::microseconds current_time
    );
    
    /**
     * Broadcast an order to all subordinates at once
     */
    void BroadcastOrder(
        EntityId issuer_id,
        const std::vector<float>& parameters,
        OrderPriority priority,
        float max_distance_m,
        float interference,
        std::chrono::microseconds current_time,
        std::span<SubordinateState> subordinates,
        // Optional: per-subordinate distance lookup
        std::function<float(EntityId)> get_distance = nullptr
    );
    
    /**
     * Get communication diagnostics (for UI/debugging)
     */
    struct Diagnostics {
        int total_orders_issued = 0;
        int total_orders_executed = 0;
        int pending_orders = 0;
        float average_delay_ms = 0.0f;
    };
    
    Diagnostics GetDiagnostics(std::span<const SubordinateState> subordinates) const;
    
    /**
     * Get config
     */
    const CommunicationConfig& GetConfig() const { return config_; }
    
private:
    CommunicationConfig config_;
    uint32_t next_order_id_ = 1;
    
    /**
     * Internal: compute delay from components
     */
    std::chrono::microseconds ComputeDelayComponents(
        float interference,
        float distance_m,
        float priority_mult
    ) const;
};

// ============================================================================
// CommandQueue
// 
// Per-unit queue of pending commands with delayed execution.
// Used to integrate delayed communication with decision engine.
// ============================================================================

class CommandQueue {
public:
    CommandQueue() = default;
    
    /**
     * Queue a command with delayed execution
     */
    void EnqueueCommand(
        const FormationOrder& order
    );
    
    /**
     * Get next ready command (if any)
     */
    std::optional<FormationOrder> GetNextReadyCommand(
        std::chrono::microseconds current_time
    );
    
    /**
     * Peek at next command without removing it
     */
    const std::optional<FormationOrder>& PeekNextCommand() const;
    
    /**
     * Check if any commands are pending
     */
    bool HasPending() const { return !queue_.empty(); }
    
    /**
     * Clear all pending commands
     */
    void Clear() { queue_ = std::queue<FormationOrder>(); }
    
private:
    std::queue<FormationOrder> queue_;
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace CommunicationUtils {
    /**
     * Create a MOVE order
     */
    FormationOrder CreateMoveOrder(
        EntityId issuer,
        EntityId recipient,
        float destination_x,
        float destination_y,
        float destination_z,
        std::chrono::microseconds current_time
    );
    
    /**
     * Create an ATTACK order
     */
    FormationOrder CreateAttackOrder(
        EntityId issuer,
        EntityId recipient,
        EntityId target_id,
        std::chrono::microseconds current_time
    );
    
    /**
     * Create a SEEK_COVER order
     */
    FormationOrder CreateSeekCoverOrder(
        EntityId issuer,
        EntityId recipient,
        float cover_x,
        float cover_y,
        float cover_z,
        std::chrono::microseconds current_time
    );
}
