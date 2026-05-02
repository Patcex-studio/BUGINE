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
#include "projectile_properties.h"

namespace physics_core {

// Example projectile database - expand to 200+ entries
const std::vector<ProjectileProperties> projectile_database = {
    // WWII era
    {ProjectileType::APCBC, 75.0f, 6.8f, 619.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0.0f}, // Sherman 75mm
    {ProjectileType::AP, 88.0f, 10.2f, 773.0f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0.0f},  // Tiger I 88mm

    // Cold War
    {ProjectileType::APFSDS, 105.0f, 4.1f, 1493.0f, 0.18f, 0.0f, 400.0f, 0.0f, 0.0f, false, 0.0f}, // M1 Abrams 105mm
    {ProjectileType::HEAT, 125.0f, 19.0f, 905.0f, 0.35f, 1.8f, 0.0f, 125.0f, 0.0f, false, 0.0f},   // T-72 125mm

    // Modern
    {ProjectileType::APFSDS, 120.0f, 4.3f, 1650.0f, 0.15f, 0.0f, 500.0f, 0.0f, 0.0f, false, 0.0f}, // Leopard 2 120mm
    {ProjectileType::ATGM, 152.0f, 27.0f, 370.0f, 0.4f, 4.5f, 0.0f, 152.0f, 50.0f, true, 1.0f},    // Kornet ATGM
    {ProjectileType::HESH, 120.0f, 11.0f, 670.0f, 0.45f, 2.2f, 0.0f, 0.0f, 0.0f, false, 0.0f}     // Challenger 120mm
};

const ProjectileProperties* get_projectile_by_id(uint32_t id) {
    if (id < projectile_database.size()) {
        return &projectile_database[id];
    }
    return nullptr;
}

const ProjectileProperties* find_projectile_by_caliber(float caliber, ProjectileType type) {
    for (const auto& proj : projectile_database) {
        if (proj.caliber == caliber && proj.projectile_type == type) {
            return &proj;
        }
    }
    return nullptr;
}

} // namespace physics_core