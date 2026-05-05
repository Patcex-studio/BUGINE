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
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace physics_core {

/**
 * @class HierarchicalGrid
 * @brief Z-curve based hierarchical spatial grid for particles
 * 
 * Uses Morton codes for 3D spatial indexing with hierarchical blocks.
 * Optimized for 100K+ particles with efficient neighbor queries.
 */
class HierarchicalGrid {
public:
    struct Block {
        Vec3 center;
        float size;
        uint32_t start_index;
        uint32_t count;
        uint32_t level;
    };

    HierarchicalGrid();
    ~HierarchicalGrid();

    /**
     * Build spatial index from particle positions
     * @param positions_x, positions_y, positions_z - SoA position arrays
     * @param count - number of particles
     */
    void build(const float* positions_x, const float* positions_y, const float* positions_z, size_t count);

    /**
     * Query particles within radius
     * @param x, y, z - query position
     * @param radius - query radius
     * @param callback - function called for each found particle index
     */
    template<typename Func>
    void query_radius(float x, float y, float z, float radius, Func&& callback) const;

    /**
     * Get memory usage in bytes
     */
    size_t memory_usage() const;

    /**
     * Clear the grid
     */
    void clear();

private:
    static constexpr uint32_t MORTON_BITS = 21;  // 21 bits per coordinate (63 total)
    static constexpr uint32_t MAX_LEVELS = 8;
    static constexpr uint32_t BLOCK_SIZE = 32;   // Particles per leaf block

    std::vector<Block> blocks_;
    std::vector<uint32_t> sorted_indices_;
    std::vector<uint64_t> morton_codes_;
    
    Vec3 bounds_min_;
    Vec3 bounds_max_;
    float cell_size_;

    /**
     * Compute 64-bit Morton code from 3D coordinates
     */
    static uint64_t morton_code(float x, float y, float z, const Vec3& min_bounds, float cell_size);

    /**
     * Expand 21-bit integer to 63-bit Morton code
     */
    static uint64_t expand_bits(uint32_t x);

    /**
     * Build hierarchical blocks from sorted particles
     */
    void build_blocks(const float* pos_x, const float* pos_y, const float* pos_z, size_t count);

    /**
     * Find blocks overlapping with query sphere
     */
    void find_overlapping_blocks(float x, float y, float z, float radius, std::vector<uint32_t>& block_indices) const;
};

template<typename Func>
void HierarchicalGrid::query_radius(float x, float y, float z, float radius, Func&& callback) const {
    std::vector<uint32_t> overlapping_blocks;
    find_overlapping_blocks(x, y, z, radius, overlapping_blocks);
    
    for (uint32_t block_idx : overlapping_blocks) {
        const Block& block = blocks_[block_idx];
        
        // Check each particle in the block
        for (uint32_t i = 0; i < block.count; ++i) {
            uint32_t particle_idx = sorted_indices_[block.start_index + i];
            
            // Distance check would be done by caller since we don't have position arrays here
            // This is just the block filtering - actual distance check happens in DEMSystem
            callback(particle_idx);
        }
    }
}

} // namespace physics_core