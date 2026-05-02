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

#include <vector>
#include <cstdint>
#include <immintrin.h>
#include <array>
#include <queue>
#include "types.h"
#include "damage_system.h"

namespace physics_core {

// ============================================================================
// REPAIR SYSTEM STRUCTURES
// ============================================================================

struct RequiredPart {
    uint32_t part_id;
    uint32_t quantity_needed;
};

struct RequiredTool {
    uint32_t tool_id;
    uint32_t quantity_needed;
};

struct PartConsumption {
    uint32_t part_id;
    uint32_t quantity_consumed;
    float consumption_time;
};

struct ToolInventory {
    uint32_t tool_id;
    uint32_t quantity_available;
    float condition_factor;
};

struct PartInventory {
    uint32_t part_id;
    uint32_t quantity_available;
    uint32_t quantity_reserved;
    uint32_t quantity_in_transit;
    uint32_t maximum_capacity;
    float degradation_factor;
    float last_inspection_time;
    uint32_t condition_rating;
    EntityID storage_location;
    EntityID owner_entity;
};

struct SupplyRoute {
    EntityID source_node;
    EntityID destination_node;
    float transportation_time_hours;
    float transportation_capacity;
    float reliability_factor;
    float security_factor;
    uint32_t transport_method;
};

struct ResupplyOrder {
    uint32_t order_id;
    uint32_t part_id;
    uint32_t quantity_requested;
    EntityID requesting_node;
    float urgency_level;
};

struct TransportRequest {
    uint32_t request_id;
    std::vector<PartInventory> parts_to_transport;
    EntityID source_node;
    EntityID destination_node;
    float priority_level;
    float time_remaining_seconds;
};

struct RepairAssignment {
    EntityID repair_unit_entity;
    uint32_t operation_id;
    uint32_t operation_type;
    float assignment_efficiency;
};

// Component repair state (target: < 256 bytes per repair state)
struct ComponentRepairState {
    EntityID component_entity;       // Component being repaired
    EntityID repair_unit_entity;    // Repair unit performing the repair
    uint32_t operation_id;          // Unique operation ID
    float repair_progress;          // Repair progress (0.0-1.0)
    float repair_rate_per_second;   // Repair rate (health/sec)
    std::vector<RequiredPart> required_parts; // Required spare parts with quantities
    std::vector<RequiredTool> required_tools; // Required tools with quantities
    float time_remaining_seconds;   // Time remaining for completion
    uint32_t repair_priority;       // Priority level (1-10)
    bool is_under_repair;          // Currently being repaired
    bool has_required_parts;       // Has all required parts
    bool has_required_tools;       // Has all required tools
    bool is_interrupted;           // Repair interrupted (needs restart)
    float interruption_penalty;     // Penalty for interruptions
    __m256 repair_location;       // Location of repair
    // Padding to reach exactly 256 bytes
    uint32_t _padding[5];
};

// Vehicle repair state
struct VehicleRepairState {
    EntityID vehicle_entity;        // Vehicle being repaired
    std::vector<ComponentRepairState> component_repairs; // Active repairs
    EntityID repair_station_entity; // Repair station (mobile/fixed)
    float overall_repair_efficiency; // Overall efficiency factor
    float time_to_completion_minutes; // Estimated time to full repair
    bool is_repairable;            // Can be repaired at all
    bool is_repairing;             // Currently undergoing repairs
    bool needs_evacuation;         // Requires evacuation to repair facility
    float repair_cost_factor;      // Cost multiplier for repairs
    std::vector<PartConsumption> consumed_parts; // Consumed during repair
};

// Repair unit capabilities
struct RepairUnitCapabilities {
    EntityID repair_unit_entity;    // Repair unit entity
    uint32_t repair_specialties[16]; // Component types specialized in
    float repair_speed_multiplier;  // Speed multiplier for this unit
    uint32_t max_simultaneous_repairs; // Max concurrent repairs
    std::vector<ToolInventory> available_tools; // Available tools
    std::vector<PartInventory> available_parts; // Available spare parts
    float mobility_factor;         // Mobile vs stationary repair efficiency
    bool has_heavy_equipment;      // Heavy lifting equipment
    bool has_specialized_tools;    // Specialized repair tools
    float skill_level;             // Repair skill level (0.0-1.0)
};

// ============================================================================
// LOGISTICS SYSTEM STRUCTURES
// ============================================================================

// Part catalog definition
struct SparePartDefinition {
    uint32_t part_id;              // Unique part identifier
    char part_name[64];            // Human-readable name
    uint32_t part_category;        // ENGINE, TRACK, TURRET, WEAPON, etc.
    uint32_t vehicle_compatibility_mask; // Compatible vehicle types
    float mass_kg;                 // Part mass
    float volume_cubic_meters;     // Part volume
    float cost_usd;                // Part cost
    float repair_time_hours;       // Typical repair time with this part
    float durability_factor;       // Durability multiplier when installed
    bool is_consumable;            // Single-use vs reusable
    bool requires_special_handling; // Special handling requirements
    uint32_t storage_requirements; // Temperature, humidity, etc.
    uint32_t shelf_life_months;    // Shelf life before degradation
};

// Supply chain logistics
struct SupplyChainNode {
    EntityID node_entity;           // Supply node entity
    std::vector<PartInventory> inventory; // Node inventory
    std::vector<SupplyRoute> supply_routes; // Routes to other nodes
    uint32_t node_type;            // FIELD_BASE, MOBILE_UNIT, MAIN_DEPOT
    float capacity_utilization;     // Current capacity usage
    float resupply_frequency;       // Resupply frequency (per day)
    std::vector<ResupplyOrder> pending_orders; // Pending orders
    std::vector<TransportRequest> transport_requests; // Transport needs
};


// ============================================================================
// REPAIR OPERATION MANAGEMENT
// ============================================================================

enum class RepairOperationStatus : uint32_t {
    PENDING = 0,
    IN_PROGRESS = 1,
    COMPLETED = 2,
    FAILED = 3
};

// Repair operation definition
struct RepairOperation {
    uint32_t operation_id;          // Unique operation identifier
    EntityID vehicle_entity;        // Vehicle to repair
    EntityID component_entity;      // Component to repair
    EntityID repair_unit_entity;    // Assigned repair unit
    uint32_t operation_type;        // COMPONENT_REPAIR, OVERHAUL, etc.
    uint32_t priority_level;        // Priority (1-10)
    std::vector<RequiredPart> required_parts; // Needed parts
    std::vector<RequiredTool> required_tools; // Needed tools
    float estimated_time_minutes;   // Estimated completion time
    float actual_time_remaining;    // Time remaining
    float progress_percentage;      // Current progress
    uint32_t status;               // PENDING, IN_PROGRESS, COMPLETED, FAILED
    float cost_estimate;           // Estimated cost
    float actual_cost;             // Actual cost incurred
    __m256 repair_location;       // Location of repair
    float security_level;          // Security at repair location
    float weather_factor;          // Weather impact on repair
};

// Comparators
struct RepairPriorityComparator {
    bool operator()(const RepairOperation& a, const RepairOperation& b) const {
        return a.priority_level < b.priority_level;
    }
};

// Repair operation queue
struct RepairOperationQueue {
    std::vector<RepairOperation> pending_operations; // Queued operations
    std::vector<RepairOperation> active_operations;  // Currently executing
    std::vector<RepairOperation> completed_operations; // Recently completed
    std::vector<RepairOperation> failed_operations;   // Failed operations
    
    // Priority management
    std::priority_queue<RepairOperation, std::vector<RepairOperation>, 
                       RepairPriorityComparator> priority_queue;
    
    // Load balancing
    std::vector<RepairAssignment> repair_assignments; // Unit-to-operation assignments
    float current_workload_percentage; // Overall workload
    uint32_t active_repair_units;     // Number of active repair units
    uint32_t available_repair_units;  // Number of available repair units
};

// ============================================================================
// REPAIR SYSTEM CLASS
// ============================================================================

class LogisticsSystem;

class RepairSystem {
public:
    // Constructor
    RepairSystem(LogisticsSystem* logistics = nullptr) : logistics_system_(logistics) {}
    
    // Core processing functions
    void process_field_repairs(float delta_time);
    
    // Integration functions
    void sync_repair_to_damage_state(EntityID component_entity,
                                   const ComponentRepairState& repair_state,
                                   ComponentDamageState& damage_state);
    
    // Repair management
    bool start_repair_operation(const RepairOperation& operation);
    void interrupt_repair_operation(uint32_t operation_id);
    void complete_repair_operation(uint32_t operation_id);
    
    // State management
    ComponentRepairState* get_component_repair_state(EntityID component_entity);
    VehicleRepairState* get_vehicle_repair_state(EntityID vehicle_entity);
    
    // Data access
    const std::vector<ComponentRepairState>& get_active_repairs() const;
    const std::vector<VehicleRepairState>& get_vehicle_repair_states() const;
    
private:
    std::vector<ComponentRepairState> active_repairs_;
    std::vector<VehicleRepairState> vehicle_repair_states_;
    std::vector<ComponentDamageState> damage_states_;
    RepairOperationQueue operation_queue_;
    LogisticsSystem* logistics_system_;

    ComponentDamageState* get_component_damage_state(EntityID component_entity);
};

// ============================================================================
// LOGISTICS SYSTEM CLASS
// ============================================================================

class LogisticsSystem {
public:
    // Core processing functions
    void manage_supply_chain(float delta_time);
    
    // Part management
    bool request_parts_from_logistics(const std::vector<RequiredPart>& needed_parts,
                                    EntityID requesting_location,
                                    float urgency_level);
    
    // Inventory operations
    bool reserve_parts(uint32_t part_id, uint32_t quantity, EntityID location);
    bool consume_parts(uint32_t part_id, uint32_t quantity, EntityID location);
    uint32_t check_part_availability(uint32_t part_id, EntityID location);
    
    // Supply chain operations
    void generate_resupply_orders();
    void execute_transport_operations(float delta_time);
    
    // Data access
    const std::vector<SparePartDefinition>& get_part_catalog() const;
    const std::vector<SupplyChainNode>& get_supply_nodes() const;
    
private:
    std::vector<SparePartDefinition> part_catalog_;
    std::vector<SupplyChainNode> supply_nodes_;
    std::vector<SupplyRoute> supply_routes_;
    std::vector<ResupplyOrder> pending_orders_;
    std::vector<TransportRequest> transport_requests_;
};

// ============================================================================
// REPAIR SCHEDULER CLASS
// ============================================================================

class RepairScheduler {
public:
    // Assignment functions
    void assign_repairs_to_units(const std::vector<RepairOperation>& available_operations,
                               const std::vector<RepairUnitCapabilities>& available_units);
    
    // Scheduling functions
    void schedule_repair_operations(RepairOperationQueue& queue);
    void balance_repair_workload(const std::vector<RepairUnitCapabilities>& units);
    
    // Priority management
    void update_repair_priorities(const std::vector<EntityID>& critical_vehicles);
    
private:
    std::vector<RepairAssignment> current_assignments_;
};

}  // namespace physics_core