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

#include "types.h"
#include <array>
#include <cstdint>
#include <vector>
#include <deque>
#include <immintrin.h>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <queue>
#include <atomic>
#include <memory>
#include <glm/glm.hpp>

namespace physics_core {

enum class SurfaceType : uint8_t {
    CONCRETE = 0,
    ASPHALT = 1,
    DIRT = 2,
    MUD = 3,
    SAND = 4,
    SNOW = 5,
    ICE = 6,
    WATER = 7,
    GRASS = 8,
    FOREST = 9,
    ROCK = 10,
    SWAMP = 11
};

struct SurfaceMaterial {
    float friction_coefficient;
    float rolling_resistance;
    float traction_factor;
    float deformation_depth;
    float mud_formation_threshold;
    float erosion_rate;
    uint32_t visual_material_id;
    bool is_destructible;
};
static_assert(sizeof(SurfaceMaterial) <= 32, "SurfaceMaterial must fit within 32 bytes");

struct alignas(32) TerrainTile {
    int16_t tile_x, tile_z;
    uint8_t lod_level;              // 0=64x64, 1=32x32, 2=16x16
    std::atomic<bool> is_ready;     // Атомарный флаг готовности
    std::atomic<bool> dirty;        // Флаг для перестройки меша
    float height_data[64][64];
    uint8_t surface_type[64][64];
    uint8_t moisture_level[64][64];
    uint8_t snow_depth[64][64];
    uint32_t tile_flags;
    __m256 world_offset;

    TerrainTile();
    void clear();
};

struct TerrainCrater {
    float center_x;
    float center_y;
    float center_z;
    float radius;
    float depth;
    float falloff;
    uint32_t age;
    uint32_t max_age;
};

class TerrainSystem;
class PhysicsThreadPool;

class TerrainStreamingManager {
public:
    TerrainStreamingManager(TerrainSystem& terrain_system, PhysicsThreadPool& thread_pool);
    void update_streaming(const glm::vec2& camera_pos, float view_radius);
    void schedule_async_load(int16_t tile_x, int16_t tile_z, uint8_t lod_level);
    void commit_tile(const TerrainTile& new_tile);
    const TerrainTile* find_tile(int16_t x, int16_t z) const;
    TerrainTile* find_tile(int16_t x, int16_t z);

private:
    TerrainSystem& terrain_system_;
    PhysicsThreadPool& thread_pool_;
    std::unordered_map<uint64_t, std::unique_ptr<TerrainTile>> active_tiles_; // key = (tile_x << 32) | tile_z
    std::queue<uint64_t> eviction_queue_;
    static constexpr size_t MAX_ACTIVE_TILES = 256;

    uint64_t make_key(int16_t x, int16_t z) const { return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(z)); }
    bool is_tile_loaded(int16_t x, int16_t z, uint8_t lod) const;
    void evict_old_tiles();
};

class TerrainSystem {
public:
    static constexpr size_t kTileResolution = 64;
    static constexpr size_t kMaxSurfaceMaterials = 64;

    TerrainSystem();
    void initialize(float tile_size = 100.0f, float height_scale = 1.0f, uint32_t max_lod_levels = 5);
    void set_streaming_manager(std::unique_ptr<TerrainStreamingManager> manager) { streaming_manager_ = std::move(manager); }
    TerrainTile* add_tile(int16_t tile_x, int16_t tile_z, uint8_t lod_level, const Vec3& origin);
    TerrainTile* add_tile(uint32_t tile_id, const Vec3& origin);
    TerrainTile* get_tile(int16_t tile_x, int16_t tile_z);
    TerrainTile* get_tile(uint32_t tile_id);
    const TerrainTile* get_tile(int16_t tile_x, int16_t tile_z) const;
    const TerrainTile* get_tile(uint32_t tile_id) const;

    void set_default_materials();
    const SurfaceMaterial& get_material(uint8_t material_id) const;
    void update_material_conditions(float rain_intensity, float temperature);

    bool sample_height(float world_x, float world_y, float& height_out) const;
    uint8_t query_surface_type(float world_x, float world_y) const;

    void query_height_simd(
        const float* positions_x,
        const float* positions_y,
        float* heights_out,
        size_t count
    ) const;

    void query_surface_material_simd(
        const float* positions_x,
        const float* positions_y,
        uint8_t* materials_out,
        size_t count
    ) const;

    void apply_rain(float intensity);
    void apply_moisture_decay(float dt);
    void generate_mud_from_vehicles(float threshold);
    void apply_crater(const TerrainCrater& crater);
    static uint8_t compute_lod(float distance_km) noexcept {
        if (distance_km < 0.5f) return 0;
        if (distance_km < 2.0f) return 1;
        return 2;
    }
    float tile_size() const { return tile_size_; }
    float height_scale() const { return height_scale_; }
    uint32_t max_lod_levels() const { return max_lod_levels_; }

    std::vector<std::unique_ptr<TerrainTile>> tiles_;
    std::vector<uint32_t> active_tiles_;
    std::unique_ptr<TerrainStreamingManager> streaming_manager_;

private:
    const TerrainTile* find_tile_for_world(float world_x, float world_y) const;
    TerrainTile* find_tile_for_world(float world_x, float world_y);
    float sample_height_in_tile(const TerrainTile& tile, float local_x, float local_y) const;
    uint8_t sample_surface_in_tile(const TerrainTile& tile, float local_x, float local_y) const;
    void apply_crater_to_tile(TerrainTile& tile, const TerrainCrater& crater);

    float tile_size_;
    float height_scale_;
    uint32_t max_lod_levels_;
    std::array<SurfaceMaterial, kMaxSurfaceMaterials> material_table_;
    size_t material_count_;
};

}  // namespace physics_core
