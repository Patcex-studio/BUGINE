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
#include <vector>

namespace physics_core {

// Projectile types enumeration
enum class ProjectileType : uint32_t {
    AP = 0,         // Armor-Piercing
    APC = 1,        // Armor-Piercing Capped
    APCBC = 2,      // Armor-Piercing Capped Ballistic Capped
    APFSDS = 3,     // Armor-Piercing Fin-Stabilized Discarding Sabot
    HEAT = 4,       // High-Explosive Anti-Tank
    HESH = 5,       // High-Explosive Squash Head
    APDS = 6,       // Armor-Piercing Discarding Sabot
    ATGM = 7,       // Anti-Tank Guided Missile
    COUNT
};

// Projectile properties structure
struct ProjectileProperties {
    ProjectileType projectile_type;
    float caliber;                  // Diameter in mm
    float mass;                     // Mass in kg
    float velocity_muzzle;          // Muzzle velocity m/s
    float drag_coefficient;         // Aerodynamic drag
    float explosive_mass;           // TNT equivalent kg
    float penetrator_length;        // For long-rod penetrators (mm)
    float shaped_charge_diameter;   // For HEAT warheads (mm)
    float fuse_delay;               // Milliseconds
    bool has_guidance;              // Laser/GPS guidance
    float guidance_accuracy;        // CEP in meters
};

// Database of historical/modern projectiles (200+ entries)
extern const std::vector<ProjectileProperties> projectile_database;

// Helper functions
const ProjectileProperties* get_projectile_by_id(uint32_t id);
const ProjectileProperties* find_projectile_by_caliber(float caliber, ProjectileType type);

} // namespace physics_core