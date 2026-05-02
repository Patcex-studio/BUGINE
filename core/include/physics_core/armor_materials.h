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
#include <cstddef>
#include <array>
#include <vector>

namespace physics_core {

// Armor material properties for penetration calculations
struct ArmorMaterial {
    float hardness_rha;             // Relative Hardness vs RHA (baseline = 1.0)
    float density;                  // kg/m³
    float spall_coefficient;        // Spalling probability factor (0.0-1.0)
    float ricochet_threshold;       // Critical angle for ricochet (degrees)
    float heat_resistance;          // Thermal protection factor
    float composite_layers;         // Layer count for composites
    float ceramic_efficiency;       // Ceramic layer effectiveness (0.0-1.0)
};

// Predefined armor types
enum class ArmorType : uint32_t {
    RHA = 0,        // Rolled Homogeneous Armor (baseline)
    CHA = 1,        // Cast Homogeneous Armor
    FHA = 2,        // Face-Hardened Armor
    COMPOSITE = 3,  // Modern composite (Chobham, etc.)
    REACTIVE = 4,   // Explosive Reactive Armor
    SPACED = 5,     // Spaced armor (Soviet style)
    ALUMINUM = 6,   // Lightweight aluminum
    CERAMIC = 7,    // Ceramic armor
    STEEL = 8,      // Steel variants
    COUNT           // Keep last for array sizing
};

// Layered armor structures
enum class MaterialType : uint32_t {
    STEEL_RHA = 0,
    CERAMIC = 1,
    COMPOSITE_SPACER = 2,
    AIR_GAP = 3,
    COUNT
};

struct alignas(16) ArmorLayer {
    float thickness_mm;
    MaterialType type;
    float angle_deg; // Normal angle
    float density;   // kg/m³
    float hardness;  // Hardness factor
};

struct ArmorPack {
    std::vector<ArmorLayer> layers;
};

// Material database - 50+ predefined types
extern const std::array<ArmorMaterial, static_cast<size_t>(ArmorType::COUNT)> armor_materials;

// Helper function to get material by type
inline const ArmorMaterial& get_armor_material(ArmorType type) {
    return armor_materials[static_cast<size_t>(type)];
}

} // namespace physics_core