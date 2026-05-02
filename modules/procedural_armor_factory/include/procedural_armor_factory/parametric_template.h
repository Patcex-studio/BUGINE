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
#include <glm/glm.hpp>

namespace procedural_armor_factory {

/**
 * @struct ParametricTankTemplate
 * @brief Procedural tank blueprint derived from historical specifications.
 * Used as input to generate 3D models, physics parameters, and damage components.
 */
struct ParametricTankTemplate {
    // --- Gabariты корпуса (dimensions) ---
    float hull_length;              // meters
    float hull_width;               // meters
    float hull_height;              // meters
    float hull_front_angle;         // angle of front armor in degrees (0 = vertical)

    // --- Ходовая часть (suspension/drive) ---
    uint8_t road_wheels_count;      // number of road wheels per side
    float wheel_radius;             // meters
    float track_width;              // meters

    // --- Башня и орудие (turret and gun) ---
    float turret_ring_diameter;     // meters (turret ring diameter)
    float turret_height;            // meters
    float gun_caliber;              // millimeters
    float gun_length;               // in calibers (e.g., 50 = 50mm for 100mm gun)

    // --- Детализация (detail/era) ---
    uint32_t seed;                  // random seed for detail generation
    uint8_t era_flags;              // bitwise flags: ENG=1, GERMAN=2, SOVIET=4, etc.

    // --- Масса (mass) ---
    float weight_tons;              // vehicle weight in metric tons

    ParametricTankTemplate() = default;
};

} // namespace procedural_armor_factory
