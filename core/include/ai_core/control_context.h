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

#include "decision_engine.h"
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <optional>

// ============================================================================
// Control Priority System
// ============================================================================

/**
 * Control priority enum for determining who controls a unit
 */
enum class ControlPriority {
    AI_AUTO = 0,        // AI has full autonomous control
    PLAYER_DIRECT = 1,  // Player is directing unit directly
    NUM_PRIORITIES      // Total states
};

// ============================================================================
// Control Context
// ============================================================================

/**
 * ControlContext
 * 
 * Maintains control state for each unit, including:
 * - Current control priority (AI vs player)
 * - Cached behavioral state for instant resumption
 * - Smooth transition parameters
 * 
 * Thread-safety:
 * - ControlPriority is atomic for thread-safe transitions
 * - Other fields are copyable and passed by value to worker threads
 */
struct ControlContext {
    // ========== Control State ==========
    std::atomic<ControlPriority> control_priority{ControlPriority::AI_AUTO};
    
    uint64_t takeover_time_ms = 0;  // When control switched to PLAYER_DIRECT
    
    // ========== Behavioral State Cache ==========
    std::optional<BTNodeSnapshot> cached_node;  // Saved decision state
    std::vector<Command> last_input;            // Last player input for smoothing
    
    // ========== Adaptation Period (AI→Player) ==========
    // When returning from player control, AI gradually takes over
    float adaptation_time_remaining = 0.0f;     // Seconds left in adaptation period
    static constexpr float ADAPTATION_DURATION = 0.3f;  // Configured in AIConfig
    
    // ========== Decision Caching (Reaction Lag) ==========
    std::optional<DecisionCache> cached_decision;  // Cached decision during lag period
    
    // ========== Utility Functions ==========
    
    /**
     * Check if this unit is under player control
     */
    bool IsPlayerControlled() const {
        return control_priority.load(std::memory_order_acquire) == ControlPriority::PLAYER_DIRECT;
    }
    
    /**
     * Check if this unit is AI-controlled
     */
    bool IsAIControlled() const {
        return control_priority.load(std::memory_order_acquire) == ControlPriority::AI_AUTO;
    }
    
    /**
     * Switch to player control
     * Called when player selects and issues commands to this unit
     */
    void TakeoverByPlayer(uint64_t current_time_ms) {
        control_priority.store(ControlPriority::PLAYER_DIRECT, std::memory_order_release);
        takeover_time_ms = current_time_ms;
        adaptation_time_remaining = 0.0f;  // No adaptation needed on takeover
    }
    
    /**
     * Return control to AI
     * Called when player deselects or issues autonomous command
     */
    void ReturnToAI() {
        control_priority.store(ControlPriority::AI_AUTO, std::memory_order_release);
        adaptation_time_remaining = ADAPTATION_DURATION;  // Start adaptation period
        last_input.clear();  // Clear stale player input
    }
    
    /**
     * Clear cached state (for reset or cleanup)
     */
    void ClearCache() {
        cached_node.reset();
        last_input.clear();
        cached_decision.reset();
        adaptation_time_remaining = 0.0f;
    }
};

// ============================================================================
// Global Control Manager Callback Interface
// ============================================================================

/**
 * IControlCallback
 * 
 * Interface for systems that need to react to control changes
 * (e.g., animation system, input system, UI system)
 */
class IControlCallback {
public:
    virtual ~IControlCallback() = default;
    
    /**
     * Called when control switches to player
     */
    virtual void OnPlayerTakeover(EntityId entity_id, uint64_t time_ms) = 0;
    
    /**
     * Called when control returns to AI
     */
    virtual void OnAIResume(EntityId entity_id) = 0;
};

// ============================================================================
// Input Buffer (for player command tracking)
// ============================================================================

/**
 * InputBuffer
 * Tracks raw player input for smoothing on AI resume
 */
struct InputBuffer {
    uint8_t button_flags = 0;       // Bitmask of pressed buttons
    float axis_x = 0.0f;            // Horizontal axis
    float axis_y = 0.0f;            // Vertical axis
    float axis_z = 0.0f;            // Vertical axis (for flight/elevation)
    uint64_t timestamp_ms = 0;      // When input was received

    bool HasInput() const {
        return button_flags != 0 || axis_x != 0.0f || axis_y != 0.0f || axis_z != 0.0f;
    }

    void Clear() {
        button_flags = 0;
        axis_x = axis_y = axis_z = 0.0f;
        timestamp_ms = 0;
    }
};

static_assert(std::is_trivially_copyable_v<InputBuffer>, "InputBuffer must be trivially copyable for lock-free control sync");
