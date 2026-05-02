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
#include <optional>
#include <memory>
#include <span>
#include "ai_core/dynamic_modifiers.h"
#include "ai_core/perception_component.h"

using EntityId = uint64_t;

// Forward declarations
class CollisionSystem;

// ============================================================================
// Decision Engine Core Types
// ============================================================================

/**
 * Execution result returned by a decision engine
 */
enum class DecisionResult {
    Success,  // Decision completed successfully
    Running,  // Decision not complete, will continue next frame
    Failure   // Decision failed
};

/**
 * Command type identifier
 */
enum class CommandType : uint32_t {
    MOVE = 0,           // Move to position
    ATTACK = 1,         // Attack target
    PATROL = 2,         // Patrol area
    SEEK_COVER = 3,     // Move to cover
    DEFEND = 4,         // Defend position
    IDLE = 5,           // Do nothing
    NUM_COMMANDS         // Total count
};

/**
 * Basic command structure
 */
struct Command {
    CommandType type = CommandType::IDLE;
    float priority = 0.5f;          // Priority for command arbitration
    uint32_t target_id = 0;         // Target entity ID (if relevant)
    float parameters[4] = {0, 0, 0, 0};  // Generic parameters (position, angle, etc.)
};

/**
 * Lightweight cache for decision results
 * Used to avoid recalculating decisions within the reaction lag period
 */
struct DecisionCache {
    std::vector<Command> last_commands;     // Last accepted commands
    uint64_t valid_until_frame = 0;         // Frame ID up to which cache is valid
    float confidence = 0.5f;                // Decision confidence (0..1)
};

/**
 * Snapshot for saving/restoring behavior tree state
 * Allows resuming decision-making from where it left off
 */
struct BTNodeSnapshot {
    uint32_t tree_id = 0;                              // Tree identifier
    uint32_t node_index = 0;                           // Node index in the tree array
    std::array<uint8_t, 64> blackboard;                // Fixed size serialized local variables
    uint8_t blackboard_size = 0;                       // Actual size used
    uint64_t timestamp = 0;                            // Timestamp of snapshot
};
static_assert(sizeof(BTNodeSnapshot) <= 256, "BTNodeSnapshot is too large");

/**
 * Unit Struct-of-Arrays reference
 * Provides efficient access to unit data for AI processing
 * 
 * Expanded in Phase 3 to include tactical/suppression fields.
 */
struct UnitSoA {
    // ========== Core Identity ==========
    EntityId entity_id = 0;
    std::span<EntityId> entity_ids;

    // ========== Phase 1: Movement and Control ==========
    std::span<float> positions_x, positions_y, positions_z;
    std::span<float> velocities_x, velocities_y, velocities_z;
    std::span<float> health;

    [[nodiscard]] size_t count() const noexcept {
        return positions_x.size();
    }

    [[nodiscard]] EntityId GetEntityId(size_t index) const noexcept {
        if (!entity_ids.empty()) {
            return entity_ids[index];
        }
        return entity_id + static_cast<EntityId>(index);
    }

    // ========== Phase 4: Micro-level State ==========
    std::span<float> fatigue;                    // 0..1
    std::span<float> stress;                     // 0..1
    std::span<float> morale;                     // 0..1
    std::span<uint8_t> stance;                   // Stance enum values stored as raw bytes
    std::span<float> stance_transition_progress; // Transition progress 0..1
    std::span<uint8_t> stance_transition_state;  // Stance transition state values

    // ========== Phase 5: Perception State ==========
    std::span<PerceptionState<16>> perception_states;  // Perception data for each unit

    // ========== Phase 3: Tactical State (Suppression & Formation) ==========
    // Note: These are span references to shared tactical arrays.
    // Each unit index maps to corresponding tactical state.
    
    // Suppression and combat effectiveness
    std::span<float> suppression_levels;  // 0..1 for each unit
    // TODO: std::span<float> accuracy_multipliers;  // 1.0 = base accuracy
    // TODO: std::span<bool> should_break_formation;  // Formation break flags
    // TODO: std::span<bool> refuses_to_move;  // Movement refusal flags
    
    // Formation positions and assignments
    // TODO: std::span<uint16_t> formation_slot_id;  // Assigned slot in formation
    // TODO: std::span<glm::vec3> target_formation_pos;  // Where unit should be
    
    // Communication state
    // TODO: std::span<uint32_t> last_order_id;  // Last received order ID
    // TODO: std::span<float> order_ready_time;  // When order becomes executable
};

/**
 * Request passed to decision engine Execute() method
 */
struct DecisionRequest {
    EntityId entity_id = 0;           // Entity making decision
    const UnitSoA* units = nullptr;   // Reference to global SoA slice
    const WorldState* world = nullptr; // Current world state
    SupplyStatus supply;              // Current supply status for the squad
    float delta_time = 0.0f;          // Time step this frame
    uint64_t frame_id = 0;            // Frame counter for determinism
};

// ============================================================================
// Core Decision Engine Interface
// ============================================================================

/**
 * IDecisionEngine
 * 
 * Universal interface for all AI decision-making backends:
 * - Behavior Trees
 * - Utility AI
 * - GOAP (Goal-Oriented Action Planning)
 * - Neural networks
 * 
 * Implementation ensures:
 * - Single frame execution under time budget
 * - State caching for resumption
 * - Smooth transitions between control contexts
 */
class IDecisionEngine {
public:
    virtual ~IDecisionEngine() = default;

    /**
     * Execute decision-making process for a unit
     * 
     * @param req DecisionRequest containing entity and context
     * @param out_commands Output command buffer (may be reused)
     * @return DecisionResult indicating execution status
     * 
     * Contract:
     * - Must complete in O(1) time budget
     * - Can return Running if decision will continue next frame
     * - Commands in out_commands are copied by caller
     */
    virtual DecisionResult Execute(DecisionRequest& req,
                                   std::vector<Command>& out_commands) = 0;

    /**
     * Reset internal state for an entity
     * Called when switching control contexts or rebuilding state
     * 
     * @param entity_id Entity to reset
     */
    virtual void ResetContext(EntityId entity_id) = 0;

    /**
     * Serialize current execution state to snapshot
     * Allows pausing and resuming mid-decision
     * 
     * @param entity_id Entity to snapshot
     * @return Optional snapshot, empty if no context to save
     */
    virtual std::optional<BTNodeSnapshot> SaveSnapshot(EntityId entity_id) const = 0;

    /**
     * Restore execution state from snapshot
     * Allows resuming decision-making from saved point
     * 
     * @param entity_id Entity to restore
     * @param snap Snapshot to restore from
     * @return true if restored successfully, false if snapshot invalid
     */
    virtual bool RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snap) = 0;

    /**
     * Validate a snapshot against current world state
     * Checks if cached decision is still valid
     * 
     * @param snap Snapshot to validate
     * @param world Current world state
     * @return true if snapshot conditions still hold
     */
    virtual bool ValidateSnapshot(const BTNodeSnapshot& snap,
                                  const WorldState& world) const = 0;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Smoothly blend/interpolate between two command sets
 * Used for smooth transitions between player and AI control
 * 
 * @param cmd1 First command set
 * @param cmd2 Second command set
 * @param blend_factor Blend parameter (0 = all cmd1, 1 = all cmd2)
 * @return Interpolated commands
 */
std::vector<Command> LerpCommands(const std::vector<Command>& cmd1,
                                  const std::vector<Command>& cmd2,
                                  float blend_factor);

/**
 * Create a null command (do-nothing command)
 */
inline Command MakeNullCommand() {
    return Command{CommandType::IDLE, 0.0f, 0, {0, 0, 0, 0}};
}
