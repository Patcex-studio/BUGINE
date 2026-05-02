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
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "electrical_system/electrical_system.h"
#include "electrical_system/components.h"
#include <cmath>

using namespace electrical_system;
using Catch::Matchers::WithinAbs;

// Mock EntityManager for testing (minimal implementation)
namespace ecs {
class EntityManager {
public:
    virtual ~EntityManager() = default;
};
} // namespace ecs

// Test fixtures
struct ElectricalSystemTest {
    ElectricalSystemTest()
        : sys(512), em(std::make_unique<ecs::EntityManager>()) {
    }
    
    ElectricalSystem sys;
    std::unique_ptr<ecs::EntityManager> em;
};

TEST_CASE_METHOD(ElectricalSystemTest, "ElectricalGrid creation and initialization") {
    EntityID vehicle_id = 1;
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    
    sys.on_grid_created(vehicle_id, &grid);
    
    auto* retrieved_grid = sys.get_grid(vehicle_id);
    REQUIRE(retrieved_grid != nullptr);
    REQUIRE(retrieved_grid->parent_vehicle_id == vehicle_id);
    REQUIRE_THAT(retrieved_grid->battery_charge_w_h, 
                 WithinAbs(100.0f, 0.01f));
}

TEST_CASE_METHOD(ElectricalSystemTest, "Generator power generation") {
    EntityID vehicle_id = 1;
    EntityID gen_id = 100;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    GeneratorComponent gen(5000.0f, 0.95f);
    
    sys.on_grid_created(vehicle_id, &grid);
    sys.on_generator_created(gen_id, &gen, vehicle_id);
    sys.sync_generator_with_engine(vehicle_id, true);
    
    gen.is_running = true;
    gen.current_power_w = gen.max_power_w;
    
    REQUIRE_THAT(gen.get_effective_power(), 
                 WithinAbs(5000.0f * 0.95f, 1.0f));
}

TEST_CASE_METHOD(ElectricalSystemTest, "Consumer power consumption") {
    EntityID vehicle_id = 1;
    EntityID consumer_id = 200;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    ConsumerComponent consumer(1000.0f, 0.3f); // 1000W, priority 0.3 (critical)
    
    sys.on_grid_created(vehicle_id, &grid);
    sys.on_consumer_created(consumer_id, &consumer, vehicle_id);
    
    consumer.is_active.store(1, std::memory_order_release);
    consumer.is_damaged.store(0, std::memory_order_release);
    
    REQUIRE_THAT(consumer.get_consumption(), 
                 WithinAbs(1000.0f, 0.01f));
}

TEST_CASE_METHOD(ElectricalSystemTest, "Battery charging") {
    EntityID vehicle_id = 1;
    EntityID gen_id = 100;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    grid.battery_charge_w_h = 50.0f;
    grid.total_generated_power_w = 2000.0f; // Excess power
    grid.total_consumed_power_w = 1000.0f;
    
    sys.on_grid_created(vehicle_id, &grid);
    
    float dt = 1.0f / 60.0f; // Frame time
    sys.update(dt, em.get());
    
    // Battery should increase (1000W excess * (1/3600) * efficiency)
    REQUIRE(grid.battery_charge_w_h > 50.0f);
    REQUIRE(grid.battery_charge_w_h <= 100.0f);
}

TEST_CASE_METHOD(ElectricalSystemTest, "Battery discharging") {
    EntityID vehicle_id = 1;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    grid.battery_charge_w_h = 50.0f;
    grid.total_generated_power_w = 0.0f;
    grid.total_consumed_power_w = 1000.0f;
    
    sys.on_grid_created(vehicle_id, &grid);
    
    float dt = 1.0f / 60.0f;
    sys.update(dt, em.get());
    
    // Battery should decrease
    REQUIRE(grid.battery_charge_w_h < 50.0f);
    REQUIRE(grid.battery_charge_w_h >= 0.0f);
}

TEST_CASE_METHOD(ElectricalSystemTest, "Battery remains non-negative") {
    EntityID vehicle_id = 1;
    
    ElectricalGridComponent grid(vehicle_id, 10.0f);
    grid.battery_charge_w_h = 1.0f;
    grid.total_generated_power_w = 0.0f;
    grid.total_consumed_power_w = 5000.0f; // Large drain
    
    sys.on_grid_created(vehicle_id, &grid);
    
    for (int i = 0; i < 100; ++i) {
        sys.update(0.016f, em.get());
    }
    
    REQUIRE(grid.battery_charge_w_h >= 0.0f);
    REQUIRE(grid.battery_charge_w_h <= grid.max_battery_charge_w_h);
}

TEST_CASE_METHOD(ElectricalSystemTest, "Battery percentage calculation") {
    EntityID vehicle_id = 1;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    grid.battery_charge_w_h = 50.0f;
    
    sys.on_grid_created(vehicle_id, &grid);
    
    float pct = grid.get_battery_percentage();
    REQUIRE_THAT(pct, WithinAbs(0.5f, 0.01f));
}

TEST_CASE_METHOD(ElectricalSystemTest, "Overload detection") {
    EntityID vehicle_id = 1;
    
    ElectricalGridComponent grid(vehicle_id, 100.0f);
    sys.on_grid_created(vehicle_id, &grid);
    
    grid.total_generated_power_w = 1000.0f;
    grid.total_consumed_power_w = 2000.0f;
    
    REQUIRE(grid.check_overload());
    
    grid.total_consumed_power_w = 1000.0f;
    REQUIRE(!grid.check_overload());
}

TEST_CASE_METHOD(ElectricalSystemTest, "Battery quantization for network") {
    // Test 10-bit quantization
    float max_charge = 100.0f;
    
    float test_values[] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};
    
    for (float value : test_values) {
        uint16_t quantized = ElectricalSystem::quantize_battery(value, max_charge);
        float dequantized = ElectricalSystem::dequantize_battery(quantized, max_charge);
        
        // Quantization error should be < 0.1 Wh due to 10-bit resolution
        REQUIRE_THAT(dequantized, WithinAbs(value, 0.1f));
    }
}

TEST_CASE_METHOD(ElectricalSystemTest, "Deterministic RNG seeding") {
    // Same seed should produce same sequence
    DeterministicRNG rng1(DeterministicRNG::make_seed(42, 100));
    DeterministicRNG rng2(DeterministicRNG::make_seed(42, 100));
    
    for (int i = 0; i < 100; ++i) {
        REQUIRE(rng1.next() == rng2.next());
    }
}

TEST_CASE_METHOD(ElectricalSystemTest, "Deterministic RNG float range") {
    DeterministicRNG rng(12345);
    
    for (int i = 0; i < 100; ++i) {
        float val = rng.next_float_range(10.0f, 20.0f);
        REQUIRE(val >= 10.0f);
        REQUIRE(val <= 20.0f);
    }
}

TEST_CASE_METHOD(ElectricalSystemTest, "Consumer priority ordering") {
    ConsumerPriority p1{1, 100};
    ConsumerPriority p2{1, 50};
    ConsumerPriority p3{0, 200};
    
    // Tier 0 < tier 1
    REQUIRE(p3 < p1);
    
    // Same tier: lower entity_id < higher
    REQUIRE(p2 < p1);
}

TEST_CASE_METHOD(ElectricalSystemTest, "Network state serialization") {
    EntityID grid_id = 1;
    
    ElectricalGridComponent grid(grid_id, 100.0f);
    grid.battery_charge_w_h = 75.0f;
    grid.damage_flags = static_cast<uint32_t>(DamageFlag::BATTERY_DEAD);
    
    sys.on_grid_created(grid_id, &grid);
    
    auto state = sys.get_network_state(grid_id);
    REQUIRE(state.grid_id == grid_id);
    REQUIRE_THAT(ElectricalSystem::dequantize_battery(state.battery_charge_q, 100.0f),
                 WithinAbs(75.0f, 0.1f));
}

TEST_CASE_METHOD(ElectricalSystemTest, "Multiple vehicles isolation") {
    EntityID vehicle_1 = 1;
    EntityID vehicle_2 = 2;
    
    ElectricalGridComponent grid1(vehicle_1, 100.0f);
    ElectricalGridComponent grid2(vehicle_2, 50.0f);
    
    sys.on_grid_created(vehicle_1, &grid1);
    sys.on_grid_created(vehicle_2, &grid2);
    
    auto* retrieved_1 = sys.get_grid(vehicle_1);
    auto* retrieved_2 = sys.get_grid(vehicle_2);
    
    REQUIRE(retrieved_1 != retrieved_2);
    REQUIRE_THAT(retrieved_1->max_battery_charge_w_h, WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(retrieved_2->max_battery_charge_w_h, WithinAbs(50.0f, 0.01f));
}
