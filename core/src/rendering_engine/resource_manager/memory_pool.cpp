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
#include "../../../include/rendering_engine/resource_manager/memory_pool.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rendering_engine::resource_manager {

bool MemoryPool::initialize(size_t size_bytes, uint32_t alignment) {
    if (size_bytes == 0 || alignment == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    try {
        memory_block_ = std::make_unique<char[]>(size_bytes);
        block_size_ = size_bytes;
        alignment_ = alignment;
        used_bytes_ = 0;
        next_handle_ = 1;
        
        // Initialize with single free block covering entire pool
        free_blocks_.push(FreeBlock(0, size_bytes));
        
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

void* MemoryPool::allocate(size_t size, uint32_t alignment) {
    if (size == 0) {
        return nullptr;
    }
    
    if (alignment == 0) {
        alignment = alignment_;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (memory_block_ == nullptr) {
        return nullptr;
    }
    
    // Find best-fit free block
    auto best_fit = find_best_fit(size, alignment);
    if (!best_fit.has_value()) {
        return nullptr;
    }
    
    FreeBlock block = best_fit.value();
    
    // Align offset within the block
    uint64_t aligned_offset = align_offset(block.offset, alignment);
    uint64_t padding = aligned_offset - block.offset;
    size_t total_size = size + padding;
    
    // Check if block is large enough
    if (total_size > block.size) {
        return nullptr;
    }
    
    // Create allocation info
    AllocationInfo alloc(aligned_offset, size, next_handle_++, alignment);
    allocations_.push_back(alloc);
    
    // Update free block
    uint64_t remaining_offset = aligned_offset + size;
    size_t remaining_size = block.size - total_size;
    
    if (remaining_size > 0) {
        free_blocks_.push(FreeBlock(remaining_offset, remaining_size));
    }
    
    used_bytes_ += size;
    
    return memory_block_.get() + aligned_offset;
}

bool MemoryPool::deallocate(uint64_t offset, size_t size) {
    if (size == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Find and mark allocation as free
    auto it = std::find_if(allocations_.begin(), allocations_.end(),
        [offset](const AllocationInfo& alloc) {
            return alloc.offset == offset && alloc.is_active;
        });
    
    if (it == allocations_.end()) {
        return false;
    }
    
    it->is_active = false;
    used_bytes_ -= it->size;
    free_blocks_.push(FreeBlock(offset, size));
    
    return true;
}

MemoryPool::PoolStats MemoryPool::get_stats() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    PoolStats stats;
    stats.total_size = block_size_;
    stats.used_bytes = used_bytes_;
    stats.free_bytes = block_size_ - used_bytes_;
    stats.fragmentation_percent = static_cast<uint32_t>(calculate_fragmentation());
    
    stats.active_allocations = 0;
    for (const auto& alloc : allocations_) {
        if (alloc.is_active) {
            stats.active_allocations++;
        }
    }
    
    stats.free_blocks_count = free_blocks_.size();
    
    return stats;
}

uint32_t MemoryPool::defragment() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (is_locked_.load()) {
        return 0;
    }
    
    uint32_t coalesced = 0;
    
    // Sort free blocks by offset
    std::vector<FreeBlock> sorted_blocks;
    while (!free_blocks_.empty()) {
        sorted_blocks.push_back(free_blocks_.top());
        free_blocks_.pop();
    }
    std::sort(sorted_blocks.begin(), sorted_blocks.end(),
        [](const FreeBlock& a, const FreeBlock& b) {
            return a.offset < b.offset;
        });
    
    // Coalesce adjacent blocks
    std::vector<FreeBlock> coalesced_blocks;
    for (const auto& block : sorted_blocks) {
        if (!coalesced_blocks.empty() && 
            coalesced_blocks.back().offset + coalesced_blocks.back().size == block.offset) {
            coalesced_blocks.back().size += block.size;
            coalesced++;
        } else {
            coalesced_blocks.push_back(block);
        }
    }
    
    // Rebuild free blocks queue
    for (const auto& block : coalesced_blocks) {
        free_blocks_.push(block);
    }
    
    return coalesced;
}

bool MemoryPool::contains(const void* ptr) const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (memory_block_ == nullptr || ptr == nullptr) {
        return false;
    }
    
    const char* char_ptr = static_cast<const char*>(ptr);
    return char_ptr >= memory_block_.get() && 
           char_ptr < memory_block_.get() + block_size_;
}

MemoryPool::~MemoryPool() {
    memory_block_.reset();
}

std::optional<FreeBlock> MemoryPool::find_best_fit(size_t size, uint32_t alignment) {
    // Create temporary vector from priority queue
    std::vector<FreeBlock> blocks;
    std::optional<FreeBlock> best_fit;
    
    while (!free_blocks_.empty()) {
        FreeBlock block = free_blocks_.top();
        free_blocks_.pop();
        blocks.push_back(block);
        
        // Check if this block can accommodate the allocation
        uint64_t aligned_offset = align_offset(block.offset, alignment);
        if (aligned_offset + size <= block.offset + block.size) {
            if (!best_fit.has_value() || block.size < best_fit->size) {
                best_fit = block;
            }
        }
    }
    
    // Restore priority queue
    for (const auto& block : blocks) {
        free_blocks_.push(block);
    }
    
    return best_fit;
}

float MemoryPool::calculate_fragmentation() const {
    if (used_bytes_ == 0 || free_blocks_.empty()) {
        return 0.0f;
    }
    
    // Fragmentation = (number of free blocks - 1) / total free regions
    // Represents how split up the free space is
    size_t total_free = block_size_ - used_bytes_;
    if (total_free == 0) {
        return 0.0f;
    }
    
    int num_free_blocks = free_blocks_.size();
    if (num_free_blocks <= 1) {
        return 0.0f;
    }
    
    return (100.0f * (num_free_blocks - 1)) / num_free_blocks;
}

// ============================================================================
// PoolAllocator Implementation
// ============================================================================

bool PoolAllocator::initialize(const std::array<size_t, 4>& pool_config) {
    for (uint32_t i = 0; i < 4; ++i) {
        if (!pools_[i].initialize(pool_config[i])) {
            return false;
        }
    }
    return true;
}

std::pair<void*, uint64_t> PoolAllocator::allocate(PoolType pool_type, 
                                                    size_t size, 
                                                    uint32_t alignment) {
    uint32_t pool_idx = static_cast<uint32_t>(pool_type);
    if (pool_idx >= 4) {
        return {nullptr, 0};
    }
    
    void* ptr = pools_[pool_idx].allocate(size, alignment);
    if (ptr != nullptr) {
        total_allocated_ += size;
        size_t current = total_allocated_.load();
        size_t peak = peak_usage_.load();
        while (peak < current && !peak_usage_.compare_exchange_weak(peak, current)) {
            // Retry
        }
        return {ptr, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr))};
    }
    
    return {nullptr, 0};
}

bool PoolAllocator::deallocate(PoolType pool_type, uint64_t offset, size_t size) {
    uint32_t pool_idx = static_cast<uint32_t>(pool_type);
    if (pool_idx >= 4) {
        return false;
    }
    
    if (pools_[pool_idx].deallocate(offset, size)) {
        total_allocated_ -= size;
        return true;
    }
    
    return false;
}

PoolAllocator::AllocationStats PoolAllocator::get_statistics() const {
    AllocationStats stats;
    size_t total_allocated = 0;
    size_t total_available = 0;
    
    for (uint32_t i = 0; i < 4; ++i) {
        stats.pool_stats[i] = pools_[i].get_stats();
        total_allocated += stats.pool_stats[i].used_bytes;
        total_available += stats.pool_stats[i].total_size;
    }
    
    stats.total_allocated = total_allocated;
    stats.total_available = total_available;
    
    if (total_allocated == 0) {
        stats.overall_fragmentation = 0.0f;
    } else {
        float total_frag = 0.0f;
        for (uint32_t i = 0; i < 4; ++i) {
            total_frag += stats.pool_stats[i].fragmentation_percent;
        }
        stats.overall_fragmentation = total_frag / 4.0f;
    }
    
    return stats;
}

void PoolAllocator::defragment_all() {
    for (uint32_t i = 0; i < 4; ++i) {
        pools_[i].defragment();
    }
}

}  // namespace rendering_engine::resource_manager
