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
#include <string>
#include <chrono>
#include <future>
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace rendering_engine::resource_manager {

/**
 * @brief Unique identifier for assets
 */
using AssetID = uint64_t;

/**
 * @brief Asset type enumeration
 */
enum class AssetType : uint32_t {
    TEXTURE_2D = 0,
    TEXTURE_CUBE = 1,
    TEXTURE_3D = 2,
    MESH = 3,
    SHADER = 4,
    MATERIAL = 5,
    AUDIO = 6,
    ANIMATION = 7,
    PHYSICS_MESH = 8,
    TERRAIN_PATCH = 9,
    UNKNOWN = 10
};

/**
 * @brief Texture format enumeration aligned with Vulkan formats
 */
enum class TextureFormat : uint32_t {
    RGBA8 = VK_FORMAT_R8G8B8A8_UNORM,
    RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,
    RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,
    BC1 = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
    BC3 = VK_FORMAT_BC3_UNORM_BLOCK,
    BC4 = VK_FORMAT_BC4_UNORM_BLOCK,
    BC5 = VK_FORMAT_BC5_UNORM_BLOCK,
    BC6H = VK_FORMAT_BC6H_UFLOAT_BLOCK,
    BC7 = VK_FORMAT_BC7_UNORM_BLOCK,
    ASTC_4x4 = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
};

/**
 * @brief Load priority for asset requests
 */
enum class LoadPriority : uint32_t {
    PRIORITY_CRITICAL = 0,   // Load immediately (UI, essential)
    PRIORITY_HIGH = 1,       // Pre-load visible assets
    PRIORITY_NORMAL = 2,     // Regular background loading
    PRIORITY_LOW = 3,        // Stream assets at leisure
    PRIORITY_DEFERRED = 4    // Load when prioritized
};

/**
 * @brief Asset request for async loading
 */
struct AssetRequest {
    uint64_t request_id;              // Unique request ID
    AssetID asset_id;                 // Asset identifier
    AssetType asset_type;             // Type of asset
    std::string file_path;            // Path to asset file
    LoadPriority priority;            // Loading priority
    std::chrono::steady_clock::time_point created_time;
    
    // Metadata and options
    bool async_only{false};           // Don't use sync loading
    bool preload_dependencies{true}; // Load related assets
    uint32_t target_mip_levels{0};   // 0 = auto-calculate
    bool generate_mipmaps{true};      // Auto-generate mipmaps
    
    AssetRequest() : request_id(0), asset_id(0), asset_type(AssetType::UNKNOWN),
                     priority(LoadPriority::PRIORITY_NORMAL) {
        created_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Base asset metadata
 */
struct AssetMetadata {
    AssetID id;
    AssetType type;
    std::string name;
    size_t size_bytes;
    uint32_t mip_levels;
    bool is_compressed;
    std::chrono::steady_clock::time_point load_time;
    uint32_t reference_count;
    
    AssetMetadata() : id(0), type(AssetType::UNKNOWN), size_bytes(0),
                     mip_levels(1), is_compressed(false), reference_count(0) {}
};

/**
 * @brief Texture asset information
 */
struct Texture {
    AssetMetadata metadata;
    
    // Texture properties
    uint32_t width;
    uint32_t height;
    uint32_t depth;                  // For 3D textures
    TextureFormat format;
    VkImage vk_image;                // Vulkan image handle
    VkImageView vk_image_view;       // Vulkan image view
    VkSampler vk_sampler;            // Vulkan sampler
    
    // CPU data (before GPU upload)
    std::vector<uint8_t> data;
    
    // Memory info
    uint64_t memory_offset;
    size_t memory_size;
    
    Texture() : width(0), height(0), depth(1), format(TextureFormat::RGBA8),
               vk_image(VK_NULL_HANDLE), vk_image_view(VK_NULL_HANDLE),
               vk_sampler(VK_NULL_HANDLE), memory_offset(0), memory_size(0) {}
};

/**
 * @brief Mesh asset information
 */
struct Mesh {
    AssetMetadata metadata;
    
    // Mesh properties
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t lod_count;              // Number of LOD variants
    
    // CPU data (before GPU upload)
    std::vector<uint8_t> vertex_data;
    std::vector<uint32_t> index_data;
    
    // GPU buffers
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VkDeviceMemory vertex_memory;
    VkDeviceMemory index_memory;
    VkBuffer lod_buffers[8];         // Up to 8 LOD levels
    
    // Bounds
    glm::vec3 bounds_min;
    glm::vec3 bounds_max;
    
    // Memory info
    uint64_t vertex_memory_offset;
    uint64_t index_memory_offset;
    size_t vertex_buffer_size;
    size_t index_buffer_size;
    
    Mesh() : vertex_count(0), index_count(0), lod_count(1),
            vertex_buffer(VK_NULL_HANDLE), index_buffer(VK_NULL_HANDLE),
            vertex_memory(VK_NULL_HANDLE), index_memory(VK_NULL_HANDLE),
            vertex_memory_offset(0), index_memory_offset(0),
            vertex_buffer_size(0), index_buffer_size(0) {
        for (uint32_t i = 0; i < 8; ++i) {
            lod_buffers[i] = VK_NULL_HANDLE;
        }
    }
};

/**
 * @brief Shader asset information
 */
struct Shader {
    AssetMetadata metadata;
    
    // Shader stages
    VkShaderModule vertex_module;
    VkShaderModule fragment_module;
    VkShaderModule geometry_module;
    VkShaderModule compute_module;
    
    // Pipeline layout
    VkPipelineLayout pipeline_layout;
    
    Shader() : vertex_module(VK_NULL_HANDLE), fragment_module(VK_NULL_HANDLE),
              geometry_module(VK_NULL_HANDLE), compute_module(VK_NULL_HANDLE),
              pipeline_layout(VK_NULL_HANDLE) {}
};

/**
 * @brief Material asset information
 */
struct Material {
    AssetMetadata metadata;
    
    // Material properties
    glm::vec4 albedo_color;
    float roughness;
    float metallic;
    float ambient_occlusion;
    
    // Texture references
    AssetID albedo_texture_id;
    AssetID normal_texture_id;
    AssetID metallic_roughness_texture_id;
    AssetID emission_texture_id;
    
    // Shader reference
    AssetID shader_id;
    
    Material() : albedo_color(1.0f, 1.0f, 1.0f, 1.0f),
                roughness(0.5f), metallic(0.0f), ambient_occlusion(1.0f),
                albedo_texture_id(0), normal_texture_id(0),
                metallic_roughness_texture_id(0), emission_texture_id(0),
                shader_id(0) {}
};

/**
 * @brief Asset loading callback type
 */
using AssetLoadCallback = std::function<void(AssetID, bool success)>;

/**
 * @brief Asset loading future for async operations
 */
template<typename T>
using AssetFuture = std::future<T>;

/**
 * @brief Streaming context for LOD-based loading
 */
struct StreamingContext {
    std::vector<AssetID> priority_assets;    // High-priority assets
    std::vector<AssetID> visible_assets;     // Currently visible
    std::vector<AssetID> streaming_assets;   // Low-priority streaming
    
    float streaming_distance;                 // Distance for pre-loading
    float unloading_distance;                 // Distance for unloading
    uint32_t max_streaming_requests;         // Concurrent streaming limit
    
    StreamingContext() : streaming_distance(100.0f), unloading_distance(200.0f),
                       max_streaming_requests(8) {}
};

}  // namespace rendering_engine::resource_manager
