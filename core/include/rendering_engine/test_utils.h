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

#include "core/include/rendering_engine/types.h"
#include "core/include/rendering_engine/model_system.h"
#include <glm/glm.hpp>
#include <vector>

namespace rendering_engine {
namespace test_utils {

/**
 * Create a simple tank blueprint made from cubes
 * @return Tank blueprint ready for generation
 */
inline VehicleBlueprint CreateSimpleTankBlueprint() {
    VehicleBlueprint blueprint;
    blueprint.vehicle_name = "TestTank";
    blueprint.vehicle_type = 0; // TANK
    blueprint.mass_total = 1000.0f;

    // HULL COMPONENT (main body - large cube)
    {
        ComponentBlueprint hull;
        hull.component_type = 0; // HULL
        // Position: (0,0,0), Scale: (2.0, 1.0, 3.0) for a rectangular hull
        // Using SIMD transform format: _mm_setr_ps(x,y,z,scale,...)
        // Actually for simplicity, just create identity with scale 2.0
        hull.local_transform = _mm_setr_ps(0, 0, 0, 2.0f, 0, 0, 0, 0);
        hull.attachment_points[0] = 0; // Socket 0 for turret
        hull.health_max = 100.0f;
        hull.armor_thickness = 50.0f;
        hull.is_destructible = true;
        blueprint.components.push_back(hull);
    }

    // TURRET COMPONENT (on top of hull)
    {
        ComponentBlueprint turret;
        turret.component_type = 1; // TURRET
        turret.local_transform = _mm_setr_ps(0, 0.6f, 0, 1.0f, 0, 0, 0, 0);
        turret.attachment_points[0] = 0; // Can attach weapons here
        turret.health_max = 80.0f;
        turret.armor_thickness = 60.0f;
        turret.is_destructible = true;
        blueprint.components.push_back(turret);
    }

    // Connection: hull socket 0 -> turret socket 0
    SocketConnection conn;
    conn.parent_socket_id = 0;       // Hull's socket 0
    conn.child_socket_id = 0;        // Turret's socket 0
    conn.relative_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.5f, 0));
    blueprint.connections.push_back(conn);

    return blueprint;
}

/**
 * Create simple directional lighting for the scene
 * @return Vector with one directional light
 */
inline std::vector<LightData> CreateDefaultLighting() {
    std::vector<LightData> lights;

    // Sun light
    LightData sun;
    sun.position = glm::vec3(-10.0f, 10.0f, -10.0f);
    sun.direction =
        glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f)); // Actually this should be down
    sun.color = glm::vec3(1.0f, 0.95f, 0.9f);
    sun.intensity = 2.0f;
    sun.light_type = static_cast<uint32_t>(LightType::DIRECTIONAL);
    sun.radius = 1000.0f; // Not used for directional
    sun.inner_angle = 0.0f;
    sun.outer_angle = 0.0f;

    lights.push_back(sun);

    return lights;
}

} // namespace test_utils
} // namespace rendering_engine
