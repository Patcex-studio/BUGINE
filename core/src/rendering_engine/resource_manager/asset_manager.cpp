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
#include "../../../include/rendering_engine/resource_manager/asset_manager.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>
#include <array>

// Include stb_image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "../../../../third_party/stb_image.h"

namespace rendering_engine::resource_manager {

bool AssetManager::initialize(VkDevice device, VkPhysicalDevice physical_device,
                             GPUAllocator& gpu_allocator,
                             uint32_t num_loader_threads,
                             size_t max_cache_size_mb) {
    device_ = device;
    physical_device_ = physical_device;
    gpu_allocator_ = &gpu_allocator;
    max_cache_size_bytes_ = max_cache_size_mb * 1024 * 1024;
    shutdown_requested_ = false;
    
    // Find transfer queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());
    
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            transfer_queue_family_index_ = i;
            break;
        }
    }
    
    // Create transfer command pool
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = transfer_queue_family_index_;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(device_, &pool_info, nullptr, &transfer_command_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create transfer command pool" << std::endl;
        return false;
    }
    
    // Get transfer queue
    vkGetDeviceQueue(device_, transfer_queue_family_index_, 0, &transfer_queue_);
    
    // Start loader threads
    for (uint32_t i = 0; i < num_loader_threads; ++i) {
        try {
            loader_threads_.emplace_back(&AssetManager::loader_thread_main, this);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create loader thread: " << e.what() << std::endl;
            shutdown();
            return false;
        }
    }
    
    return true;
}

void AssetManager::shutdown() {
    shutdown_requested_ = true;
    request_cv_.notify_all();
    
    for (auto& thread : loader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    loader_threads_.clear();
    
    if (transfer_command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, transfer_command_pool_, nullptr);
        transfer_command_pool_ = VK_NULL_HANDLE;
    }
}

uint64_t AssetManager::load_asset_async(AssetID asset_id, AssetType asset_type,
                                       LoadPriority priority, 
                                       const std::string& file_path) {
    if (is_asset_loaded(asset_id)) {
        return next_request_id_.fetch_add(1);
    }
    
    AssetRequest request;
    request.request_id = next_request_id_.fetch_add(1);
    request.asset_id = asset_id;
    request.asset_type = asset_type;
    request.file_path = file_path;
    request.priority = priority;
    request.created_time = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(request_mutex_);
        request_queue_.push(request);
    }
    
    request_cv_.notify_one();
    return request.request_id;
}

AssetID AssetManager::load_asset_sync(AssetID asset_id, AssetType asset_type,
                                     const std::string& file_path) {
    if (is_asset_loaded(asset_id)) {
        return asset_id;
    }
    
    AssetRequest request;
    request.request_id = 0;
    request.asset_id = asset_id;
    request.asset_type = asset_type;
    request.file_path = file_path;
    request.priority = LoadPriority::PRIORITY_CRITICAL;
    request.async_only = false;
    
    process_asset_request(request);
    return is_asset_loaded(asset_id) ? asset_id : 0;
}

bool AssetManager::unload_asset(AssetID asset_id) {
    // Remove from caches
    {
        std::lock_guard<std::shared_mutex> lock(texture_cache_mutex_);
        auto it = texture_cache_.find(asset_id);
        if (it != texture_cache_.end()) {
            size_t size = it->second->memory_size;
            texture_cache_.erase(it);
            cache_size_bytes_ -= size;
            return true;
        }
    }
    
    {
        std::lock_guard<std::shared_mutex> lock(mesh_cache_mutex_);
        auto it = mesh_cache_.find(asset_id);
        if (it != mesh_cache_.end()) {
            size_t size = it->second->vertex_buffer_size + it->second->index_buffer_size;
            mesh_cache_.erase(it);
            cache_size_bytes_ -= size;
            return true;
        }
    }
    
    {
        std::lock_guard<std::shared_mutex> lock(shader_cache_mutex_);
        auto it = shader_cache_.find(asset_id);
        if (it != shader_cache_.end()) {
            shader_cache_.erase(it);
            return true;
        }
    }
    
    {
        std::lock_guard<std::shared_mutex> lock(material_cache_mutex_);
        auto it = material_cache_.find(asset_id);
        if (it != material_cache_.end()) {
            material_cache_.erase(it);
            return true;
        }
    }
    
    return false;
}

bool AssetManager::is_asset_loaded(AssetID asset_id) const {
    {
        std::shared_lock<std::shared_mutex> lock(texture_cache_mutex_);
        if (texture_cache_.find(asset_id) != texture_cache_.end()) {
            return true;
        }
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(mesh_cache_mutex_);
        if (mesh_cache_.find(asset_id) != mesh_cache_.end()) {
            return true;
        }
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(shader_cache_mutex_);
        if (shader_cache_.find(asset_id) != shader_cache_.end()) {
            return true;
        }
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(material_cache_mutex_);
        if (material_cache_.find(asset_id) != material_cache_.end()) {
            return true;
        }
    }
    
    return false;
}

std::shared_ptr<Texture> AssetManager::get_texture(AssetID asset_id) const {
    std::shared_lock<std::shared_mutex> lock(texture_cache_mutex_);
    auto it = texture_cache_.find(asset_id);
    return it != texture_cache_.end() ? it->second : nullptr;
}

std::shared_ptr<Mesh> AssetManager::get_mesh(AssetID asset_id) const {
    std::shared_lock<std::shared_mutex> lock(mesh_cache_mutex_);
    auto it = mesh_cache_.find(asset_id);
    return it != mesh_cache_.end() ? it->second : nullptr;
}

std::shared_ptr<Shader> AssetManager::get_shader(AssetID asset_id) const {
    std::shared_lock<std::shared_mutex> lock(shader_cache_mutex_);
    auto it = shader_cache_.find(asset_id);
    return it != shader_cache_.end() ? it->second : nullptr;
}

std::shared_ptr<Material> AssetManager::get_material(AssetID asset_id) const {
    std::shared_lock<std::shared_mutex> lock(material_cache_mutex_);
    auto it = material_cache_.find(asset_id);
    return it != material_cache_.end() ? it->second : nullptr;
}

void AssetManager::register_load_callback(AssetID asset_id, AssetLoadCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    load_callbacks_[asset_id].push_back(callback);
}

void AssetManager::update_streaming_context(const StreamingContext& context) {
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    streaming_context_ = context;
}

AssetManager::LoadingStats AssetManager::get_statistics() const {
    LoadingStats stats;
    stats.pending_requests = request_queue_.size();
    stats.cache_size_bytes = cache_size_bytes_;
    stats.cache_usage_percent = (100.0f * cache_size_bytes_) / max_cache_size_bytes_;
    stats.successful_loads = successful_loads_;
    stats.failed_loads = failed_loads_;
    
    // Calculate loaded assets count
    stats.loaded_assets = 0;
    {
        std::shared_lock<std::shared_mutex> lock(texture_cache_mutex_);
        stats.loaded_assets += texture_cache_.size();
    }
    {
        std::shared_lock<std::shared_mutex> lock(mesh_cache_mutex_);
        stats.loaded_assets += mesh_cache_.size();
    }
    {
        std::shared_lock<std::shared_mutex> lock(shader_cache_mutex_);
        stats.loaded_assets += shader_cache_.size();
    }
    {
        std::shared_lock<std::shared_mutex> lock(material_cache_mutex_);
        stats.loaded_assets += material_cache_.size();
    }
    
    // Calculate average load time
    if (successful_loads_ > 0) {
        stats.average_load_time_ms = 0.0f;  // Would need timing data
    }
    
    return stats;
}

void AssetManager::clear_cache() {
    {
        std::lock_guard<std::shared_mutex> lock(texture_cache_mutex_);
        texture_cache_.clear();
    }
    {
        std::lock_guard<std::shared_mutex> lock(mesh_cache_mutex_);
        mesh_cache_.clear();
    }
    {
        std::lock_guard<std::shared_mutex> lock(shader_cache_mutex_);
        shader_cache_.clear();
    }
    {
        std::lock_guard<std::shared_mutex> lock(material_cache_mutex_);
        material_cache_.clear();
    }
    cache_size_bytes_ = 0;
}

uint32_t AssetManager::evict_lru_assets() {
    uint32_t evicted = 0;
    
    // Simple LRU eviction: remove oldest accessed assets
    while (cache_size_bytes_ > max_cache_size_bytes_ * 0.9f) {
        // Find oldest asset across all caches
        AssetID oldest_asset = 0;
        auto oldest_time = std::chrono::steady_clock::now();
        
        {
            std::shared_lock<std::shared_mutex> lock(texture_cache_mutex_);
            for (const auto& [id, texture] : texture_cache_) {
                if (texture->metadata.load_time < oldest_time) {
                    oldest_time = texture->metadata.load_time;
                    oldest_asset = id;
                }
            }
        }
        
        if (oldest_asset == 0) {
            break;
        }
        
        if (unload_asset(oldest_asset)) {
            evicted++;
        }
    }
    
    return evicted;
}

void AssetManager::loader_thread_main() {
    while (!shutdown_requested_) {
        AssetRequest request;
        
        {
            std::unique_lock<std::mutex> lock(request_mutex_);
            request_cv_.wait(lock, [this] {
                return !request_queue_.empty() || shutdown_requested_;
            });
            
            if (shutdown_requested_ && request_queue_.empty()) {
                break;
            }
            
            if (request_queue_.empty()) {
                continue;
            }
            
            request = request_queue_.top();
            request_queue_.pop();
        }
        
        process_asset_request(request);
    }
}

void AssetManager::process_asset_request(const AssetRequest& request) {
    if (is_asset_loaded(request.asset_id)) {
        invoke_load_callbacks(request.asset_id, true);
        return;
    }
    
    bool success = false;
    
    switch (request.asset_type) {
        case AssetType::TEXTURE_2D:
        case AssetType::TEXTURE_CUBE:
        case AssetType::TEXTURE_3D: {
            auto texture = load_texture_from_file(request.asset_id, request.file_path);
            if (texture) {
                check_and_evict_if_needed(texture->memory_size);
                
                upload_texture_to_gpu(texture);
                
                {
                    std::lock_guard<std::shared_mutex> lock(texture_cache_mutex_);
                    texture_cache_[request.asset_id] = texture;
                    cache_size_bytes_ += texture->memory_size;
                }
                
                successful_loads_++;
                success = true;
            }
            break;
        }
        
        case AssetType::MESH: {
            auto mesh = load_mesh_from_file(request.asset_id, request.file_path);
            if (mesh) {
                size_t mesh_size = mesh->vertex_buffer_size + mesh->index_buffer_size;
                check_and_evict_if_needed(mesh_size);
                
                upload_mesh_to_gpu(mesh);
                
                {
                    std::lock_guard<std::shared_mutex> lock(mesh_cache_mutex_);
                    mesh_cache_[request.asset_id] = mesh;
                    cache_size_bytes_ += mesh_size;
                }
                
                successful_loads_++;
                success = true;
            }
            break;
        }
        
        case AssetType::SHADER: {
            auto shader = load_shader_from_file(request.asset_id, request.file_path);
            if (shader) {
                {
                    std::lock_guard<std::shared_mutex> lock(shader_cache_mutex_);
                    shader_cache_[request.asset_id] = shader;
                }
                
                successful_loads_++;
                success = true;
            }
            break;
        }
        
        default:
            break;
    }
    
    if (!success) {
        failed_loads_++;
    }
    
    invoke_load_callbacks(request.asset_id, success);
}

std::shared_ptr<Texture> AssetManager::load_texture_from_file(
    AssetID asset_id, const std::string& file_path) {
    
    auto texture = std::make_shared<Texture>();
    texture->metadata.id = asset_id;
    texture->metadata.type = AssetType::TEXTURE_2D;
    texture->metadata.name = file_path;
    texture->metadata.is_compressed = false;
    texture->metadata.load_time = std::chrono::steady_clock::now();
    
    // Load image using stb_image
    int width, height, channels;
    stbi_uc* pixels = stbi_load(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "Failed to load texture: " << file_path << " - " << stbi_failure_reason() << std::endl;
        return nullptr;
    }
    
    texture->width = static_cast<uint32_t>(width);
    texture->height = static_cast<uint32_t>(height);
    texture->depth = 1;
    texture->format = TextureFormat::RGBA8;
    texture->metadata.mip_levels = 1;  // Will be updated after mip generation
    
    // Copy pixel data
    size_t data_size = static_cast<size_t>(width * height * 4);  // RGBA8
    texture->data.assign(pixels, pixels + data_size);
    texture->memory_size = data_size;
    texture->metadata.size_bytes = data_size;
    
    stbi_image_free(pixels);
    
    return texture;
}

std::shared_ptr<Mesh> AssetManager::load_mesh_from_file(
    AssetID asset_id, const std::string& file_path) {
    
    auto mesh = std::make_shared<Mesh>();
    mesh->metadata.id = asset_id;
    mesh->metadata.type = AssetType::MESH;
    mesh->metadata.name = file_path;
    mesh->metadata.load_time = std::chrono::steady_clock::now();
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open mesh file: " << file_path << std::endl;
        return nullptr;
    }
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        } else if (type == "vn") {
            glm::vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        } else if (type == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            uvs.push_back(uv);
        } else if (type == "f") {
            std::string face_data;
            std::vector<uint32_t> face_indices;
            
            while (iss >> face_data) {
                std::replace(face_data.begin(), face_data.end(), '/', ' ');
                std::istringstream face_iss(face_data);
                uint32_t v_idx, t_idx = 0, n_idx = 0;
                face_iss >> v_idx;
                if (face_iss.peek() == ' ') face_iss >> t_idx;
                if (face_iss.peek() == ' ') face_iss >> n_idx;
                
                // OBJ uses 1-based indexing
                face_indices.push_back(v_idx - 1);
                // For simplicity, assume UV and normal indices match vertex index
                // In real OBJ, they can be different
            }
            
            // Triangulate (assume quads or triangles)
            for (size_t i = 1; i + 1 < face_indices.size(); ++i) {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[i]);
                indices.push_back(face_indices[i + 1]);
            }
        }
    }
    
    if (positions.empty()) {
        std::cerr << "No vertices found in mesh file: " << file_path << std::endl;
        return nullptr;
    }
    
    // Create vertex data: position (12 bytes), normal (12 bytes), uv (8 bytes) = 32 bytes per vertex
    mesh->vertex_count = positions.size();
    mesh->index_count = indices.size();
    mesh->vertex_buffer_size = mesh->vertex_count * 32;
    mesh->index_buffer_size = mesh->index_count * 4;
    mesh->metadata.size_bytes = mesh->vertex_buffer_size + mesh->index_buffer_size;
    
    mesh->vertex_data.resize(mesh->vertex_buffer_size);
    uint8_t* vertex_ptr = mesh->vertex_data.data();
    
    for (size_t i = 0; i < positions.size(); ++i) {
        // Position
        memcpy(vertex_ptr, &positions[i], 12);
        vertex_ptr += 12;
        
        // Normal (use default if not provided)
        glm::vec3 normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 1, 0);
        memcpy(vertex_ptr, &normal, 12);
        vertex_ptr += 12;
        
        // UV (use default if not provided)
        glm::vec2 uv = (i < uvs.size()) ? uvs[i] : glm::vec2(0, 0);
        memcpy(vertex_ptr, &uv, 8);
        vertex_ptr += 8;
    }
    
    mesh->index_data = indices;
    
    // Calculate bounds
    mesh->bounds_min = mesh->bounds_max = positions[0];
    for (const auto& pos : positions) {
        mesh->bounds_min = glm::min(mesh->bounds_min, pos);
        mesh->bounds_max = glm::max(mesh->bounds_max, pos);
    }
    
    return mesh;
}

std::shared_ptr<Shader> AssetManager::load_shader_from_file(
    AssetID asset_id, const std::string& file_path) {
    
    auto shader = std::make_shared<Shader>();
    shader->metadata.id = asset_id;
    shader->metadata.type = AssetType::SHADER;
    shader->metadata.name = file_path;
    shader->metadata.load_time = std::chrono::steady_clock::now();
    
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << file_path << std::endl;
        return nullptr;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Failed to read shader file: " << file_path << std::endl;
        return nullptr;
    }
    
    // Determine shader stage from filename
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
    if (file_path.find("frag") != std::string::npos || file_path.find("pixel") != std::string::npos) {
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    } else if (file_path.find("geom") != std::string::npos) {
        stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    } else if (file_path.find("comp") != std::string::npos) {
        stage = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buffer.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
    
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module for: " << file_path << std::endl;
        return nullptr;
    }
    
    // Assign to appropriate stage
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            shader->vertex_module = shader_module;
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            shader->fragment_module = shader_module;
            break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            shader->geometry_module = shader_module;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            shader->compute_module = shader_module;
            break;
        default:
            vkDestroyShaderModule(device_, shader_module, nullptr);
            return nullptr;
    }
    
    shader->metadata.size_bytes = buffer.size();
    
    return shader;
}

bool AssetManager::upload_texture_to_gpu(std::shared_ptr<Texture> texture) {
    if (!texture || texture->data.empty()) {
        return false;
    }
    
    // Create VkImage
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = texture->width;
    image_info.extent.height = texture->height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;  // Start with 1, will generate more
    image_info.arrayLayers = 1;
    image_info.format = static_cast<VkFormat>(texture->format);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device_, &image_info, nullptr, &texture->vk_image) != VK_SUCCESS) {
        std::cerr << "Failed to create VkImage" << std::endl;
        return false;
    }
    
    // Get memory requirements
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device_, texture->vk_image, &mem_requirements);
    
    // Allocate GPU memory
    auto alloc = gpu_allocator_->allocate(mem_requirements.size, mem_requirements.alignment);
    if (!alloc.has_value()) {
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    auto [offset, handle] = alloc.value();
    texture->memory_offset = offset;
    texture->memory_size = mem_requirements.size;
    
    // Bind memory
    VkDeviceMemory device_memory = gpu_allocator_->get_device_memory(handle);
    if (vkBindImageMemory(device_, texture->vk_image, device_memory, offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    // Create staging buffer
    VkBufferCreateInfo staging_buffer_info{};
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.size = texture->data.size();
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer staging_buffer;
    if (vkCreateBuffer(device_, &staging_buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    VkMemoryRequirements staging_mem_req;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_mem_req);
    
    // Allocate staging memory (host visible)
    auto staging_alloc = gpu_allocator_->allocate(staging_mem_req.size, staging_mem_req.alignment, 
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging_alloc.has_value()) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    auto [staging_offset, staging_handle] = staging_alloc.value();
    VkDeviceMemory staging_memory = gpu_allocator_->get_device_memory(staging_handle);
    
    if (vkBindBufferMemory(device_, staging_buffer, staging_memory, staging_offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    // Copy data to staging buffer
    void* mapped_data;
    vkMapMemory(device_, staging_memory, staging_offset, texture->data.size(), 0, &mapped_data);
    memcpy(mapped_data, texture->data.data(), texture->data.size());
    vkUnmapMemory(device_, staging_memory);
    
    // Create command buffer for transfer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = transfer_command_pool_;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);
    
    // Record commands
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    
    // Transition image to transfer dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture->vk_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {texture->width, texture->height, 1};
    
    vkCmdCopyBufferToImage(command_buffer, staging_buffer, texture->vk_image, 
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    vkEndCommandBuffer(command_buffer);
    
    // Submit and wait
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(transfer_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(transfer_queue_);
    
    // Cleanup
    vkFreeCommandBuffers(device_, transfer_command_pool_, 1, &command_buffer);
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);
    
    // Create image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = texture->vk_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = static_cast<VkFormat>(texture->format);
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device_, &view_info, nullptr, &texture->vk_image_view) != VK_SUCCESS) {
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    // Create sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 1.0f;
    
    if (vkCreateSampler(device_, &sampler_info, nullptr, &texture->vk_sampler) != VK_SUCCESS) {
        vkDestroyImageView(device_, texture->vk_image_view, nullptr);
        gpu_allocator_->deallocate(offset, mem_requirements.size);
        vkDestroyImage(device_, texture->vk_image, nullptr);
        return false;
    }
    
    // Clear CPU data after upload
    texture->data.clear();
    texture->data.shrink_to_fit();
    
    return true;
}

bool AssetManager::upload_mesh_to_gpu(std::shared_ptr<Mesh> mesh) {
    if (!mesh || mesh->vertex_data.empty() || mesh->index_data.empty()) {
        return false;
    }
    
    // Create vertex buffer
    VkBufferCreateInfo vertex_buffer_info{};
    vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_info.size = mesh->vertex_buffer_size;
    vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device_, &vertex_buffer_info, nullptr, &mesh->vertex_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    // Create index buffer
    VkBufferCreateInfo index_buffer_info{};
    index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_info.size = mesh->index_buffer_size;
    index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device_, &index_buffer_info, nullptr, &mesh->index_buffer) != VK_SUCCESS) {
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        std::cerr << "Failed to create index buffer" << std::endl;
        return false;
    }
    
    // Get memory requirements
    VkMemoryRequirements vertex_mem_req;
    vkGetBufferMemoryRequirements(device_, mesh->vertex_buffer, &vertex_mem_req);
    
    VkMemoryRequirements index_mem_req;
    vkGetBufferMemoryRequirements(device_, mesh->index_buffer, &index_mem_req);
    
    // Allocate GPU memory for vertex buffer
    auto vertex_alloc = gpu_allocator_->allocate(vertex_mem_req.size, vertex_mem_req.alignment);
    if (!vertex_alloc.has_value()) {
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    auto [vertex_offset, vertex_handle] = vertex_alloc.value();
    mesh->vertex_memory_offset = vertex_offset;
    mesh->vertex_memory = gpu_allocator_->get_device_memory(vertex_handle);
    
    VkDeviceMemory vertex_memory = mesh->vertex_memory;
    if (vkBindBufferMemory(device_, mesh->vertex_buffer, vertex_memory, vertex_offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    // Allocate GPU memory for index buffer
    auto index_alloc = gpu_allocator_->allocate(index_mem_req.size, index_mem_req.alignment);
    if (!index_alloc.has_value()) {
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    auto [index_offset, index_handle] = index_alloc.value();
    mesh->index_memory_offset = index_offset;
    mesh->index_memory = gpu_allocator_->get_device_memory(index_handle);
    
    VkDeviceMemory index_memory = mesh->index_memory;
    if (vkBindBufferMemory(device_, mesh->index_buffer, index_memory, index_offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    // Create staging buffer for vertex data
    VkBufferCreateInfo staging_vertex_info{};
    staging_vertex_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_vertex_info.size = mesh->vertex_data.size();
    staging_vertex_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_vertex_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer staging_vertex_buffer;
    if (vkCreateBuffer(device_, &staging_vertex_info, nullptr, &staging_vertex_buffer) != VK_SUCCESS) {
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    VkMemoryRequirements staging_vertex_mem_req;
    vkGetBufferMemoryRequirements(device_, staging_vertex_buffer, &staging_vertex_mem_req);
    
    auto staging_vertex_alloc = gpu_allocator_->allocate(staging_vertex_mem_req.size, staging_vertex_mem_req.alignment,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging_vertex_alloc.has_value()) {
        vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    auto [staging_vertex_offset, staging_vertex_handle] = staging_vertex_alloc.value();
    VkDeviceMemory staging_vertex_memory = gpu_allocator_->get_device_memory(staging_vertex_handle);
    
    if (vkBindBufferMemory(device_, staging_vertex_buffer, staging_vertex_memory, staging_vertex_offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(staging_vertex_offset, staging_vertex_mem_req.size);
        vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    // Copy vertex data
    void* vertex_mapped;
    vkMapMemory(device_, staging_vertex_memory, staging_vertex_offset, mesh->vertex_data.size(), 0, &vertex_mapped);
    memcpy(vertex_mapped, mesh->vertex_data.data(), mesh->vertex_data.size());
    vkUnmapMemory(device_, staging_vertex_memory);
    
    // Create staging buffer for index data
    VkBufferCreateInfo staging_index_info{};
    staging_index_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_index_info.size = mesh->index_data.size() * 4;
    staging_index_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_index_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer staging_index_buffer;
    if (vkCreateBuffer(device_, &staging_index_info, nullptr, &staging_index_buffer) != VK_SUCCESS) {
        gpu_allocator_->deallocate(staging_vertex_offset, staging_vertex_mem_req.size);
        vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    VkMemoryRequirements staging_index_mem_req;
    vkGetBufferMemoryRequirements(device_, staging_index_buffer, &staging_index_mem_req);
    
    auto staging_index_alloc = gpu_allocator_->allocate(staging_index_mem_req.size, staging_index_mem_req.alignment,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging_index_alloc.has_value()) {
        vkDestroyBuffer(device_, staging_index_buffer, nullptr);
        gpu_allocator_->deallocate(staging_vertex_offset, staging_vertex_mem_req.size);
        vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    auto [staging_index_offset, staging_index_handle] = staging_index_alloc.value();
    VkDeviceMemory staging_index_memory = gpu_allocator_->get_device_memory(staging_index_handle);
    
    if (vkBindBufferMemory(device_, staging_index_buffer, staging_index_memory, staging_index_offset) != VK_SUCCESS) {
        gpu_allocator_->deallocate(staging_index_offset, staging_index_mem_req.size);
        vkDestroyBuffer(device_, staging_index_buffer, nullptr);
        gpu_allocator_->deallocate(staging_vertex_offset, staging_vertex_mem_req.size);
        vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
        gpu_allocator_->deallocate(index_offset, index_mem_req.size);
        gpu_allocator_->deallocate(vertex_offset, vertex_mem_req.size);
        vkDestroyBuffer(device_, mesh->vertex_buffer, nullptr);
        vkDestroyBuffer(device_, mesh->index_buffer, nullptr);
        return false;
    }
    
    // Copy index data
    void* index_mapped;
    vkMapMemory(device_, staging_index_memory, staging_index_offset, mesh->index_data.size() * 4, 0, &index_mapped);
    memcpy(index_mapped, mesh->index_data.data(), mesh->index_data.size() * 4);
    vkUnmapMemory(device_, staging_index_memory);
    
    // Create command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = transfer_command_pool_;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);
    
    // Record commands
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    
    // Copy vertex buffer
    VkBufferCopy vertex_copy{};
    vertex_copy.size = mesh->vertex_data.size();
    vkCmdCopyBuffer(command_buffer, staging_vertex_buffer, mesh->vertex_buffer, 1, &vertex_copy);
    
    // Copy index buffer
    VkBufferCopy index_copy{};
    index_copy.size = mesh->index_data.size() * 4;
    vkCmdCopyBuffer(command_buffer, staging_index_buffer, mesh->index_buffer, 1, &index_copy);
    
    vkEndCommandBuffer(command_buffer);
    
    // Submit
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(transfer_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(transfer_queue_);
    
    // Cleanup
    vkFreeCommandBuffers(device_, transfer_command_pool_, 1, &command_buffer);
    vkDestroyBuffer(device_, staging_vertex_buffer, nullptr);
    vkDestroyBuffer(device_, staging_index_buffer, nullptr);
    gpu_allocator_->deallocate(staging_vertex_offset, staging_vertex_mem_req.size);
    gpu_allocator_->deallocate(staging_index_offset, staging_index_mem_req.size);
    
    // Clear CPU data
    mesh->vertex_data.clear();
    mesh->vertex_data.shrink_to_fit();
    mesh->index_data.clear();
    mesh->index_data.shrink_to_fit();
    
    return true;
}

void AssetManager::invoke_load_callbacks(AssetID asset_id, bool success) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto it = load_callbacks_.find(asset_id);
    if (it != load_callbacks_.end()) {
        for (auto& callback : it->second) {
            callback(asset_id, success);
        }
        load_callbacks_.erase(it);
    }
}

void AssetManager::check_and_evict_if_needed(size_t additional_size) {
    if (cache_size_bytes_ + additional_size > max_cache_size_bytes_) {
        evict_lru_assets();
    }
}

AssetID AssetManager::register_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    AssetID asset_id = next_request_id_.fetch_add(1);
    
    auto mesh = std::make_shared<Mesh>();
    mesh->metadata.id = asset_id;
    mesh->metadata.type = AssetType::MESH;
    mesh->metadata.name = "procedural_mesh_" + std::to_string(asset_id);
    mesh->metadata.load_time = std::chrono::steady_clock::now();
    mesh->metadata.is_compressed = false;
    mesh->metadata.mip_levels = 1;
    mesh->metadata.reference_count = 1;
    
    mesh->vertex_count = vertices.size();
    mesh->index_count = indices.size();
    mesh->lod_count = 1;
    
    // Copy vertex data
    size_t vertex_data_size = vertices.size() * sizeof(Vertex);
    mesh->vertex_data.resize(vertex_data_size);
    memcpy(mesh->vertex_data.data(), vertices.data(), vertex_data_size);
    
    // Copy index data
    mesh->index_data = indices;
    
    mesh->vertex_buffer_size = vertex_data_size;
    mesh->index_buffer_size = indices.size() * sizeof(uint32_t);
    mesh->metadata.size_bytes = mesh->vertex_buffer_size + mesh->index_buffer_size;
    
    // Calculate bounds
    if (!vertices.empty()) {
        mesh->bounds_min = mesh->bounds_max = vertices[0].position;
        for (const auto& vertex : vertices) {
            mesh->bounds_min = glm::min(mesh->bounds_min, vertex.position);
            mesh->bounds_max = glm::max(mesh->bounds_max, vertex.position);
        }
    }
    
    // Upload to GPU
    if (!upload_mesh_to_gpu(mesh)) {
        return 0; // Failed
    }
    
    // Add to cache
    {
        std::lock_guard<std::shared_mutex> lock(mesh_cache_mutex_);
        mesh_cache_[asset_id] = mesh;
        cache_size_bytes_ += mesh->metadata.size_bytes;
    }
    
    check_and_evict_if_needed(0);
    
    return asset_id;
}

bool AssetManager::update_mesh_vertices(AssetID asset_id, const std::vector<Vertex>& vertices) {
    std::shared_ptr<Mesh> mesh;
    {
        std::shared_lock<std::shared_mutex> lock(mesh_cache_mutex_);
        auto it = mesh_cache_.find(asset_id);
        if (it == mesh_cache_.end() || !it->second) {
            return false;
        }
        mesh = it->second;
    }

    size_t vertex_size = vertices.size() * sizeof(Vertex);
    if (vertex_size != mesh->vertex_buffer_size) {
        return false;
    }

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo staging_buffer_info{};
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.size = vertex_size;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &staging_buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements staging_mem_req;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_mem_req);

    auto staging_alloc = gpu_allocator_->allocate(
        staging_mem_req.size,
        staging_mem_req.alignment,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (!staging_alloc.has_value()) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }

    uint64_t staging_offset = staging_alloc->first;
    uint64_t staging_handle = staging_alloc->second;
    VkDeviceMemory staging_device_memory = gpu_allocator_->get_device_memory(staging_handle);

    if (vkBindBufferMemory(device_, staging_buffer, staging_device_memory, staging_offset) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device_, staging_device_memory, staging_offset, vertex_size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);
        return false;
    }

    memcpy(mapped, vertices.data(), vertex_size);
    vkUnmapMemory(device_, staging_device_memory);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = transfer_command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region{};
    copy_region.size = vertex_size;
    vkCmdCopyBuffer(command_buffer, staging_buffer, mesh->vertex_buffer, 1, &copy_region);
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    vkQueueSubmit(transfer_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(transfer_queue_);

    vkFreeCommandBuffers(device_, transfer_command_pool_, 1, &command_buffer);
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    gpu_allocator_->deallocate(staging_offset, staging_mem_req.size);

    return true;
}

AssetID AssetManager::get_or_create_default_material() {
    static AssetID default_material_id = 0;
    
    // Check if already exists
    if (default_material_id != 0) {
        auto existing = get_material(default_material_id);
        if (existing) {
            return default_material_id;
        }
    }
    
    // Create new default material
    default_material_id = next_request_id_.fetch_add(1);
    
    auto material = std::make_shared<Material>();
    material->metadata.id = default_material_id;
    material->metadata.type = AssetType::MATERIAL;
    material->metadata.name = "default_material";
    material->metadata.load_time = std::chrono::steady_clock::now();
    material->metadata.size_bytes = sizeof(Material);
    material->metadata.is_compressed = false;
    material->metadata.mip_levels = 1;
    material->metadata.reference_count = 1;
    
    // Default PBR values
    material->albedo_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f); // Light gray
    material->roughness = 0.5f;
    material->metallic = 0.2f;
    material->ambient_occlusion = 1.0f;
    
    // No textures
    material->albedo_texture_id = 0;
    material->normal_texture_id = 0;
    material->metallic_roughness_texture_id = 0;
    material->emission_texture_id = 0;
    material->shader_id = 0;
    
    // Add to cache
    {
        std::lock_guard<std::shared_mutex> lock(material_cache_mutex_);
        material_cache_[default_material_id] = material;
        cache_size_bytes_ += material->metadata.size_bytes;
    }
    
    check_and_evict_if_needed(0);
    
    return default_material_id;
}

}  // namespace rendering_engine::resource_manager
