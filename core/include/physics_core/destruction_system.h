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

#include "physics_core/types.h"
#include "physics_core/terrain_system.h"
#include <vector>
#include <cstdint>
#include <immintrin.h>

namespace physics_core {

class PhysicsCore;

struct DestructibleObject {
    EntityID object_id;
    uint32_t object_type;
    float structural_integrity;
    float destruction_threshold;
    __m256 world_aabb_min;
    __m256 world_aabb_max;
    uint32_t fragment_count;
    uint32_t destruction_flags;
};

class DestructionSystem {
public:
    DestructionSystem();

    void register_object(const DestructibleObject& object);
    void apply_damage(EntityID object_id, float damage, TerrainSystem& terrain, PhysicsCore& physics_core);
    void update(float dt, TerrainSystem& terrain);
    void cleanup_old_effects();

    const std::vector<DestructibleObject>& get_objects() const { return objects_; }
    const std::vector<TerrainCrater>& get_craters() const { return craters_; }

private:
    void simulate_collapse(DestructibleObject& object, TerrainSystem& terrain, PhysicsCore& physics_core, const Vec3& impact_impulse = Vec3{0,0,0});

    std::vector<DestructibleObject> objects_;
    std::vector<TerrainCrater> craters_;
};

}  // namespace physics_core
