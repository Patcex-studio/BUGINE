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

#include "memory_pool.h"
#include "gpu_allocator.h"
#include "asset_manager.h"
#include "asset_types.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace rendering_engine::resource_manager {

/**
 * @brief Main resource management system
 * 
 * Unified interface for all resource management operations:
 * - CPU memory pooling
 * - GPU memory allocation
 * - Async asset loading
 * - Asset caching and streaming
 * 
 * This is a singleton-like manager that should be initialized once
 * at engine startup and used throughout the application lifetime.
 * 
 * Example initialization:
 * ```cpp
 * ResourceManager& manager = ResourceManager::get_instance();
 * 
 * ResourceManager::InitConfig config;
 * config.device = vk_device;
 * config.physical_device = vk_physical_device;
 * config.cpu_memory_budget_mb = 512;
 * config.gpu_memory_budget_mb = 2048;
 * config.num_loader_threads = 4;
 * 
 * manager.initialize(config);
 * ```
 */
class ResourceManager {
public:
    /**
     * @brief Initialization configuration
     */
    struct InitConfig {
        VkDevice device;
        VkPhysicalDevice physical_device;
        
        // CPU memory pools (see PoolType enum)
        std::array<size_t, 4> cpu_pool_sizes = {
            128 * 1024 * 1024,   // TEXTURE_POOL: 128 MB
            256 * 1024 * 1024,   // VERTEX_POOL: 256 MB
            64 * 1024 * 1024,    // UNIFORM_POOL: 64 MB
            128 * 1024 * 1024    // SYSTEM_POOL: 128 MB
        };
        
        // GPU memory budget
        size_t gpu_memory_budget_mb = 2048;
        
        // Asset manager config
        uint32_t num_loader_threads = 3;
        size_t asset_cache_size_mb = 1024;
    };
    
    /**
     * @brief Get resource manager singleton instance
     */
    static ResourceManager& get_instance() {
        static ResourceManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize resource manager
     * @param config Initialization configuration
     * @return true on success
     */
    bool initialize(const InitConfig& config);
    
    /**
     * @brief Shutdown resource manager
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool is_initialized() const { return initialized_.load(); }
    
    // =============================
    // CPU Memory Pool Interface
    // =============================
    
    /**
     * @brief Allocate from CPU memory pool
     */
    std::pair<void*, uint64_t> allocate_cpu_memory(
        PoolType pool_type, size_t size, uint32_t alignment = 256);
    
    /**
     * @brief Deallocate from CPU memory pool
     */
    bool deallocate_cpu_memory(PoolType pool_type, uint64_t offset, size_t size);
    
    /**
     * @brief Get CPU pool allocator
     */
    PoolAllocator& get_cpu_allocator() { return cpu_allocator_; }
    
    // =============================
    // GPU Memory Interface
    // =============================
    
    /**
     * @brief Allocate from GPU memory
     */
    std::optional<std::pair<uint64_t, uint64_t>> allocate_gpu_memory(
        size_t size, size_t alignment = 256,
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    /**
     * @brief Deallocate from GPU memory
     */
    bool deallocate_gpu_memory(uint64_t offset, size_t size);
    
    /**
     * @brief Get GPU allocator
     */
    GPUAllocator& get_gpu_allocator() { return gpu_allocator_; }
    
    // =============================
    // Asset Management Interface
    // =============================
    
    /**
     * @brief Load asset asynchronously
     */
    uint64_t load_asset_async(AssetID asset_id, AssetType asset_type,
                             LoadPriority priority, 
                             const std::string& file_path);
    
    /**
     * @brief Load asset synchronously
     */
    AssetID load_asset_sync(AssetID asset_id, AssetType asset_type,
                           const std::string& file_path);
    
    /**
     * @brief Unload asset from cache
     */
    bool unload_asset(AssetID asset_id);
    
    /**
     * @brief Check if asset is loaded
     */
    bool is_asset_loaded(AssetID asset_id) const;
    
    /**
     * @brief Get cached texture
     */
    std::shared_ptr<Texture> get_texture(AssetID asset_id) const;
    
    /**
     * @brief Get cached mesh
     */
    std::shared_ptr<Mesh> get_mesh(AssetID asset_id) const;
    
    /**
     * @brief Get cached shader
     */
    std::shared_ptr<Shader> get_shader(AssetID asset_id) const;
    
    /**
     * @brief Get cached material
     */
    std::shared_ptr<Material> get_material(AssetID asset_id) const;
    
    /**
     * @brief Register load callback
     */
    void register_load_callback(AssetID asset_id, AssetLoadCallback callback);
    
    /**
     * @brief Update streaming context for LOD-based loading
     */
    void update_streaming_context(const StreamingContext& context);
    
    /**
     * @brief Get asset manager
     */
    AssetManager& get_asset_manager() { return asset_manager_; }
    
    // =============================
    // Statistics and Diagnostics
    // =============================
    
    /**
     * @brief Resource management statistics
     */
    struct ResourceStats {
        PoolAllocator::AllocationStats cpu_stats;
        GPUAllocator::GPUMemoryStats gpu_stats;
        AssetManager::LoadingStats asset_stats;
    };
    
    /**
     * @brief Get comprehensive resource statistics
     */
    ResourceStats get_statistics() const;
    
    /**
     * @brief Print debug information
     */
    void print_debug_info() const;
    
    /**
     * @brief Clear all caches
     */
    void clear_all_caches();

private:
    ResourceManager() = default;
    ~ResourceManager();
    
    // Copy and move operations disabled
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;
    
    // State
    std::atomic<bool> initialized_{false};
    
    // Components
    PoolAllocator cpu_allocator_;
    GPUAllocator gpu_allocator_;
    AssetManager asset_manager_;
};

}  // namespace rendering_engine::resource_manager
