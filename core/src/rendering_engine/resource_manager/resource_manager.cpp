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
#include "../../../include/rendering_engine/resource_manager/resource_manager.h"
#include <iostream>

namespace rendering_engine::resource_manager {

bool ResourceManager::initialize(const InitConfig& config) {
    // Initialize CPU allocator
    if (!cpu_allocator_.initialize(config.cpu_pool_sizes)) {
        std::cerr << "Failed to initialize CPU allocator" << std::endl;
        return false;
    }
    
    // Initialize GPU allocator
    if (!gpu_allocator_.initialize(config.device, config.physical_device,
                                   config.gpu_memory_budget_mb * 1024 * 1024)) {
        std::cerr << "Failed to initialize GPU allocator" << std::endl;
        return false;
    }
    
    // Initialize asset manager
    if (!asset_manager_.initialize(config.device, config.physical_device, gpu_allocator_,
                                  config.num_loader_threads,
                                  config.asset_cache_size_mb)) {
        std::cerr << "Failed to initialize asset manager" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

void ResourceManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    asset_manager_.shutdown();
    initialized_ = false;
}

std::pair<void*, uint64_t> ResourceManager::allocate_cpu_memory(
    PoolType pool_type, size_t size, uint32_t alignment) {
    
    if (!initialized_.load()) {
        return {nullptr, 0};
    }
    
    return cpu_allocator_.allocate(pool_type, size, alignment);
}

bool ResourceManager::deallocate_cpu_memory(PoolType pool_type, 
                                           uint64_t offset, size_t size) {
    if (!initialized_.load()) {
        return false;
    }
    
    return cpu_allocator_.deallocate(pool_type, offset, size);
}

std::optional<std::pair<uint64_t, uint64_t>> ResourceManager::allocate_gpu_memory(
    size_t size, size_t alignment, VkMemoryPropertyFlags properties) {
    
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    return gpu_allocator_.allocate(size, alignment, properties);
}

bool ResourceManager::deallocate_gpu_memory(uint64_t offset, size_t size) {
    if (!initialized_.load()) {
        return false;
    }
    
    return gpu_allocator_.deallocate(offset, size);
}

uint64_t ResourceManager::load_asset_async(AssetID asset_id, AssetType asset_type,
                                          LoadPriority priority, 
                                          const std::string& file_path) {
    if (!initialized_.load()) {
        return 0;
    }
    
    return asset_manager_.load_asset_async(asset_id, asset_type, priority, file_path);
}

AssetID ResourceManager::load_asset_sync(AssetID asset_id, AssetType asset_type,
                                        const std::string& file_path) {
    if (!initialized_.load()) {
        return 0;
    }
    
    return asset_manager_.load_asset_sync(asset_id, asset_type, file_path);
}

bool ResourceManager::unload_asset(AssetID asset_id) {
    if (!initialized_.load()) {
        return false;
    }
    
    return asset_manager_.unload_asset(asset_id);
}

bool ResourceManager::is_asset_loaded(AssetID asset_id) const {
    if (!initialized_.load()) {
        return false;
    }
    
    return asset_manager_.is_asset_loaded(asset_id);
}

std::shared_ptr<Texture> ResourceManager::get_texture(AssetID asset_id) const {
    if (!initialized_.load()) {
        return nullptr;
    }
    
    return asset_manager_.get_texture(asset_id);
}

std::shared_ptr<Mesh> ResourceManager::get_mesh(AssetID asset_id) const {
    if (!initialized_.load()) {
        return nullptr;
    }
    
    return asset_manager_.get_mesh(asset_id);
}

std::shared_ptr<Shader> ResourceManager::get_shader(AssetID asset_id) const {
    if (!initialized_.load()) {
        return nullptr;
    }
    
    return asset_manager_.get_shader(asset_id);
}

std::shared_ptr<Material> ResourceManager::get_material(AssetID asset_id) const {
    if (!initialized_.load()) {
        return nullptr;
    }
    
    return asset_manager_.get_material(asset_id);
}

void ResourceManager::register_load_callback(AssetID asset_id, 
                                            AssetLoadCallback callback) {
    if (!initialized_.load()) {
        return;
    }
    
    asset_manager_.register_load_callback(asset_id, callback);
}

void ResourceManager::update_streaming_context(const StreamingContext& context) {
    if (!initialized_.load()) {
        return;
    }
    
    asset_manager_.update_streaming_context(context);
}

ResourceManager::ResourceStats ResourceManager::get_statistics() const {
    ResourceStats stats;
    
    if (initialized_.load()) {
        stats.cpu_stats = cpu_allocator_.get_statistics();
        stats.gpu_stats = gpu_allocator_.get_statistics();
        stats.asset_stats = asset_manager_.get_statistics();
    }
    
    return stats;
}

void ResourceManager::print_debug_info() const {
    if (!initialized_.load()) {
        std::cout << "Resource Manager: NOT INITIALIZED" << std::endl;
        return;
    }
    
    auto stats = get_statistics();
    
    std::cout << "\n=== Resource Manager Statistics ===" << std::endl;
    
    std::cout << "\nCPU Memory Pools:" << std::endl;
    const char* pool_names[] = {"TEXTURE_POOL", "VERTEX_POOL", "UNIFORM_POOL", "SYSTEM_POOL"};
    for (uint32_t i = 0; i < 4; ++i) {
        const auto& pool_stat = stats.cpu_stats.pool_stats[i];
        std::cout << "  " << pool_names[i] << ":" << std::endl;
        std::cout << "    Total: " << (pool_stat.total_size / (1024.0 * 1024.0)) << " MB" << std::endl;
        std::cout << "    Used: " << (pool_stat.used_bytes / (1024.0 * 1024.0)) << " MB" << std::endl;
        std::cout << "    Fragmentation: " << pool_stat.fragmentation_percent << "%" << std::endl;
    }
    
    std::cout << "\nGPU Memory:" << std::endl;
    std::cout << "  Allocated: " << (stats.gpu_stats.total_allocated / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Available: " << (stats.gpu_stats.total_available / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Peak Usage: " << (stats.gpu_stats.peak_usage / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Fragmentation: " << stats.gpu_stats.fragmentation << "%" << std::endl;
    std::cout << "  Block Count: " << stats.gpu_stats.block_count << std::endl;
    
    std::cout << "\nAsset Manager:" << std::endl;
    std::cout << "  Loaded Assets: " << stats.asset_stats.loaded_assets << std::endl;
    std::cout << "  Pending Requests: " << stats.asset_stats.pending_requests << std::endl;
    std::cout << "  Cache Usage: " << (stats.asset_stats.cache_size_bytes / (1024.0 * 1024.0)) 
              << " MB (" << stats.asset_stats.cache_usage_percent << "%)" << std::endl;
    std::cout << "  Successful Loads: " << stats.asset_stats.successful_loads << std::endl;
    std::cout << "  Failed Loads: " << stats.asset_stats.failed_loads << std::endl;
    
    std::cout << "\n===================================" << std::endl;
}

void ResourceManager::clear_all_caches() {
    if (!initialized_.load()) {
        return;
    }
    
    asset_manager_.clear_cache();
}

ResourceManager::~ResourceManager() {
    if (initialized_.load()) {
        shutdown();
    }
}

}  // namespace rendering_engine::resource_manager
