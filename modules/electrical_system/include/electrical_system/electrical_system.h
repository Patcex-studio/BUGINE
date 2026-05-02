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

#include "components.h"
#include "electrical_types.h"
#include <array>
#include <vector>
#include <span>
#include <cstdint>
#include <unordered_map>

// Forward declaration for ECS integration
namespace ecs {
class EntityManager;
class SystemBase;
} // namespace ecs

namespace electrical_system {

/// Main electrical system - manages power generation, distribution, and damage
class ElectricalSystem {
public:
    explicit ElectricalSystem(size_t max_vehicles = 512);
    ~ElectricalSystem() = default;
    
    // Core update - called once per simulation frame
    void update(float dt, ecs::EntityManager* em);
    
    // Component lifecycle
    void on_grid_created(EntityID vehicle_id, ElectricalGridComponent* grid);
    void on_grid_destroyed(EntityID vehicle_id);
    void on_generator_created(EntityID entity_id, GeneratorComponent* gen, EntityID parent_vehicle);
    void on_generator_destroyed(EntityID entity_id);
    void on_consumer_created(EntityID entity_id, ConsumerComponent* consumer, EntityID parent_vehicle);
    void on_consumer_destroyed(EntityID entity_id);
    
    // Query API
    ElectricalGridComponent* get_grid(EntityID vehicle_id);
    const ElectricalGridComponent* get_grid(EntityID vehicle_id) const;
    
    // Debug/test hooks
    #ifdef ENABLE_ELECTRICAL_DEBUG
    void debug_inject_overload(EntityID grid_id, float excess_power);
    void debug_inject_short_circuit(EntityID grid_id);
    void debug_reset_grid(EntityID grid_id);
    #endif
    
    // Network support
    NetworkElectricalState get_network_state(EntityID grid_id) const;
    void apply_network_state(EntityID grid_id, const NetworkElectricalState& state);
    
    // Engine integration - synchronize generator with engine state
    void sync_generator_with_engine(EntityID vehicle_id, bool engine_running);
    
private:
    // Configuration constants
    static constexpr size_t MAX_CONSUMERS_PER_VEHICLE = 32;
    static constexpr size_t MAX_GENERATORS_PER_VEHICLE = 4;
    static constexpr size_t TEMP_BUFFER_SIZE = 256; // For scratch space during updates
    static constexpr float BATTERY_DEAD_THRESHOLD = 2.0f; // Seconds to confirm battery dead
    static constexpr float SHORT_CIRCUIT_DAMAGE = 0.5f; // Health penalty on neighbors
    static constexpr float FIRE_START_PROBABILITY = 0.1f; // Chance fire starts on short circuit
    static constexpr float CASCADE_RADIUS_METERS = 2.0f; // Cascade damage radius
    
    // Tracking data
    struct VehicleElectricalData {
        ElectricalGridComponent* grid = nullptr;
        std::vector<GeneratorComponent*> generators;
        std::vector<ConsumerComponent*> consumers;
        std::vector<EntityID> consumer_entity_ids; // Parallel to consumers vector
    };
    
    std::vector<VehicleElectricalData> vehicles_;
    std::unordered_map<EntityID, size_t> vehicle_index_map_;
    std::unordered_map<EntityID, EntityID> generator_to_vehicle_;
    std::unordered_map<EntityID, EntityID> consumer_to_vehicle_;
    
    // Temporary buffers for this frame (SoA layout for cache locality)
    std::array<ConsumerEntry, TEMP_BUFFER_SIZE> temp_consumer_buffer_;
    std::array<ConsumerPriority, MAX_CONSUMERS_PER_VEHICLE> priority_buffer_;
    
    uint64_t frame_id_ = 0;
    ecs::EntityManager* entity_manager_ = nullptr;
    
    // === Private implementation methods ===
    
    /// Gather all active consumers for a vehicle
    size_t gather_consumers(VehicleElectricalData& vdata, 
                           std::span<ConsumerEntry> buffer);
    
    /// Accumulate power from all generators
    void accumulate_generators(VehicleElectricalData& vdata);
    
    /// Main power distribution algorithm
    void distribute_power(VehicleElectricalData& vdata,
                        std::span<ConsumerEntry> consumers,
                        float dt);
    
    /// Handle overload condition (deterministic consumer disconnect)
    void handle_overload(ElectricalGridComponent& grid,
                        std::span<ConsumerEntry> consumers,
                        float deficit);
    
    /// Update battery charge/discharge with numerical stability
    void update_battery(ElectricalGridComponent& grid,
                       float generated,
                       float consumed,
                       float dt);
    
    /// Process battery critical state and emergency shutdown
    void process_battery_critical(ElectricalGridComponent& grid,
                                 VehicleElectricalData& vdata,
                                 float dt);
    
    /// Handle short circuit cascade effects
    void handle_short_circuit(EntityID grid_id,
                            const ElectricalGridComponent& grid);
    
    /// Deterministic sorting with stability
    void sort_consumers_by_priority(std::span<ConsumerEntry> consumers);
    
    /// Quantize battery charge for network transmission
    static uint16_t quantize_battery(float charge, float max_charge);
    static float dequantize_battery(uint16_t quantized, float max_charge);
    
    /// Get damage flags as enum
    static DamageFlag get_damage_flag(uint32_t flags, DamageFlag flag) {
        return static_cast<DamageFlag>(flags & static_cast<uint32_t>(flag));
    }
    
    /// Set damage flag
    static void set_damage_flag(uint32_t& flags, DamageFlag flag, bool set) {
        if (set) {
            flags |= static_cast<uint32_t>(flag);
        } else {
            flags &= ~static_cast<uint32_t>(flag);
        }
    }
};

} // namespace electrical_system
