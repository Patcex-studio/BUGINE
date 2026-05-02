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
#include "physics_core/damage_system.h"
#include "physics_core/vehicle_component.h"

using namespace physics_core;

class DamageSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test components for a tank
        engine_component = {
            1,          // component_id
            1,          // parent_vehicle_id
            ComponentType::ENGINE,
            500.0f,     // max health
            500.0f,     // current health
            50.0f,      // thin armor
            0.0f,       // no slope
            ArmorType::RHA,
            CRITICAL | FLAMMABLE, // Critical and flammable
            _mm256_setzero_ps(),
            _mm256_setzero_ps(),
            _mm256_setzero_ps()
        };

        turret_component = {
            2,
            1,
            ComponentType::TURRET,
            800.0f,
            800.0f,
            300.0f,
            30.0f,
            ArmorType::COMPOSITE,
            FIREPOWER,  // Affects firepower
            _mm256_setzero_ps(),
            _mm256_setzero_ps(),
            _mm256_setzero_ps()
        };

        hull_component = {
            3,
            1,
            ComponentType::HULL,
            1500.0f,
            1500.0f,
            600.0f,
            60.0f,
            ArmorType::COMPOSITE,
            0,          // No special flags
            _mm256_setzero_ps(),
            _mm256_setzero_ps(),
            _mm256_setzero_ps()
        };

        // Setup test hit result
        test_hit = {
            {true, 550.0f, 0.2f, 0.0f, 50.0f, 35.0f}, // Penetrated with spalling
            3,          // hit hull
            300.0f,     // 300 damage to component
            {1, 2},     // affected components (engine, turret)
            _mm_setr_ps(1000.0f, 0.0f, 0.0f, 0.0f) // impulse vector
        };
    }

    VehicleComponent engine_component;
    VehicleComponent turret_component;
    VehicleComponent hull_component;
    HitResult test_hit;
};

TEST_F(DamageSystemTest, DamagePropagation) {
    std::vector<DamageEvent> events;

    DamageSystem::propagate_damage(1, test_hit, events);

    // Should generate multiple damage events
    EXPECT_GT(events.size(), 1);

    // Primary damage event
    EXPECT_EQ(events[0].component_id, test_hit.hit_component_id);
    EXPECT_EQ(events[0].damage_amount, test_hit.damage_to_component);
}

TEST_F(DamageSystemTest, ComponentHealthUpdate) {
    float initial_health = turret_component.health_current;

    DamageSystem::update_component_health(turret_component, 200.0f, 0); // kinetic damage

    EXPECT_EQ(turret_component.health_current, initial_health - 200.0f);
}

TEST_F(DamageSystemTest, CriticalHitDetection) {
    // Heavy damage to engine
    HitResult critical_hit = test_hit;
    critical_hit.hit_component_id = 1; // engine
    critical_hit.damage_to_component = 300.0f; // > 50% of 500

    EXPECT_TRUE(DamageSystem::is_critical_hit(engine_component, critical_hit));
}

TEST_F(DamageSystemTest, VehicleStatusCalculation) {
    std::vector<VehicleComponent> components = {engine_component, turret_component, hull_component};

    float mobility, firepower, survivability;

    DamageSystem::calculate_vehicle_status(1, components, mobility, firepower, survivability);

    // All components at full health
    EXPECT_FLOAT_EQ(mobility, 1.0f);
    EXPECT_FLOAT_EQ(firepower, 1.0f);
    EXPECT_FLOAT_EQ(survivability, 1.0f);

    // Damage engine
    components[0].health_current = 100.0f; // 20% health

    DamageSystem::calculate_vehicle_status(1, components, mobility, firepower, survivability);

    EXPECT_LT(mobility, 1.0f); // Mobility reduced
    EXPECT_FLOAT_EQ(firepower, 1.0f); // Turret still full
    EXPECT_FLOAT_EQ(survivability, 1.0f); // Hull still full
}

TEST_F(DamageSystemTest, DamageTypeMultipliers) {
    VehicleComponent ammo_rack = engine_component;
    ammo_rack.component_type = ComponentType::AMMO_RACK;
    ammo_rack.vulnerability_flags = EXPLOSIVE;

    float kinetic_damage = 100.0f;
    float explosive_damage = 100.0f;

    // Kinetic damage normal
    DamageSystem::update_component_health(ammo_rack, kinetic_damage, 0);
    float health_after_kinetic = ammo_rack.health_current;

    // Reset health
    ammo_rack.health_current = ammo_rack.health_max;

    // Explosive damage doubled for explosive components
    DamageSystem::update_component_health(ammo_rack, explosive_damage, 1);
    float health_after_explosive = ammo_rack.health_current;

    EXPECT_LT(health_after_explosive, health_after_kinetic);
}