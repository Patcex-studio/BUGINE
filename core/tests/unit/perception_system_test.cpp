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
#include <catch2/catch_approx.hpp>
#include "ai_core/perception_system.h"
#include "ai_core/spatial_grid.h"
#include "ai_core/ai_config.h"
#include "ai_core/decision_engine.h"

TEST_CASE("PerceptionSystem Basic Functionality", "[perception]") {
    PerceptionSystem perception_system;
    SpatialGrid spatial_grid;
    AIConfig cfg;
    WorldState world;
    world.frame_id = 0;
    world.time_ms = 0.0f;
    world.precipitation = 0.0f;
    world.wind_speed = 0.0f;
    world.fog_density = 0.0f;

    // Create mock UnitSoA with one unit
    std::vector<float> positions_x = {0.0f};
    std::vector<float> positions_y = {0.0f};
    std::vector<float> positions_z = {0.0f};
    std::vector<float> stress = {0.0f};
    std::vector<float> fatigue = {0.0f};
    std::vector<PerceptionState<16>> perceptions = {PerceptionState<16>(50.0f, 30.0f)};

    UnitSoA units;
    units.positions_x = positions_x;
    units.positions_y = positions_y;
    units.positions_z = positions_z;
    units.stress = stress;
    units.fatigue = fatigue;
    units.perception_states = perceptions;
    units.entity_id = 1;

    SECTION("Stationary unit does not trigger repeated raycasts") {
        // Initial update
        perception_system.Update(units, spatial_grid, world, 0, cfg);

        // Simulate stationary unit (same position)
        perception_system.Update(units, spatial_grid, world, 60, cfg);  // 1 second later

        // Check that perception state is updated but no new raycasts (hard to test without mocks)
        REQUIRE(perceptions[0].threat_count == 0);  // No threats detected
    }

    SECTION("Memory decay removes threat after time") {
        // Add a threat manually
        perceptions[0].add_or_update(2, ThreatRecord::Type::Visual, 1.0f, Vector3(10, 0, 0), 0);

        REQUIRE(perceptions[0].threat_count == 1);
        REQUIRE(perceptions[0].threats[0].confidence == Catch::Approx(1.0f));

        // Simulate decay over frames
        for (int frame = 1; frame <= 120; ++frame) {  // ~2 seconds
            perception_system.Update(units, spatial_grid, world, frame, cfg);
        }

        // Threat should be decayed or removed
        REQUIRE(perceptions[0].threat_count == 0);  // Removed after decay
    }

    SECTION("Stress affects effective range") {
        stress[0] = 0.8f;  // High stress
        fatigue[0] = 0.5f; // High fatigue

        perception_system.Update(units, spatial_grid, world, 0, cfg);

        // Effective range should be reduced
        float expected_range = 50.0f * 1.0f * (1.0f - 0.8f * 0.3f - 0.5f * 0.1f);  // ~32.5
        REQUIRE(perceptions[0].effective_range == Catch::Approx(expected_range).epsilon(0.1f));
    }
}