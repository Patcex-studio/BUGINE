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
#include <immintrin.h> // For AVX2 types
#include "types.h" // Assuming EntityID is defined there
#include "armor_materials.h" // For ArmorType

namespace physics_core {

// Component types enumeration
enum class ComponentType : uint32_t {
    HULL = 0,
    TURRET = 1,
    ENGINE = 2,
    TRACK_LEFT = 3,
    TRACK_RIGHT = 4,
    GUN_BARREL = 5,
    AMMO_RACK = 6,
    FUEL_TANK = 7,
    CREW_COMPARTMENT = 8,
    SUSPENSION = 9,
    COUNT
};

// Vulnerability flags
enum VulnerabilityFlags : uint32_t {
    NONE = 0,
    CRITICAL = 1 << 0,      // Critical component (engine, ammo)
    EXPLOSIVE = 1 << 1,     // Can explode (ammo, fuel)
    FLAMMABLE = 1 << 2,     // Can catch fire (fuel, crew)
    MOBILITY = 1 << 3,      // Affects movement (tracks, engine)
    FIREPOWER = 1 << 4      // Affects combat capability (gun, turret)
};

// Component-based vehicle structure
struct VehicleComponent {
    EntityID component_id;          // Unique component identifier
    uint32_t parent_vehicle_id;     // Parent vehicle reference
    ComponentType component_type;   // Component type
    float health_max;               // Max HP for this component
    float health_current;           // Current HP
    float armor_thickness;          // Base armor thickness (mm)
    float armor_angle;              // Slope angle relative to normal (degrees)
    ArmorType material_type;        // Armor material type
    uint32_t vulnerability_flags;   // Vulnerability flags
    __m256 local_transform;         // Local position/orientation (SIMD: x,y,z,w, rx,ry,rz,rw)
    __m256 world_aabb_min;          // World-space bounding box min (x,y,z)
    __m256 world_aabb_max;          // World-space bounding box max (x,y,z)
};

// SIMD batch processing: 8 components per AVX2 operation
struct ComponentBatch {
    __m256 health_current[8];       // Current health for 8 components
    __m256 armor_thickness[8];      // Armor thickness
    __m256 armor_angle[8];          // Armor angles
    __m256 material_hardness[8];    // Material hardness values
};

} // namespace physics_core