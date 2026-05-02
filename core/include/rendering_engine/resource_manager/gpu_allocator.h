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

#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <optional>
#include <unordered_map>
#include <atomic>
#include <cstdint>

namespace rendering_engine::resource_manager {

/**
 * @brief GPU memory block size categories
 */
enum class GPUBlockSize : uint32_t {
    SMALL = 0,     // 16 MB
    MEDIUM = 1,    // 64 MB
    LARGE = 2      // 256 MB
};

/**
 * @brief Sub-allocation within a GPU memory block
 */
struct GPUSubAllocation {
    uint64_t offset;                     // Offset within block
    size_t size;                         // Size of allocation
    uint64_t handle;                     // Unique handle
    bool is_free;                        // Is this region free
    std::chrono::steady_clock::time_point access_time;
    
    GPUSubAllocation(uint64_t off = 0, size_t sz = 0, uint64_t h = 0,
                     bool free = true)
        : offset(off), size(sz), handle(h), is_free(free) {
        access_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Represents a contiguous GPU memory block
 * 
 * Each GPU memory block is allocated from Vulkan device memory and
 * divided into sub-allocations for efficient memory management.
 */
class GPUMemoryBlock {
public:
    /**
     * @brief Initialize GPU memory block
     */
    bool initialize(VkDevice device, uint32_t memory_type_index, 
                   size_t block_size);
    
    /**
     * @brief Allocate from this block
     * @return Pair of (offset, handle) on success, (-1, 0) on failure
     */
    std::optional<std::pair<uint64_t, uint64_t>> allocate(size_t size, 
                                                          size_t alignment = 256);
    
    /**
     * @brief Deallocate from this block
     * @return Handle of freed allocation on success, std::nullopt on failure
     */
    std::optional<uint64_t> deallocate(uint64_t offset, size_t size);
    
    /**
     * @brief Get available memory in block
     */
    size_t available_memory() const;
    
    /**
     * @brief Get used memory in block
     */
    size_t used_memory() const;
    
    /**
     * @brief Check if block has space for allocation
     */
    bool can_fit(size_t size, size_t alignment = 256) const;
    
    /**
     * @brief Defragment block by coalescing free regions
     * @return Number of regions coalesced
     */
    uint32_t defragment();
    
    /**
     * @brief Get Vulkan device memory handle
     */
    VkDeviceMemory get_vk_memory() const { return device_memory_; }
    
    /**
     * @brief Destructor - frees GPU memory
     */
    ~GPUMemoryBlock();

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkDeviceMemory device_memory_{VK_NULL_HANDLE};
    size_t block_size_{0};
    std::vector<GPUSubAllocation> sub_allocations_;
    mutable std::mutex block_mutex_;
    uint64_t next_handle_{1};
    
    /**
     * @brief Find best-fit free region for allocation
     */
    std::optional<size_t> find_best_fit(size_t size, size_t alignment);
    
    /**
     * @brief Align offset to required alignment
     */
    static size_t align_offset(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }
};

/**
 * @brief GPU memory allocator managing multiple blocks
 * 
 * Provides efficient GPU memory allocation with support for:
 * - Multiple block size categories (small, medium, large)
 * - Automatic defragmentation
 * - Multi-GPU support
 * - Memory budgeting and limits
 */
class GPUAllocator {
public:
    /**
     * @brief Initialize GPU allocator
     * @param device Vulkan device
     * @param physical_device Vulkan physical device
     * @param memory_budget Maximum GPU memory to use
     */
    bool initialize(VkDevice device, VkPhysicalDevice physical_device,
                   size_t memory_budget = 2ull * 1024 * 1024 * 1024);  // 2 GB
    
    /**
     * @brief Allocate GPU memory
     * @param size Size to allocate
     * @param alignment Required alignment
     * @param memory_type_filter VkMemoryPropertyFlags to filter memory types
     * @return Pair of (offset, handle), or (0, 0) on failure
     */
    std::optional<std::pair<uint64_t, uint64_t>> allocate(
        size_t size, 
        size_t alignment = 256,
        VkMemoryPropertyFlags memory_properties = 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    /**
     * @brief Deallocate GPU memory
     * @return true on success
     */
    bool deallocate(uint64_t offset, size_t size);
    
    /**
     * @brief Get GPU memory statistics
     */
    struct GPUMemoryStats {
        size_t total_allocated;
        size_t total_available;
        size_t peak_usage;
        uint32_t block_count;
        float fragmentation;
        std::array<size_t, 3> block_usage;  // Usage per block size category
    };
    
    GPUMemoryStats get_statistics() const;
    
    /**
     * @brief Defragment all GPU memory blocks
     */
    void defragment_all();
    
    /**
     * @brief Check if memory allocation fits within budget
     */
    bool fits_in_budget(size_t size) const;
    
    /**
     * @brief Get VkDeviceMemory for a given handle
     * @param handle Handle returned by allocate
     * @return VkDeviceMemory or VK_NULL_HANDLE if invalid
     */
    VkDeviceMemory get_device_memory(uint64_t handle) const;

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    size_t memory_budget_{2ull * 1024 * 1024 * 1024};
    
    // Memory blocks organized by size category
    std::array<std::vector<std::unique_ptr<GPUMemoryBlock>>, 3> gpu_blocks_;
    std::array<size_t, 3> block_sizes_ = {
        16 * 1024 * 1024,    // SMALL: 16 MB
        64 * 1024 * 1024,    // MEDIUM: 64 MB
        256 * 1024 * 1024    // LARGE: 256 MB
    };
    
    std::atomic<uint64_t> current_block_id_{0};
    std::atomic<size_t> current_usage_{0};
    std::atomic<size_t> peak_usage_{0};
    mutable std::mutex allocator_mutex_;
    std::unordered_map<uint64_t, std::pair<uint32_t, size_t>> handle_to_block_idx_;
    
    /**
     * @brief Select appropriate block size category for allocation
     */
    GPUBlockSize select_block_size(size_t size) const;
    
    /**
     * @brief Find block with space for allocation
     * @return Block index and allocation result
     */
    std::optional<std::pair<size_t, std::pair<uint64_t, uint64_t>>> 
    find_block_for_allocation(GPUBlockSize block_size, size_t size,
                             size_t alignment);
    
    /**
     * @brief Create new GPU memory block
     */
    bool create_new_block(GPUBlockSize block_size, 
                         VkMemoryPropertyFlags properties);
    
    /**
     * @brief Get Vulkan memory type index
     */
    std::optional<uint32_t> find_memory_type(
        uint32_t type_filter,
        VkMemoryPropertyFlags properties);
};

}  // namespace rendering_engine::resource_manager
