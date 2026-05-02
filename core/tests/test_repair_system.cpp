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
#include <gtest/gtest.h>
#include "repair_system.h"
#include "damage_system.h"

using namespace physics_core;

class RepairSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test repair unit
        repair_unit = {
            100,        // repair_unit_entity
            {ENGINE, TRACK, TURRET, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // specialties
            1.0f,       // repair_speed_multiplier
            3,          // max_simultaneous_repairs
            {},         // available_tools
            {},         // available_parts
            0.8f,       // mobility_factor
            true,       // has_heavy_equipment
            true,       // has_specialized_tools
            0.9f        // skill_level
        };

        // Setup test component damage state
        damage_state = {
            1,          // component_entity
            500.0f,     // current_health
            500.0f,     // max_health
            0,          // damage_type_flags
            1.0f,       // structural_integrity
            50.0f,      // armor_thickness
            1.0f,       // armor_hardness
            0,          // damage_level
            0.0f,       // cumulative_stress
            0,          // status_flags
            0.0f,       // last_damage_time
            0.0f,       // fire_start_time
            _mm256_setzero_ps(), // damage_position
            _mm256_setzero_ps(), // damage_normal
            0.0f,       // total_damage_received
            0,          // hit_count
            0.0f,       // reactive_tile_coverage
            false,      // reactive_tiles_triggered
            {0, 0}      // padding
        };

        // Setup test repair operation
        repair_operation = {
            1,          // operation_id
            1,          // vehicle_entity
            1,          // component_entity
            100,        // repair_unit_entity
            COMPONENT_REPAIR, // operation_type
            5,          // priority_level
            {},         // required_parts
            {},         // required_tools
            30.0f,      // estimated_time_minutes
            1800.0f,    // actual_time_remaining
            0.0f,       // progress_percentage
            PENDING,    // status
            1000.0f,    // cost_estimate
            0.0f,       // actual_cost
            _mm256_setzero_ps(), // repair_location
            0.8f,       // security_level
            1.0f        // weather_factor
        };
    }

    RepairUnitCapabilities repair_unit;
    ComponentDamageState damage_state;
    RepairOperation repair_operation;
    RepairSystem repair_system;
    LogisticsSystem logistics_system;
    RepairScheduler repair_scheduler;
};

// Test basic repair processing
TEST_F(RepairSystemTest, ProcessFieldRepairs) {
    // Start a repair operation
    ASSERT_TRUE(repair_system.start_repair_operation(repair_operation));
    
    // Process repairs for 10 seconds
    repair_system.process_field_repairs(10.0f);
    
    // Check that repair progress has increased
    auto* repair_state = repair_system.get_component_repair_state(1);
    ASSERT_NE(repair_state, nullptr);
    EXPECT_GT(repair_state->repair_progress, 0.0f);
    EXPECT_LT(repair_state->time_remaining_seconds, 1800.0f);
}

// Test repair-damage synchronization
TEST_F(RepairSystemTest, SyncRepairToDamageState) {
    ComponentRepairState repair_state = {};
    repair_state.repair_progress = 0.5f; // 50% repaired
    
    // Initially damage the component
    damage_state.current_health = 250.0f; // Half health
    
    repair_system.sync_repair_to_damage_state(1, repair_state, damage_state);
    
    // Health should be restored
    EXPECT_GT(damage_state.current_health, 250.0f);
}

// Test logistics part request
TEST_F(RepairSystemTest, RequestPartsFromLogistics) {
    std::vector<RequiredPart> needed_parts = {
        {ENGINE_PART, 2},
        {TRACK_PART, 4}
    };
    
    // Request parts
    bool available = logistics_system.request_parts_from_logistics(needed_parts, 1, 0.8f);
    
    // Should generate orders if not available
    EXPECT_FALSE(available); // Parts not available initially
}

// Test repair assignment
TEST_F(RepairSystemTest, AssignRepairsToUnits) {
    std::vector<RepairOperation> operations = {repair_operation};
    std::vector<RepairUnitCapabilities> units = {repair_unit};
    
    repair_scheduler.assign_repairs_to_units(operations, units);
    
    // Should assign the operation to the unit
    // TODO: Check assignments
}

// Performance test for repair processing
TEST_F(RepairSystemTest, PerformanceFieldRepairs) {
    // Create multiple repair operations
    for (int i = 0; i < 100; ++i) {
        RepairOperation op = repair_operation;
        op.operation_id = i + 1;
        op.component_entity = i + 1;
        repair_system.start_repair_operation(op);
    }
    
    // Measure time for processing
    auto start = std::chrono::high_resolution_clock::now();
    repair_system.process_field_repairs(1.0f);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 5); // Should be < 5ms for 100 repairs
}

// Test memory usage
TEST_F(RepairSystemTest, MemoryUsage) {
    // Check that ComponentRepairState is <= 256 bytes
    EXPECT_LE(sizeof(ComponentRepairState), 256u);
    
    // Check that VehicleRepairState has reasonable size
    EXPECT_LT(sizeof(VehicleRepairState), 1024u);
}