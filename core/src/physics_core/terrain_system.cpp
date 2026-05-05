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
#include "physics_core/terrain_system.h"
#include "physics_core/physics_thread_pool.h"
<<<<<<< HEAD
=======
#include "physics_core/simd_config.h"
>>>>>>> c308d63 (Helped the rabbits find a home)
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <glm/glm.hpp>

namespace physics_core {

TerrainTile::TerrainTile()
    : tile_x(0), tile_z(0), lod_level(0), is_ready(false), dirty(false), tile_flags(0), world_offset(_mm256_setzero_ps()) {
    clear();
}

void TerrainTile::clear() {
    std::memset(height_data, 0, sizeof(height_data));
    std::memset(surface_type, 0, sizeof(surface_type));
    std::memset(moisture_level, 0, sizeof(moisture_level));
    std::memset(snow_depth, 0, sizeof(snow_depth));
    tile_flags = 0;
    world_offset = _mm256_setzero_ps();
}

TerrainStreamingManager::TerrainStreamingManager(TerrainSystem& terrain_system, PhysicsThreadPool& thread_pool)
    : terrain_system_(terrain_system), thread_pool_(thread_pool) {}

void TerrainStreamingManager::update_streaming(const glm::vec2& camera_pos, float view_radius) {
    int16_t cx = static_cast<int16_t>(camera_pos.x / terrain_system_.tile_size());
    int16_t cz = static_cast<int16_t>(camera_pos.y / terrain_system_.tile_size());

    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            int16_t tx = cx + dx, tz = cz + dz;
            float dist = glm::distance(glm::vec2{tx, tz}, glm::vec2{cx, cz}) * terrain_system_.tile_size() / 1000.0f; // km
            uint8_t target_lod = TerrainSystem::compute_lod(dist);

            if (!is_tile_loaded(tx, tz, target_lod)) {
                schedule_async_load(tx, tz, target_lod);
            }
        }
    }
    evict_old_tiles();
}

void TerrainStreamingManager::schedule_async_load(int16_t tile_x, int16_t tile_z, uint8_t lod_level) {
    thread_pool_.submit_task([this, tile_x, tile_z, lod_level]() {
        TerrainTile new_tile;
        new_tile.tile_x = tile_x;
        new_tile.tile_z = tile_z;
        new_tile.lod_level = lod_level;
        new_tile.is_ready = false;
        // Generate height data procedurally or load from disk
        // For now, simple noise
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                new_tile.height_data[y][x] = sinf(x * 0.1f) * cosf(y * 0.1f) * 10.0f;
                new_tile.surface_type[y][x] = static_cast<uint8_t>(SurfaceType::DIRT);
            }
        }
        new_tile.world_offset = _mm256_setr_ps(tile_x * terrain_system_.tile_size(), tile_z * terrain_system_.tile_size(), 0.0f, 0.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f);
        commit_tile(new_tile);
    });
}

void TerrainStreamingManager::commit_tile(const TerrainTile& new_tile) {
    uint64_t key = make_key(new_tile.tile_x, new_tile.tile_z);
    auto tile_ptr = std::make_unique<TerrainTile>();
    tile_ptr->tile_x = new_tile.tile_x;
    tile_ptr->tile_z = new_tile.tile_z;
    tile_ptr->lod_level = new_tile.lod_level;
    tile_ptr->world_offset = new_tile.world_offset;
    tile_ptr->tile_flags = new_tile.tile_flags;
    tile_ptr->dirty.store(new_tile.dirty.load(std::memory_order_relaxed), std::memory_order_relaxed);
    std::memcpy(tile_ptr->height_data, new_tile.height_data, sizeof(tile_ptr->height_data));
    std::memcpy(tile_ptr->surface_type, new_tile.surface_type, sizeof(tile_ptr->surface_type));
    std::memcpy(tile_ptr->moisture_level, new_tile.moisture_level, sizeof(tile_ptr->moisture_level));
    std::memcpy(tile_ptr->snow_depth, new_tile.snow_depth, sizeof(tile_ptr->snow_depth));
    tile_ptr->is_ready.store(true, std::memory_order_release);

    active_tiles_[key] = std::move(tile_ptr);
    eviction_queue_.push(key);
}

bool TerrainStreamingManager::is_tile_loaded(int16_t x, int16_t z, uint8_t lod) const {
    auto it = active_tiles_.find(make_key(x, z));
    return it != active_tiles_.end() && it->second->lod_level <= lod && it->second->is_ready.load(std::memory_order_acquire);
}

const TerrainTile* TerrainStreamingManager::find_tile(int16_t x, int16_t z) const {
    auto it = active_tiles_.find(make_key(x, z));
    return it != active_tiles_.end() ? it->second.get() : nullptr;
}

TerrainTile* TerrainStreamingManager::find_tile(int16_t x, int16_t z) {
    auto it = active_tiles_.find(make_key(x, z));
    return it != active_tiles_.end() ? it->second.get() : nullptr;
}

void TerrainStreamingManager::evict_old_tiles() {
    while (active_tiles_.size() > MAX_ACTIVE_TILES && !eviction_queue_.empty()) {
        uint64_t key = eviction_queue_.front();
        eviction_queue_.pop();
        active_tiles_.erase(key);
    }
}

TerrainSystem::TerrainSystem()
    : tile_size_(100.0f),
      height_scale_(1.0f),
      max_lod_levels_(5),
      material_count_(kMaxSurfaceMaterials),
      streaming_manager_(nullptr) {
    set_default_materials();
}

void TerrainSystem::initialize(float tile_size, float height_scale, uint32_t max_lod_levels) {
    tile_size_ = tile_size;
    height_scale_ = height_scale;
    max_lod_levels_ = max_lod_levels;
    material_count_ = kMaxSurfaceMaterials;
    set_default_materials();
}

TerrainTile* TerrainSystem::add_tile(int16_t tile_x, int16_t tile_z, uint8_t lod_level, const Vec3& origin) {
    auto tile_ptr = std::make_unique<TerrainTile>();
    TerrainTile* tile = tile_ptr.get();
    tile->tile_x = tile_x;
    tile->tile_z = tile_z;
    tile->lod_level = lod_level;
    tile->is_ready.store(true, std::memory_order_release);
    tile->world_offset = _mm256_setr_ps(static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z), 0.0f,
                                       0.0f, 0.0f, 0.0f, 0.0f);
    tiles_.push_back(std::move(tile_ptr));
    active_tiles_.push_back(static_cast<uint32_t>(tiles_.size() - 1));
    return tile;
}

TerrainTile* TerrainSystem::add_tile(uint32_t tile_id, const Vec3& origin) {
    return add_tile(static_cast<int16_t>(tile_id), 0, 0, origin);
}

TerrainTile* TerrainSystem::get_tile(int16_t tile_x, int16_t tile_z) {
    for (auto& tile_ptr : tiles_) {
        TerrainTile* tile = tile_ptr.get();
        if (tile->tile_x == tile_x && tile->tile_z == tile_z) {
            return tile;
        }
    }
    return nullptr;
}

TerrainTile* TerrainSystem::get_tile(uint32_t tile_id) {
    return get_tile(static_cast<int16_t>(tile_id), 0);
}

const TerrainTile* TerrainSystem::get_tile(int16_t tile_x, int16_t tile_z) const {
    for (const auto& tile_ptr : tiles_) {
        const TerrainTile* tile = tile_ptr.get();
        if (tile->tile_x == tile_x && tile->tile_z == tile_z) {
            return tile;
        }
    }
    return nullptr;
}

const TerrainTile* TerrainSystem::get_tile(uint32_t tile_id) const {
    return get_tile(static_cast<int16_t>(tile_id), 0);
}

void TerrainSystem::set_default_materials() {
    material_count_ = kMaxSurfaceMaterials;
    for (size_t index = 0; index < kMaxSurfaceMaterials; ++index) {
        SurfaceMaterial material{};
        switch (index % 12) {
            case 0:
                material = {0.90f, 0.06f, 1.00f, 0.04f, 0.10f, 0.01f, static_cast<uint32_t>(index), true};
                break;
            case 1:
                material = {0.85f, 0.08f, 0.96f, 0.05f, 0.12f, 0.02f, static_cast<uint32_t>(index), true};
                break;
            case 2:
                material = {0.70f, 0.16f, 0.88f, 0.14f, 0.30f, 0.05f, static_cast<uint32_t>(index), true};
                break;
            case 3:
                material = {0.50f, 0.30f, 0.72f, 0.22f, 0.65f, 0.12f, static_cast<uint32_t>(index), true};
                break;
            case 4:
                material = {0.60f, 0.20f, 0.75f, 0.09f, 0.05f, 0.03f, static_cast<uint32_t>(index), false};
                break;
            case 5:
                material = {0.35f, 0.18f, 0.70f, 0.08f, 0.05f, 0.01f, static_cast<uint32_t>(index), false};
                break;
            case 6:
                material = {0.10f, 0.10f, 0.48f, 0.02f, 0.02f, 0.00f, static_cast<uint32_t>(index), false};
                break;
            case 7:
                material = {0.60f, 0.24f, 0.50f, 0.00f, 0.00f, 0.00f, static_cast<uint32_t>(index), false};
                break;
            case 8:
                material = {0.75f, 0.10f, 0.95f, 0.05f, 0.10f, 0.02f, static_cast<uint32_t>(index), false};
                break;
            case 9:
                material = {0.55f, 0.20f, 0.80f, 0.20f, 0.35f, 0.08f, static_cast<uint32_t>(index), true};
                break;
            case 10:
                material = {0.45f, 0.12f, 0.86f, 0.06f, 0.05f, 0.02f, static_cast<uint32_t>(index), true};
                break;
            default:
                material = {0.50f, 0.15f, 0.80f, 0.10f, 0.20f, 0.05f, static_cast<uint32_t>(index), false};
                break;
        }
        material_table_[index] = material;
    }
}

const SurfaceMaterial& TerrainSystem::get_material(uint8_t material_id) const {
    if (material_id >= material_count_) {
        static const SurfaceMaterial default_material = {0.65f, 0.15f, 0.85f, 0.08f, 0.20f, 0.03f, 0u, false};
        return default_material;
    }
    return material_table_[material_id];
}

void TerrainSystem::update_material_conditions(float rain_intensity, float temperature) {
    float rain_factor = std::clamp(rain_intensity, 0.0f, 1.0f);
    float temp_factor = std::clamp((35.0f - temperature) / 60.0f, 0.0f, 1.0f);

    for (size_t i = 0; i < material_count_; ++i) {
        SurfaceMaterial& material = material_table_[i];
        material.friction_coefficient = std::max(0.1f, material.friction_coefficient * (1.0f - rain_factor * 0.25f));
        material.traction_factor = std::max(0.3f, material.traction_factor * (1.0f - rain_factor * 0.20f));
        if (temp_factor > 0.8f && (i == static_cast<size_t>(SurfaceType::WATER) || i == static_cast<size_t>(SurfaceType::SNOW))) {
            material.friction_coefficient *= 0.9f;
        }
    }
}

const TerrainTile* TerrainSystem::find_tile_for_world(float world_x, float world_y) const {
    if (streaming_manager_) {
        int16_t tx = static_cast<int16_t>(world_x / tile_size_);
        int16_t tz = static_cast<int16_t>(world_y / tile_size_);
        const TerrainTile* tile = streaming_manager_->find_tile(tx, tz);
        if (tile && tile->is_ready.load(std::memory_order_acquire)) return tile;
    }
    for (const auto& tile_ptr : tiles_) {
        const TerrainTile* tile = tile_ptr.get();
        alignas(32) float origin[8];
        _mm256_store_ps(origin, tile->world_offset);
        if (world_x >= origin[0] && world_x < origin[0] + tile_size_ &&
            world_y >= origin[1] && world_y < origin[1] + tile_size_) {
            return tile;
        }
    }
    return nullptr;
}

TerrainTile* TerrainSystem::find_tile_for_world(float world_x, float world_y) {
    if (streaming_manager_) {
        int16_t tx = static_cast<int16_t>(world_x / tile_size_);
        int16_t tz = static_cast<int16_t>(world_y / tile_size_);
        TerrainTile* tile = streaming_manager_->find_tile(tx, tz);
        if (tile && tile->is_ready.load(std::memory_order_acquire)) return tile;
    }
    for (auto& tile_ptr : tiles_) {
        TerrainTile* tile = tile_ptr.get();
        alignas(32) float origin[8];
        _mm256_store_ps(origin, tile->world_offset);
        if (world_x >= origin[0] && world_x < origin[0] + tile_size_ &&
            world_y >= origin[1] && world_y < origin[1] + tile_size_) {
            return tile;
        }
    }
    return nullptr;
}

float TerrainSystem::sample_height_in_tile(const TerrainTile& tile, float local_x, float local_y) const {
    int ix = std::clamp(static_cast<int>(std::floor(local_x)), 0, static_cast<int>(kTileResolution - 2));
    int iy = std::clamp(static_cast<int>(std::floor(local_y)), 0, static_cast<int>(kTileResolution - 2));
    float fx = local_x - ix;
    float fy = local_y - iy;

    float h00 = tile.height_data[iy][ix];
    float h10 = tile.height_data[iy][ix + 1];
    float h01 = tile.height_data[iy + 1][ix];
    float h11 = tile.height_data[iy + 1][ix + 1];

    float height = (h00 * (1.0f - fx) + h10 * fx) * (1.0f - fy) +
                   (h01 * (1.0f - fx) + h11 * fx) * fy;
    return height * height_scale_;
}

uint8_t TerrainSystem::sample_surface_in_tile(const TerrainTile& tile, float local_x, float local_y) const {
    int ix = std::clamp(static_cast<int>(std::floor(local_x)), 0, static_cast<int>(kTileResolution - 1));
    int iy = std::clamp(static_cast<int>(std::floor(local_y)), 0, static_cast<int>(kTileResolution - 1));
    return tile.surface_type[iy][ix];
}

bool TerrainSystem::sample_height(float world_x, float world_y, float& height_out) const {
    const TerrainTile* tile = find_tile_for_world(world_x, world_y);
    if (!tile) {
        return false;
    }

    alignas(32) float origin[8];
    _mm256_store_ps(origin, tile->world_offset);
    float local_x = (world_x - origin[0]) / tile_size_ * static_cast<float>(kTileResolution - 1);
    float local_y = (world_y - origin[1]) / tile_size_ * static_cast<float>(kTileResolution - 1);
    height_out = sample_height_in_tile(*tile, local_x, local_y);
    return true;
}

uint8_t TerrainSystem::query_surface_type(float world_x, float world_y) const {
    const TerrainTile* tile = find_tile_for_world(world_x, world_y);
    if (!tile) {
        return static_cast<uint8_t>(SurfaceType::DIRT);
    }
    alignas(32) float origin[8];
    _mm256_store_ps(origin, tile->world_offset);
    float local_x = (world_x - origin[0]) / tile_size_ * static_cast<float>(kTileResolution);
    float local_y = (world_y - origin[1]) / tile_size_ * static_cast<float>(kTileResolution);
    return sample_surface_in_tile(*tile, local_x, local_y);
}

void TerrainSystem::query_height_simd(
    const float* positions_x,
    const float* positions_y,
    float* heights_out,
    size_t count
) const {
    const float inv_tile_scalar = 1.0f / tile_size_;

<<<<<<< HEAD
=======
    // Process 8 queries at a time using SIMD
>>>>>>> c308d63 (Helped the rabbits find a home)
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        __m256 world_x = _mm256_loadu_ps(positions_x + i);
        __m256 world_y = _mm256_loadu_ps(positions_y + i);

<<<<<<< HEAD
        alignas(32) float x_buffer[8];
        alignas(32) float y_buffer[8];
        alignas(32) float local_x[8];
        alignas(32) float local_y[8];

        _mm256_store_ps(x_buffer, world_x);
        _mm256_store_ps(y_buffer, world_y);

        for (size_t k = 0; k < 8; ++k) {
            const TerrainTile* tile = find_tile_for_world(x_buffer[k], y_buffer[k]);
            if (!tile) {
                heights_out[i + k] = 0.0f;
                continue;
            }
            alignas(32) float origin[8];
            _mm256_store_ps(origin, tile->world_offset);
            local_x[k] = (x_buffer[k] - origin[0]) * inv_tile_scalar * static_cast<float>(kTileResolution - 1);
            local_y[k] = (y_buffer[k] - origin[1]) * inv_tile_scalar * static_cast<float>(kTileResolution - 1);
            heights_out[i + k] = sample_height_in_tile(*tile, local_x[k], local_y[k]);
        }
    }

=======
        // Store to find tiles (tile lookup is not SIMD-friendly due to hash map)
        alignas(32) float x_buffer[8];
        alignas(32) float y_buffer[8];
        _mm256_store_ps(x_buffer, world_x);
        _mm256_store_ps(y_buffer, world_y);

        // Batch fetch tiles and precompute local coordinates
        alignas(32) const TerrainTile* tiles[8];
        alignas(32) float local_x[8];
        alignas(32) float local_y[8];
        alignas(32) float tile_origin_x[8];
        alignas(32) float tile_origin_y[8];
        
        // Fetch tile pointers (serial but cached)
        for (size_t k = 0; k < 8; ++k) {
            tiles[k] = find_tile_for_world(x_buffer[k], y_buffer[k]);
        }

        // Vectorize local coordinate computation
        __m256 inv_tile = _mm256_set1_ps(inv_tile_scalar);
        __m256 kTileResolution_m1 = _mm256_set1_ps(static_cast<float>(kTileResolution - 1));
        
        for (size_t k = 0; k < 8; ++k) {
            if (!tiles[k]) {
                heights_out[i + k] = 0.0f;
                continue;
            }
            
            // Extract world offset from tile
            alignas(32) float origin[8];
            _mm256_store_ps(origin, tiles[k]->world_offset);
            tile_origin_x[k] = origin[0];
            tile_origin_y[k] = origin[1];
            
            // Compute local coordinates using SIMD (but serially per tile since they vary)
            local_x[k] = (x_buffer[k] - tile_origin_x[k]) * inv_tile_scalar * static_cast<float>(kTileResolution - 1);
            local_y[k] = (y_buffer[k] - tile_origin_y[k]) * inv_tile_scalar * static_cast<float>(kTileResolution - 1);
            heights_out[i + k] = sample_height_in_tile(*tiles[k], local_x[k], local_y[k]);
        }
    }

    // Process remaining queries (< 8)
>>>>>>> c308d63 (Helped the rabbits find a home)
    for (; i < count; ++i) {
        const TerrainTile* tile = find_tile_for_world(positions_x[i], positions_y[i]);
        if (!tile) {
            heights_out[i] = 0.0f;
            continue;
        }
        alignas(32) float origin[8];
        _mm256_store_ps(origin, tile->world_offset);
        float local_x = (positions_x[i] - origin[0]) / tile_size_ * static_cast<float>(kTileResolution - 1);
        float local_y = (positions_y[i] - origin[1]) / tile_size_ * static_cast<float>(kTileResolution - 1);
        heights_out[i] = sample_height_in_tile(*tile, local_x, local_y);
    }
}

void TerrainSystem::query_surface_material_simd(
    const float* positions_x,
    const float* positions_y,
    uint8_t* materials_out,
    size_t count
) const {
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        alignas(32) float x_buffer[8];
        alignas(32) float y_buffer[8];
        _mm256_store_ps(x_buffer, _mm256_loadu_ps(positions_x + i));
        _mm256_store_ps(y_buffer, _mm256_loadu_ps(positions_y + i));

        for (size_t k = 0; k < 8; ++k) {
            const TerrainTile* tile = find_tile_for_world(x_buffer[k], y_buffer[k]);
            if (!tile) {
                materials_out[i + k] = static_cast<uint8_t>(SurfaceType::DIRT);
                continue;
            }
            alignas(32) float origin[8];
            _mm256_store_ps(origin, tile->world_offset);
            float local_x = (x_buffer[k] - origin[0]) / tile_size_ * static_cast<float>(kTileResolution);
            float local_y = (y_buffer[k] - origin[1]) / tile_size_ * static_cast<float>(kTileResolution);
            materials_out[i + k] = sample_surface_in_tile(*tile, local_x, local_y);
        }
    }

    for (; i < count; ++i) {
        const TerrainTile* tile = find_tile_for_world(positions_x[i], positions_y[i]);
        if (!tile) {
            materials_out[i] = static_cast<uint8_t>(SurfaceType::DIRT);
            continue;
        }
        alignas(32) float origin[8];
        _mm256_store_ps(origin, tile->world_offset);
        float local_x = (positions_x[i] - origin[0]) / tile_size_ * static_cast<float>(kTileResolution);
        float local_y = (positions_y[i] - origin[1]) / tile_size_ * static_cast<float>(kTileResolution);
        materials_out[i] = sample_surface_in_tile(*tile, local_x, local_y);
    }
}

void TerrainSystem::apply_rain(float intensity) {
    float delta = std::clamp(intensity * 8.0f, 0.0f, 16.0f);
    for (auto& tile_ptr : tiles_) {
        TerrainTile& tile = *tile_ptr;
        for (size_t y = 0; y < kTileResolution; ++y) {
            for (size_t x = 0; x < kTileResolution; ++x) {
                uint16_t value = static_cast<uint16_t>(tile.moisture_level[y][x]) + static_cast<uint16_t>(delta);
                tile.moisture_level[y][x] = static_cast<uint8_t>(std::min<uint16_t>(255, value));
            }
        }
    }
}

void TerrainSystem::apply_moisture_decay(float dt) {
    float decay = std::clamp(dt * 3.0f, 0.0f, 10.0f);
    for (auto& tile_ptr : tiles_) {
        TerrainTile& tile = *tile_ptr;
        for (size_t y = 0; y < kTileResolution; ++y) {
            for (size_t x = 0; x < kTileResolution; ++x) {
                int value = static_cast<int>(tile.moisture_level[y][x]) - static_cast<int>(decay);
                tile.moisture_level[y][x] = static_cast<uint8_t>(std::max(0, value));
            }
        }
    }
}

void TerrainSystem::generate_mud_from_vehicles(float threshold) {
    for (auto& tile_ptr : tiles_) {
        TerrainTile& tile = *tile_ptr;
        for (size_t y = 0; y < kTileResolution; ++y) {
            for (size_t x = 0; x < kTileResolution; ++x) {
                if (tile.moisture_level[y][x] >= static_cast<uint8_t>(threshold * 255.0f)) {
                    tile.surface_type[y][x] = static_cast<uint8_t>(SurfaceType::MUD);
                }
            }
        }
    }
}

void TerrainSystem::apply_crater(const TerrainCrater& crater) {
    // Handle streaming tiles
    if (streaming_manager_) {
        int16_t min_tx = static_cast<int16_t>((crater.center_x - crater.radius) / tile_size_);
        int16_t max_tx = static_cast<int16_t>((crater.center_x + crater.radius) / tile_size_);
        int16_t min_tz = static_cast<int16_t>((crater.center_y - crater.radius) / tile_size_);
        int16_t max_tz = static_cast<int16_t>((crater.center_y + crater.radius) / tile_size_);
        for (int16_t tz = min_tz; tz <= max_tz; ++tz) {
            for (int16_t tx = min_tx; tx <= max_tx; ++tx) {
                TerrainTile* tile = streaming_manager_->find_tile(tx, tz);
                if (tile && tile->is_ready.load(std::memory_order_acquire)) {
                    apply_crater_to_tile(*tile, crater);
                    tile->dirty.store(true, std::memory_order_release);
                }
            }
        }
    }
    // Handle legacy tiles
    for (auto& tile_ptr : tiles_) {
        TerrainTile& tile = *tile_ptr;
        alignas(32) float origin[8];
        _mm256_store_ps(origin, tile.world_offset);
        if (crater.center_x + crater.radius < origin[0] || crater.center_x - crater.radius > origin[0] + tile_size_ ||
            crater.center_y + crater.radius < origin[1] || crater.center_y - crater.radius > origin[1] + tile_size_) {
            continue;
        }
        apply_crater_to_tile(tile, crater);
        tile.dirty = true;
    }
}

void TerrainSystem::apply_crater_to_tile(TerrainTile& tile, const TerrainCrater& crater) {
    alignas(32) float origin[8];
    _mm256_store_ps(origin, tile.world_offset);
    for (size_t y = 0; y < kTileResolution; ++y) {
        for (size_t x = 0; x < kTileResolution; ++x) {
            float sample_x = origin[0] + (static_cast<float>(x) / static_cast<float>(kTileResolution - 1)) * tile_size_;
            float sample_y = origin[1] + (static_cast<float>(y) / static_cast<float>(kTileResolution - 1)) * tile_size_;
            float dx = sample_x - crater.center_x;
            float dy = sample_y - crater.center_y;
            float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > crater.radius) {
                continue;
            }
            float normalized = 1.0f - (distance / crater.radius);
            float delta = crater.depth * normalized * std::pow(normalized, crater.falloff);
            tile.height_data[y][x] = std::max(0.0f, tile.height_data[y][x] - delta);
        }
    }
}

}  // namespace physics_core
