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
#include "ballistics_system.h"
#include "projectile_properties.h"
#include "vehicle_component.h"
#include "armor_materials.h"

using namespace physics_core;

class BallisticsFormulasTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test projectile
        projectile = {
            ProjectileType::APFSDS,
            120.0f,     // mm
            4.3f,       // kg
            1650.0f,    // m/s
            0.15f,      // drag coefficient
            0.0f,       // no explosive
            500.0f,     // mm
            0.0f,       // no shaped charge
            0.0f,       // no fuse
            false,      // no guidance
            0.0f        // no guidance accuracy
        };

        // Setup test component
        component = {
            1,          // component_id
            1,          // parent_vehicle_id
            ComponentType::HULL,
            1000.0f,    // max health
            100.0f,     // current health
            50.0f,      // thickness mm
            ArmorType::ROLLED_HOMOGENEOUS,
            0.0f,       // angle
            {0.0f, 0.0f, 0.0f}, // position
            {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f} // rotation
        };

        impact = {
            0.0f,       // impact_angle degrees
            1650.0f,    // impact_velocity m/s
            100.0f,     // distance_traveled m
            0.0f,       // standoff_distance mm
            _mm_setr_ps(0.0f, 0.0f, 0.0f, 0.0f), // impact_position
            _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f), // impact_normal
        };
    }

    ProjectileProperties projectile;
    VehicleComponent component;
    ImpactData impact;
};

// Test penetration calculation correctness
TEST_F(BallisticsFormulasTest, PenetrationCalculation) {
    auto result = BallisticsSystem::calculate_penetration(projectile, component, impact);

    // For APFSDS at 1650 m/s against 50mm RHA at 0 degrees, should penetrate
    EXPECT_TRUE(result.penetrated);
    EXPECT_GT(result.penetration_depth, 50.0f); // mm
    EXPECT_GT(result.residual_energy, 0.0f); // joules
}

// Test ricochet probability
TEST_F(BallisticsFormulasTest, RicochetProbability) {
    // High angle impact
    impact.impact_angle = 70.0f; // degrees
    auto result = BallisticsSystem::calculate_penetration(projectile, component, impact);

    // High ricochet probability at steep angles
    EXPECT_GT(result.ricochet_probability, 0.5f);
}

// Test energy conservation
TEST_F(BallisticsFormulasTest, EnergyConservation) {
    auto result = BallisticsSystem::calculate_penetration(projectile, component, impact);

    // Initial kinetic energy
    float initial_energy = 0.5f * projectile.mass * projectile.muzzle_velocity * projectile.muzzle_velocity;

    // Energy should be conserved (some absorbed by armor)
    float total_energy = result.residual_energy + (result.spall_damage * 1000.0f); // approximate
    EXPECT_LE(total_energy, initial_energy * 1.01f); // Allow small numerical error
}