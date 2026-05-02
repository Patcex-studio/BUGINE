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
#include "../../../include/rendering_engine/resource_manager/gpu_allocator.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rendering_engine::resource_manager {

bool GPUMemoryBlock::initialize(VkDevice device, uint32_t memory_type_index,
                                 size_t block_size) {
    device_ = device;
    block_size_ = block_size;
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.allocationSize = block_size;
    alloc_info.memoryTypeIndex = memory_type_index;
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &device_memory_) != VK_SUCCESS) {
        return false;
    }
    
    // Initialize with single free block covering entire allocation
    sub_allocations_.emplace_back(0, block_size, next_handle_++, true);
    
    return true;
}

std::optional<std::pair<uint64_t, uint64_t>> GPUMemoryBlock::allocate(size_t size,
                                                                        size_t alignment) {
    if (size == 0 || size > block_size_) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    // Find best-fit free region
    auto best_idx = find_best_fit(size, alignment);
    if (!best_idx.has_value()) {
        return std::nullopt;
    }
    
    size_t idx = best_idx.value();
    GPUSubAllocation& free_block = sub_allocations_[idx];
    
    uint64_t aligned_offset = align_offset(free_block.offset, alignment);
    uint64_t padding = aligned_offset - free_block.offset;
    
    // Check if block is large enough after alignment
    if (aligned_offset + size > free_block.offset + free_block.size) {
        return std::nullopt;
    }
    
    uint64_t handle = next_handle_++;
    
    // Create allocation
    GPUSubAllocation allocation(aligned_offset, size, handle, false);
    allocation.access_time = std::chrono::steady_clock::now();
    
    // Update or split free block
    uint64_t remaining_offset = aligned_offset + size;
    size_t remaining_size = (free_block.offset + free_block.size) - remaining_offset;
    
    free_block.offset = aligned_offset;
    free_block.size = size;
    free_block.is_free = false;
    free_block.handle = handle;
    
    if (remaining_size > 0) {
        sub_allocations_.emplace_back(remaining_offset, remaining_size, 0, true);
    }
    
    return std::make_pair(aligned_offset, handle);
}

std::optional<uint64_t> GPUMemoryBlock::deallocate(uint64_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    // Find allocation and mark as free
    auto it = std::find_if(sub_allocations_.begin(), sub_allocations_.end(),
        [offset](const GPUSubAllocation& sub) {
            return sub.offset == offset && !sub.is_free;
        });
    
    if (it == sub_allocations_.end()) {
        return std::nullopt;
    }
    
    uint64_t handle = it->handle;
    it->is_free = true;
    it->handle = 0;
    
    return handle;
}

size_t GPUMemoryBlock::available_memory() const {
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    size_t available = 0;
    for (const auto& sub : sub_allocations_) {
        if (sub.is_free) {
            available += sub.size;
        }
    }
    return available;
}

size_t GPUMemoryBlock::used_memory() const {
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    size_t used = 0;
    for (const auto& sub : sub_allocations_) {
        if (!sub.is_free) {
            used += sub.size;
        }
    }
    return used;
}

bool GPUMemoryBlock::can_fit(size_t size, size_t alignment) const {
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    for (const auto& sub : sub_allocations_) {
        if (sub.is_free) {
            uint64_t aligned_offset = align_offset(sub.offset, alignment);
            if (aligned_offset + size <= sub.offset + sub.size) {
                return true;
            }
        }
    }
    return false;
}

uint32_t GPUMemoryBlock::defragment() {
    std::lock_guard<std::mutex> lock(block_mutex_);
    
    uint32_t coalesced = 0;
    
    // Sort by offset
    std::sort(sub_allocations_.begin(), sub_allocations_.end(),
        [](const GPUSubAllocation& a, const GPUSubAllocation& b) {
            return a.offset < b.offset;
        });
    
    // Coalesce adjacent free blocks
    for (size_t i = 0; i < sub_allocations_.size() - 1; ++i) {
        if (sub_allocations_[i].is_free && sub_allocations_[i + 1].is_free) {
            sub_allocations_[i].size += sub_allocations_[i + 1].size;
            sub_allocations_.erase(sub_allocations_.begin() + i + 1);
            coalesced++;
            i--;  // Re-check this position
        }
    }
    
    return coalesced;
}

GPUMemoryBlock::~GPUMemoryBlock() {
    if (device_ != VK_NULL_HANDLE && device_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, device_memory_, nullptr);
    }
}

std::optional<size_t> GPUMemoryBlock::find_best_fit(size_t size, size_t alignment) {
    size_t best_idx = std::numeric_limits<size_t>::max();
    size_t best_size = std::numeric_limits<size_t>::max();
    
    for (size_t i = 0; i < sub_allocations_.size(); ++i) {
        const auto& sub = sub_allocations_[i];
        if (!sub.is_free) {
            continue;
        }
        
        uint64_t aligned_offset = align_offset(sub.offset, alignment);
        if (aligned_offset + size <= sub.offset + sub.size) {
            if (sub.size < best_size) {
                best_idx = i;
                best_size = sub.size;
            }
        }
    }
    
    if (best_idx == std::numeric_limits<size_t>::max()) {
        return std::nullopt;
    }
    
    return best_idx;
}

// ============================================================================
// GPUAllocator Implementation
// ============================================================================

bool GPUAllocator::initialize(VkDevice device, VkPhysicalDevice physical_device,
                              size_t memory_budget) {
    device_ = device;
    physical_device_ = physical_device;
    memory_budget_ = memory_budget;
    
    return true;
}

std::optional<std::pair<uint64_t, uint64_t>> GPUAllocator::allocate(
    size_t size, 
    size_t alignment,
    VkMemoryPropertyFlags memory_properties) {
    
    std::lock_guard<std::mutex> lock(allocator_mutex_);
    
    if (size == 0 || current_usage_.load() + size > memory_budget_) {
        return std::nullopt;
    }
    
    GPUBlockSize block_size = select_block_size(size);
    
    // Try to find existing block with space
    auto result = find_block_for_allocation(block_size, size, alignment);
    if (result.has_value()) {
        auto [local_block_idx, alloc] = result.value();
        uint64_t composite_handle = (static_cast<uint64_t>(block_size) << 56) |
            (static_cast<uint64_t>(local_block_idx) << 48) |
            alloc.second;
        handle_to_block_idx_[composite_handle] = {static_cast<uint32_t>(block_size), local_block_idx};
        current_usage_ += size;
        
        size_t peak = peak_usage_.load();
        size_t current = current_usage_.load();
        while (peak < current && !peak_usage_.compare_exchange_weak(peak, current)) {
            // Retry
        }
        
        return std::make_pair(alloc.first, composite_handle);
    }
    
    // Create new block if we have budget
    if (!create_new_block(block_size, memory_properties)) {
        return std::nullopt;
    }
    
    // Try allocation again
    result = find_block_for_allocation(block_size, size, alignment);
    if (result.has_value()) {
        auto [local_block_idx, alloc] = result.value();
        uint64_t composite_handle = (static_cast<uint64_t>(block_size) << 56) |
            (static_cast<uint64_t>(local_block_idx) << 48) |
            alloc.second;
        handle_to_block_idx_[composite_handle] = {static_cast<uint32_t>(block_size), local_block_idx};
        current_usage_ += size;
        
        size_t peak = peak_usage_.load();
        size_t current = current_usage_.load();
        while (peak < current && !peak_usage_.compare_exchange_weak(peak, current)) {
            // Retry
        }
        
        return std::make_pair(alloc.first, composite_handle);
    }
    
    return std::nullopt;
}

bool GPUAllocator::deallocate(uint64_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(allocator_mutex_);
    
    for (auto& block_vec : gpu_blocks_) {
        for (auto& block : block_vec) {
            auto freed_handle = block->deallocate(offset, size);
            if (freed_handle.has_value()) {
                current_usage_ -= size;
                handle_to_block_idx_.erase(freed_handle.value());
                return true;
            }
        }
    }
    
    return false;
}

GPUAllocator::GPUMemoryStats GPUAllocator::get_statistics() const {
    std::lock_guard<std::mutex> lock(allocator_mutex_);
    
    GPUMemoryStats stats{};
    stats.total_allocated = current_usage_.load();
    stats.total_available = memory_budget_ - current_usage_.load();
    stats.peak_usage = peak_usage_.load();
    stats.block_count = 0;
    
    for (uint32_t i = 0; i < 3; ++i) {
        stats.block_count += gpu_blocks_[i].size();
        for (const auto& block : gpu_blocks_[i]) {
            stats.block_usage[i] += block->used_memory();
        }
    }
    
    // Calculate fragmentation
    size_t total_waste = 0;
    for (const auto& block_vec : gpu_blocks_) {
        for (const auto& block : block_vec) {
            // Fragmentation is free space that's split up
            total_waste += block->available_memory();
        }
    }
    
    if (stats.total_allocated > 0) {
        stats.fragmentation = (100.0f * total_waste) / memory_budget_;
    }
    
    return stats;
}

void GPUAllocator::defragment_all() {
    std::lock_guard<std::mutex> lock(allocator_mutex_);
    
    for (auto& block_vec : gpu_blocks_) {
        for (auto& block : block_vec) {
            block->defragment();
        }
    }
}

bool GPUAllocator::fits_in_budget(size_t size) const {
    return current_usage_.load() + size <= memory_budget_;
}

GPUBlockSize GPUAllocator::select_block_size(size_t size) const {
    if (size <= block_sizes_[0]) {
        return GPUBlockSize::SMALL;
    } else if (size <= block_sizes_[1]) {
        return GPUBlockSize::MEDIUM;
    } else {
        return GPUBlockSize::LARGE;
    }
}

std::optional<std::pair<size_t, std::pair<uint64_t, uint64_t>>> 
GPUAllocator::find_block_for_allocation(GPUBlockSize block_size, size_t size,
                                        size_t alignment) {
    auto block_idx = static_cast<uint32_t>(block_size);
    
    for (size_t i = 0; i < gpu_blocks_[block_idx].size(); ++i) {
        auto result = gpu_blocks_[block_idx][i]->allocate(size, alignment);
        if (result.has_value()) {
            return std::make_pair(i, result.value());
        }
    }
    
    return std::nullopt;
}

bool GPUAllocator::create_new_block(GPUBlockSize block_size,
                                    VkMemoryPropertyFlags properties) {
    auto block_idx = static_cast<uint32_t>(block_size);
    size_t block_physical_size = block_sizes_[block_idx];
    
    // Check if we can allocate more blocks
    if (current_usage_.load() + block_physical_size > memory_budget_) {
        return false;
    }
    
    // Find memory type
    auto memory_type = find_memory_type(~0U, properties);
    if (!memory_type.has_value()) {
        return false;
    }
    
    auto new_block = std::make_unique<GPUMemoryBlock>();
    if (!new_block->initialize(device_, memory_type.value(), block_physical_size)) {
        return false;
    }
    
    gpu_blocks_[block_idx].push_back(std::move(new_block));
    return true;
}

std::optional<uint32_t> GPUAllocator::find_memory_type(
    uint32_t type_filter,
    VkMemoryPropertyFlags properties) {
    
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return std::nullopt;
}

VkDeviceMemory GPUAllocator::get_device_memory(uint64_t handle) const {
    std::lock_guard<std::mutex> lock(allocator_mutex_);
    
    auto it = handle_to_block_idx_.find(handle);
    if (it == handle_to_block_idx_.end()) {
        return VK_NULL_HANDLE;
    }
    
    auto [category, local_idx] = it->second;
    return gpu_blocks_[category][local_idx]->get_vk_memory();
}

}  // namespace rendering_engine::resource_manager
