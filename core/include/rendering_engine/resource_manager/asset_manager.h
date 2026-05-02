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

#include "asset_types.h"
#include "memory_pool.h"
#include "gpu_allocator.h"
#include "../types.h"
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <future>
#include <vulkan/vulkan.h>

namespace rendering_engine::resource_manager {

/**
 * @brief Async asset loading manager with background threads
 * 
 * Provides non-blocking asset loading with:
 * - Background loader threads
 * - Priority-based loading queue
 * - Automatic GPU upload with staging buffers
 * - LRU cache with configurable size
 * - Async callbacks for completion notification
 * 
 * Usage:
 * ```cpp
 * AssetManager manager;
 * manager.initialize(device, allocator, 3);  // 3 loader threads
 * 
 * uint64_t request_id = manager.load_asset_async(
 *     asset_id, AssetType::TEXTURE_2D, LoadPriority::PRIORITY_HIGH
 * );
 * 
 * // Later, check if loaded or register callback
 * if (manager.is_asset_loaded(asset_id)) {
 *     auto texture = manager.get_texture(asset_id);
 * }
 * ```
 */
class AssetManager {
public:
    /**
     * @brief Initialize asset manager
     * @param device Vulkan device
     * @param physical_device Vulkan physical device
     * @param gpu_allocator Reference to GPU allocator
     * @param num_loader_threads Number of background loader threads
     * @param max_cache_size_mb Maximum cache size in MB
     */
    bool initialize(VkDevice device, VkPhysicalDevice physical_device,
                   GPUAllocator& gpu_allocator,
                   uint32_t num_loader_threads = 3,
                   size_t max_cache_size_mb = 1024);
    
    /**
     * @brief Shutdown asset manager and join all threads
     */
    void shutdown();
    
    /**
     * @brief Load asset asynchronously
     * @param asset_id Unique asset identifier
     * @param asset_type Type of asset to load
     * @param priority Loading priority
     * @param file_path Path to asset file
     * @return Unique request ID for tracking
     */
    uint64_t load_asset_async(AssetID asset_id, AssetType asset_type,
                             LoadPriority priority, 
                             const std::string& file_path);
    
    /**
     * @brief Load asset synchronously (blocking)
     * @return AssetID on success, 0 on failure
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
     * @brief Get texture asset
     */
    std::shared_ptr<Texture> get_texture(AssetID asset_id) const;
    
    /**
     * @brief Get mesh asset
     */
    std::shared_ptr<Mesh> get_mesh(AssetID asset_id) const;
    
    /**
     * @brief Get shader asset
     */
    std::shared_ptr<Shader> get_shader(AssetID asset_id) const;
    
    /**
     * @brief Get material asset
     */
    std::shared_ptr<Material> get_material(AssetID asset_id) const;
    
    /**
     * @brief Register a mesh from memory
     * @param vertices Vector of vertices
     * @param indices Vector of indices
     * @return AssetID of the registered mesh
     */
    AssetID register_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    /**
     * @brief Update GPU vertex buffer data for an existing mesh asset
     * @param asset_id Mesh asset ID
     * @param vertices New vertex positions
     * @return true if update succeeded
     */
    bool update_mesh_vertices(AssetID asset_id, const std::vector<Vertex>& vertices);
    
    /**
     * @brief Get or create default material
     * @return AssetID of the default material
     */
    AssetID get_or_create_default_material();
    
    /**
     * @brief Register completion callback for specific asset
     */
    void register_load_callback(AssetID asset_id, AssetLoadCallback callback);
    
    /**
     * @brief Update streaming context for LOD-based loading
     */
    void update_streaming_context(const StreamingContext& context);
    
    /**
     * @brief Get loading statistics
     */
    struct LoadingStats {
        uint32_t pending_requests;
        uint32_t loaded_assets;
        size_t cache_size_bytes;
        float cache_usage_percent;
        uint32_t successful_loads;
        uint32_t failed_loads;
        float average_load_time_ms;
    };
    
    LoadingStats get_statistics() const;
    
    /**
     * @brief Clear cache and free memory
     */
    void clear_cache();
    
    /**
     * @brief Evict LRU assets if cache exceeds limit
     * @return Number of assets evicted
     */
    uint32_t evict_lru_assets();

private:
    // Vulkan state
    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkCommandPool transfer_command_pool_{VK_NULL_HANDLE};
    VkQueue transfer_queue_{VK_NULL_HANDLE};
    uint32_t transfer_queue_family_index_{0};
    GPUAllocator* gpu_allocator_{nullptr};
    
    // Asset caches
    mutable std::shared_mutex texture_cache_mutex_;
    std::unordered_map<AssetID, std::shared_ptr<Texture>> texture_cache_;
    
    mutable std::shared_mutex mesh_cache_mutex_;
    std::unordered_map<AssetID, std::shared_ptr<Mesh>> mesh_cache_;
    
    mutable std::shared_mutex shader_cache_mutex_;
    std::unordered_map<AssetID, std::shared_ptr<Shader>> shader_cache_;
    
    mutable std::shared_mutex material_cache_mutex_;
    std::unordered_map<AssetID, std::shared_ptr<Material>> material_cache_;
    
    // Request queue and management
    std::priority_queue<AssetRequest, std::vector<AssetRequest>,
                       std::function<bool(const AssetRequest&, 
                                         const AssetRequest&)>> 
        request_queue_{[](const AssetRequest& a, const AssetRequest& b) {
            return a.priority > b.priority;
        }};
    mutable std::mutex request_mutex_;
    std::condition_variable request_cv_;
    
    // Loading threads
    std::vector<std::thread> loader_threads_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Statistics and state
    std::atomic<uint64_t> next_request_id_{1};
    std::atomic<size_t> cache_size_bytes_{0};
    size_t max_cache_size_bytes_{1024 * 1024 * 1024};
    std::atomic<uint32_t> successful_loads_{0};
    std::atomic<uint32_t> failed_loads_{0};
    
    // Callbacks
    std::unordered_map<AssetID, std::vector<AssetLoadCallback>> load_callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Streaming context
    StreamingContext streaming_context_;
    mutable std::mutex streaming_mutex_;
    
    /**
     * @brief Main loader thread function
     */
    void loader_thread_main();
    
    /**
     * @brief Process a single asset request
     */
    void process_asset_request(const AssetRequest& request);
    
    /**
     * @brief Load texture from file
     */
    std::shared_ptr<Texture> load_texture_from_file(
        AssetID asset_id, const std::string& file_path);
    
    /**
     * @brief Load mesh from file
     */
    std::shared_ptr<Mesh> load_mesh_from_file(
        AssetID asset_id, const std::string& file_path);
    
    /**
     * @brief Load shader from file
     */
    std::shared_ptr<Shader> load_shader_from_file(
        AssetID asset_id, const std::string& file_path);
    
    /**
     * @brief Upload texture to GPU
     */
    bool upload_texture_to_gpu(std::shared_ptr<Texture> texture);
    
    /**
     * @brief Upload mesh to GPU
     */
    bool upload_mesh_to_gpu(std::shared_ptr<Mesh> mesh);
    
    /**
     * @brief Invoke load callbacks for asset
     */
    void invoke_load_callbacks(AssetID asset_id, bool success);
    
    /**
     * @brief Check and perform LRU eviction if needed
     */
    void check_and_evict_if_needed(size_t additional_size);
};

}  // namespace rendering_engine::resource_manager
