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
#include <cstring>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <optional>
#include <atomic>
#include <array>

namespace rendering_engine::resource_manager {

/**
 * @brief Pool type enumeration for different memory allocation categories
 */
enum class PoolType : uint32_t {
    TEXTURE_POOL = 0,        // GPU texture memory
    VERTEX_POOL = 1,         // Vertex/index buffer memory
    UNIFORM_POOL = 2,        // Uniform buffer memory
    SYSTEM_POOL = 3,         // CPU-side temporary allocations
    MAX_POOL_TYPES = 4
};

/**
 * @brief Metadata for individual allocation within a pool
 */
struct AllocationInfo {
    uint64_t offset;                     // Offset within pool
    size_t size;                         // Allocated size
    uint64_t handle;                     // Unique allocation handle
    bool is_active;                      // Whether allocation is currently in use
    uint64_t access_time;                // Timestamp of last access (for LRU)
    uint32_t alignment;                  // Required alignment
    
    AllocationInfo() : offset(0), size(0), handle(0), is_active(false), 
                       access_time(0), alignment(256) {}
    
    AllocationInfo(uint64_t off, size_t sz, uint64_t h, uint32_t align)
        : offset(off), size(sz), handle(h), is_active(true), 
          access_time(0), alignment(align) {}
};

/**
 * @brief Free memory region for allocation tracking
 */
struct FreeBlock {
    uint64_t offset;
    size_t size;
    
    FreeBlock(uint64_t off = 0, size_t sz = 0) : offset(off), size(sz) {}
    
    bool operator>(const FreeBlock& other) const {
        return size < other.size;  // Min-heap for best-fit
    }
};

/**
 * @brief Thread-safe memory pool for allocations
 * 
 * Implements best-fit allocation strategy with automatic coalescing
 * of adjacent free blocks. Supports lock-free reads for frequent operations.
 */
class MemoryPool {
public:
    /**
     * @brief Initialize memory pool with given size and alignment
     * @param size_bytes Total size of the pool in bytes
     * @param alignment Required alignment for all allocations (default: 256)
     * @return true on success, false if allocation failed
     */
    bool initialize(size_t size_bytes, uint32_t alignment = 256);
    
    /**
     * @brief Allocate memory from pool
     * @param size Size to allocate
     * @param alignment Specific alignment requirement (uses pool default if 0)
     * @return Pointer to allocated memory, nullptr if allocation failed
     */
    void* allocate(size_t size, uint32_t alignment = 0);
    
    /**
     * @brief Deallocate memory back to pool
     * @param offset Offset of allocation (from allocate return value)
     * @param size Size of allocation
     * @return true on success, false if deallocation failed
     */
    bool deallocate(uint64_t offset, size_t size);
    
    /**
     * @brief Get current pool statistics
     */
    struct PoolStats {
        size_t total_size;
        size_t used_bytes;
        size_t free_bytes;
        size_t fragmentation_percent;
        uint32_t active_allocations;
        uint32_t free_blocks_count;
    };
    
    PoolStats get_stats() const;
    
    /**
     * @brief Defragment pool by coalescing adjacent free blocks
     * @return Number of blocks coalesced
     */
    uint32_t defragment();
    
    /**
     * @brief Check if pool is locked (prevents resizing)
     */
    bool is_locked() const { return is_locked_.load(); }
    
    /**
     * @brief Lock pool to prevent resizing during operations
     */
    void lock() { is_locked_.store(true); }
    
    /**
     * @brief Unlock pool to allow resizing
     */
    void unlock() { is_locked_.store(false); }
    
    /**
     * @brief Get raw memory block pointer
     */
    void* get_memory_block() const { return memory_block_.get(); }
    
    /**
     * @brief Check if memory address is within this pool
     */
    bool contains(const void* ptr) const;
    
    /**
     * @brief Destructor - frees allocated memory
     */
    ~MemoryPool();

private:
    std::unique_ptr<char[]> memory_block_;               // Main memory block
    size_t block_size_{0};                              // Total size
    std::atomic<size_t> used_bytes_{0};                 // Currently allocated
    std::vector<AllocationInfo> allocations_;           // Active allocations  
    std::priority_queue<FreeBlock, std::vector<FreeBlock>, 
                        std::greater<FreeBlock>> free_blocks_; // Free regions
    mutable std::mutex pool_mutex_;                      // Thread safety
    std::atomic<bool> is_locked_{false};                // Resize lock
    uint32_t alignment_{256};                           // Default alignment
    uint64_t next_handle_{1};                           // Handle generator
    
    /**
     * @brief Find best-fit free block for allocation
     * @return Iterator to best-fit block, or end() if not found
     */
    std::optional<FreeBlock> find_best_fit(size_t size, uint32_t alignment);
    
    /**
     * @brief Align offset to required alignment
     */
    static uint64_t align_offset(uint64_t offset, uint32_t alignment) {
        return (offset + alignment - 1) & ~(static_cast<uint64_t>(alignment) - 1);
    }
    
    /**
     * @brief Calculate fragmentation percentage
     */
    float calculate_fragmentation() const;
};

/**
 * @brief Pool allocator with configurable per-type sizes
 * 
 * Manages multiple pools for different types of allocations.
 * Provides global statistics and LRU eviction policy.
 */
class PoolAllocator {
public:
    /**
     * @brief Initialize all pools with specified sizes
     * @param pool_config Size configuration for each pool type
     */
    bool initialize(const std::array<size_t, 4>& pool_config);
    
    /**
     * @brief Allocate from appropriate pool based on type
     * @param pool_type Type of pool to allocate from
     * @param size Size to allocate
     * @param alignment Alignment requirement
     * @return Pair of (memory_ptr, unique allocation handle)
     */
    std::pair<void*, uint64_t> allocate(PoolType pool_type, size_t size, 
                                        uint32_t alignment = 256);
    
    /**
     * @brief Deallocate from pool
     * @param pool_type Type of pool
     * @param offset Offset returned by allocate
     * @param size Size to deallocate
     */
    bool deallocate(PoolType pool_type, uint64_t offset, size_t size);
    
    /**
     * @brief Combined statistics for all pools
     */
    struct AllocationStats {
        std::array<MemoryPool::PoolStats, 4> pool_stats;
        size_t total_allocated;
        size_t total_available;
        float overall_fragmentation;
    };
    
    AllocationStats get_statistics() const;
    
    /**
     * @brief Defragment all pools
     */
    void defragment_all();
    
    /**
     * @brief Get specific pool reference
     */
    MemoryPool& get_pool(PoolType type) { 
        return pools_[static_cast<uint32_t>(type)]; 
    }

private:
    std::array<MemoryPool, static_cast<uint32_t>(PoolType::MAX_POOL_TYPES)> pools_;
    std::atomic<size_t> total_allocated_{0};
    std::atomic<size_t> peak_usage_{0};
};

}  // namespace rendering_engine::resource_manager
