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

class BallisticsSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test projectile (APFSDS like M829)
        test_projectile = {
            ProjectileType::APFSDS,
            120.0f,     // 120mm caliber
            4.3f,       // 4.3kg mass
            1650.0f,    // 1650 m/s muzzle velocity
            0.15f,      // drag coefficient
            0.0f,       // no explosive
            500.0f,     // 500mm penetrator length
            0.0f,       // no shaped charge
            0.0f,       // no fuse
            false,      // no guidance
            0.0f        // no guidance accuracy
        };

        // Setup test component (modern tank armor)
        test_component = {
            1,          // component_id
            1,          // parent_vehicle_id
            ComponentType::HULL,
            1000.0f,    // max health
            1000.0f,    // current health
            600.0f,     // 600mm armor
            60.0f,      // 60 degree slope
            ArmorType::COMPOSITE,
            0,          // no special vulnerabilities
            _mm256_setzero_ps(), // local transform
            _mm256_setzero_ps(), // aabb min
            _mm256_setzero_ps()  // aabb max
        };

        // Setup test impact
        test_impact = {
            30.0f,      // 30 degree impact angle
            1500.0f,    // 1500 m/s impact velocity
            2000.0f,    // 2km distance traveled
            0.0f,       // no standoff
            _mm_setzero_ps(), // impact position
            _mm_setzero_ps()  // impact normal
        };
    }

    ProjectileProperties test_projectile;
    VehicleComponent test_component;
    ImpactData test_impact;
};

TEST_F(BallisticsSystemTest, APFSDSPenetrationCalculation) {
    auto result = BallisticsSystem::calculate_penetration(
        test_projectile, test_component, test_impact
    );

    // Should penetrate modern composite armor at 30 degrees
    EXPECT_TRUE(result.penetrated);
    EXPECT_GT(result.penetration_depth, 500.0f);
    EXPECT_LT(result.ricochet_probability, 0.1f);
}

TEST_F(BallisticsSystemTest, RicochetAtHighAngle) {
    // High angle impact
    ImpactData high_angle_impact = test_impact;
    high_angle_impact.impact_angle = 75.0f;

    auto result = BallisticsSystem::calculate_penetration(
        test_projectile, test_component, high_angle_impact
    );

    // Should have high ricochet probability
    EXPECT_GT(result.ricochet_probability, 0.7f);
}

TEST_F(BallisticsSystemTest, HEATPenetration) {
    // Test HEAT projectile
    ProjectileProperties heat_projectile = {
        ProjectileType::HEAT,
        125.0f,     // 125mm
        19.0f,      // 19kg
        905.0f,     // 905 m/s
        0.35f,
        1.8f,       // 1.8kg explosive
        0.0f,
        125.0f,     // 125mm shaped charge
        0.0f,
        false,
        0.0f
    };

    ImpactData heat_impact = test_impact;
    heat_impact.standoff_distance = 0.5f; // 0.5m standoff

    auto result = BallisticsSystem::calculate_penetration(
        heat_projectile, test_component, heat_impact
    );

    // HEAT should penetrate regardless of angle
    EXPECT_TRUE(result.penetrated);
}

TEST_F(BallisticsSystemTest, BatchProcessing) {
    std::vector<ProjectileProperties> projectiles = {test_projectile, test_projectile};
    std::vector<VehicleComponent> components = {test_component, test_component};
    std::vector<ImpactData> impacts = {test_impact, test_impact};
    std::vector<PenetrationResult> results;

    BallisticsSystem::calculate_penetration_batch(
        projectiles, components, impacts, results
    );

    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].penetrated);
    EXPECT_TRUE(results[1].penetrated);
}

TEST_F(BallisticsSystemTest, PerformanceComparison) {
    // Run the performance comparison test
    BallisticsSystem::run_performance_comparison_test();
    
    // Test passes if no exceptions are thrown
    SUCCEED();
}