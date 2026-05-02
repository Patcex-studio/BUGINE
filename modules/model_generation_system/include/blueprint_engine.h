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
#include <string>
#include <immintrin.h> // For SIMD types
#include <glm/glm.hpp> // Assuming GLM is available

namespace model_generation {

// Forward declarations
struct BlueprintParameter;
struct SocketDefinition;
struct MaterialSlot;

struct BoundingBox {
    __m128 min_bounds;              // SIMD min point
    __m128 max_bounds;              // SIMD max point
};

// Asset ID type (assuming from asset manager)
using AssetID = uint64_t;

// SIMD-aligned transform (position, orientation quaternion, scale)
struct alignas(32) BlueprintNode {
    uint32_t node_type;             // PRIMITIVE, COMPOSITE, PARAMETRIC
    __m256 local_transform;         // SIMD: pos.xyz, quat.xyzw, scale.xyz (packed)
    std::vector<BlueprintParameter> parameters;
    std::vector<uint32_t> child_nodes;
    AssetID material_override;
    uint32_t lod_level;
    BoundingBox local_bounds;
};

struct BlueprintParameter {
    uint32_t param_type;            // FLOAT, VECTOR3, INTEGER, BOOLEAN
    union {
        float f_value;
        __m128 v3_value;            // SIMD vector3
        int32_t i_value;
        bool b_value;
    };
};

struct SocketDefinition {
    std::string socket_name;
    __m128 position;                // SIMD position
    __m128 orientation;             // SIMD quaternion
    uint32_t socket_type;           // ENGINE, WEAPON, TRACK, etc.
};

struct MaterialSlot {
    std::string slot_name;
    AssetID default_material;
    uint32_t material_type;         // METAL, PAINT, RUBBER, etc.
};

struct VehicleBlueprintDefinition {
    std::string blueprint_name;
    uint32_t vehicle_class;         // TANK, APC, ARTILLERY, etc.
    std::vector<BlueprintNode> nodes;
    std::vector<SocketDefinition> sockets;
    std::vector<MaterialSlot> material_slots;
    BoundingBox world_bounds;
};

// Enums for types
enum NodeType {
    PRIMITIVE = 0,
    COMPOSITE = 1,
    PARAMETRIC = 2
};

enum ParameterType {
    FLOAT = 0,
    VECTOR3 = 1,
    INTEGER = 2,
    BOOLEAN = 3
};

enum VehicleClass {
    TANK = 0,
    APC = 1,
    ARTILLERY = 2,
    AIRCRAFT = 3,
    // etc.
};

} // namespace model_generation