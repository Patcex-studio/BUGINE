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
#include "spatial_hash.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace physics_core {

HierarchicalGrid::HierarchicalGrid() = default;
HierarchicalGrid::~HierarchicalGrid() = default;

void HierarchicalGrid::build(const float* positions_x, const float* positions_y, const float* positions_z, size_t count) {
    if (count == 0) return;

    clear();
    
    // Compute bounds
    bounds_min_ = Vec3(std::numeric_limits<float>::max());
    bounds_max_ = Vec3(std::numeric_limits<float>::lowest());
    
    for (size_t i = 0; i < count; ++i) {
        bounds_min_.x = std::min(bounds_min_.x, positions_x[i]);
        bounds_min_.y = std::min(bounds_min_.y, positions_y[i]);
        bounds_min_.z = std::min(bounds_min_.z, positions_z[i]);
        bounds_max_.x = std::max(bounds_max_.x, positions_x[i]);
        bounds_max_.y = std::max(bounds_max_.y, positions_y[i]);
        bounds_max_.z = std::max(bounds_max_.z, positions_z[i]);
    }
    
    // Add padding
    Vec3 padding = (bounds_max_ - bounds_min_) * 0.01f;
    bounds_min_ -= padding;
    bounds_max_ += padding;
    
    // Compute cell size (power of 2 for Morton codes)
    Vec3 extent = bounds_max_ - bounds_min_;
    float max_extent = std::max({extent.x, extent.y, extent.z});
    cell_size_ = max_extent / (1 << MORTON_BITS);
    
    // Generate Morton codes and sort
    morton_codes_.resize(count);
    sorted_indices_.resize(count);
    for (size_t i = 0; i < count; ++i) {
        sorted_indices_[i] = static_cast<uint32_t>(i);
        morton_codes_[i] = morton_code(positions_x[i], positions_y[i], positions_z[i], bounds_min_, cell_size_);
    }
    
    // Sort by Morton code
    std::sort(sorted_indices_.begin(), sorted_indices_.end(), 
        [&](uint32_t a, uint32_t b) {
            return morton_codes_[a] < morton_codes_[b];
        });
    
    // Build hierarchical blocks
    build_blocks(positions_x, positions_y, positions_z, count);
}

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

size_t HierarchicalGrid::memory_usage() const {
    return blocks_.capacity() * sizeof(Block) +
           sorted_indices_.capacity() * sizeof(uint32_t) +
           morton_codes_.capacity() * sizeof(uint64_t);
}

void HierarchicalGrid::clear() {
    blocks_.clear();
    sorted_indices_.clear();
    morton_codes_.clear();
}

uint64_t HierarchicalGrid::morton_code(float x, float y, float z, const Vec3& min_bounds, float cell_size) {
    // Quantize coordinates
    uint32_t ix = static_cast<uint32_t>((x - min_bounds.x) / cell_size);
    uint32_t iy = static_cast<uint32_t>((y - min_bounds.y) / cell_size);
    uint32_t iz = static_cast<uint32_t>((z - min_bounds.z) / cell_size);
    
    // Clamp to valid range
    ix = std::min(ix, (1u << MORTON_BITS) - 1);
    iy = std::min(iy, (1u << MORTON_BITS) - 1);
    iz = std::min(iz, (1u << MORTON_BITS) - 1);
    
    // Interleave bits
    return expand_bits(ix) | (expand_bits(iy) << 1) | (expand_bits(iz) << 2);
}

uint64_t HierarchicalGrid::expand_bits(uint32_t x) {
    // Expand 21-bit number to 63-bit Morton code
    uint64_t result = x;
    result = (result | (result << 32)) & 0x1f00000000ffffull;
    result = (result | (result << 16)) & 0x1f0000ff0000ffull;
    result = (result | (result << 8))  & 0x100f00f00f00f00full;
    result = (result | (result << 4))  & 0x10c30c30c30c30c3ull;
    result = (result | (result << 2))  & 0x1249249249249249ull;
    return result;
}

void HierarchicalGrid::build_blocks(const float* pos_x, const float* pos_y, const float* pos_z, size_t count) {
    // Group particles into blocks of BLOCK_SIZE
    size_t num_blocks = (count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    blocks_.reserve(num_blocks);
    
    for (size_t block_start = 0; block_start < count; block_start += BLOCK_SIZE) {
        size_t block_end = std::min(block_start + BLOCK_SIZE, count);
        size_t block_count = block_end - block_start;
        
        // Compute block bounds
        Vec3 block_min(std::numeric_limits<float>::max());
        Vec3 block_max(std::numeric_limits<float>::lowest());
        
        for (size_t i = block_start; i < block_end; ++i) {
            uint32_t particle_idx = sorted_indices_[i];
            Vec3 pos(pos_x[particle_idx], pos_y[particle_idx], pos_z[particle_idx]);
            block_min.x = std::min(block_min.x, pos.x);
            block_min.y = std::min(block_min.y, pos.y);
            block_min.z = std::min(block_min.z, pos.z);
            block_max.x = std::max(block_max.x, pos.x);
            block_max.y = std::max(block_max.y, pos.y);
            block_max.z = std::max(block_max.z, pos.z);
        }
        
        Block block;
        block.center = (block_min + block_max) * 0.5f;
        block.size = (block_max - block_min).magnitude() * 0.5f;
        block.start_index = static_cast<uint32_t>(block_start);
        block.count = static_cast<uint32_t>(block_count);
        block.level = 0;  // Leaf level
        
        blocks_.push_back(block);
    }
}

void HierarchicalGrid::find_overlapping_blocks(float x, float y, float z, float radius, std::vector<uint32_t>& block_indices) const {
    block_indices.clear();
    
    for (size_t i = 0; i < blocks_.size(); ++i) {
        const Block& block = blocks_[i];
        
        // Simple sphere-AABB overlap test
        Vec3 closest(
            std::max(block.center.x - block.size, std::min(x, block.center.x + block.size)),
            std::max(block.center.y - block.size, std::min(y, block.center.y + block.size)),
            std::max(block.center.z - block.size, std::min(z, block.center.z + block.size))
        );
        
        Vec3 delta = Vec3(x, y, z) - closest;
        float distance_sq = delta.dot(delta);
        
        if (distance_sq <= radius * radius) {
            block_indices.push_back(static_cast<uint32_t>(i));
        }
    }
}

} // namespace physics_core