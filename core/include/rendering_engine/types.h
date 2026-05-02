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
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <limits>

namespace rendering_engine {

// ============================================================================
// VULKAN TYPES AND ENUMS
// ============================================================================

enum class RenderPipelineType : uint32_t {
    FORWARD_PLUS = 0,
    DEFERRED = 1,
    CLUSTERED = 2,
    RAY_TRACED = 3,
};

enum class LightType : uint32_t {
    POINT = 0,
    SPOT = 1,
    DIRECTIONAL = 2,
    AREA = 3,
};

// ============================================================================
// LIGHT DATA STRUCTURES
// ============================================================================

struct alignas(16) LightData {
    glm::vec3 position;              // World space position
    float radius;                    // Attenuation radius
    glm::vec3 color;                 // RGB intensity
    float intensity;                 // Combined intensity (luminous flux)
    uint32_t light_type;             // POINT, SPOT, DIRECTIONAL
    glm::vec3 direction;             // For spot/directional lights
    float inner_angle;               // Inner cone angle (radians)
    float outer_angle;               // Outer cone angle (radians)
    glm::mat4 shadow_projection;     // For shadow mapping
};

// Forward+ specific light culling data
struct ForwardPlusTile {
    static constexpr uint32_t MAX_LIGHTS_PER_TILE = 256;
    uint32_t light_indices[MAX_LIGHTS_PER_TILE]; // Max lights per tile
    uint32_t light_count;                        // Actual count
    uint32_t padding[3];                         // SIMD alignment
};

// Clustered shading cluster data
struct ClusteredCluster {
    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 512;
    glm::vec3 min_bounds;            // Frustum-aligned bounds (min)
    uint32_t min_z;                  // Quantized min depth
    glm::vec3 max_bounds;            // Frustum-aligned bounds (max)
    uint32_t max_z;                  // Quantized max depth
    uint32_t light_indices[MAX_LIGHTS_PER_CLUSTER]; // Max lights per cluster
    uint32_t light_count;            // Actual count
    uint32_t padding[2];             // Alignment
};

// ============================================================================
// G-BUFFER STRUCTURES
// ============================================================================

struct GBufferAttachments {
    VkImage albedo_roughness;        // RGBA8 - Base color + roughness
    VkImageView albedo_roughness_view;
    
    VkImage normal;                  // RG16F - World space normals (oct-encoded)
    VkImageView normal_view;
    
    VkImage material_data;           // RGBA8 - Metallic, AO, emissive, flags
    VkImageView material_data_view;
    
    VkImage depth;                   // D32F - Depth buffer
    VkImageView depth_view;
    
    VkImage velocity;                // RG16F - Motion vectors for TAA
    VkImageView velocity_view;
    
    VkImage object_id;               // R32UI - Entity ID for picking
    VkImageView object_id_view;
    
    VkFramebuffer framebuffer;       // G-buffer framebuffer
    VkRenderPass render_pass;        // G-buffer render pass
};

// ============================================================================
// MESH AND GEOMETRY STRUCTURES
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texcoord;
    uint32_t color;                  // RGBA packed
};

struct Mesh {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VkDeviceMemory vertex_memory;
    VkDeviceMemory index_memory;
    uint32_t index_count;
    uint32_t vertex_count;
};

// ============================================================================
// MATERIAL STRUCTURES
// ============================================================================

struct alignas(16) MaterialProperties {
    glm::vec3 albedo;                // Base color
    float roughness;                 // 0.0 - 1.0
    glm::vec3 f0;                    // Specular color (usually constant)
    float metallic;                  // 0.0 - 1.0
    float ao;                        // Ambient occlusion
    float emissive_strength;         // Emissive multiplier
    uint32_t flags;                  // Material flags
    uint32_t texture_indices;        // Packed texture indices
};

// ============================================================================
// CAMERA STRUCTURES
// ============================================================================

struct Camera {
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;
    
    float fov;                       // Vertical FOV in radians
    float aspect_ratio;              // Width / Height
    float near_plane;                // Near clipping plane
    float far_plane;                 // Far clipping plane
    
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
    glm::mat4 view_projection_matrix;
    glm::mat4 inverse_view_matrix;
    glm::mat4 inverse_projection_matrix;
    glm::mat4 inverse_view_projection_matrix;
    
    glm::vec4 frustum_planes[6];     // for frustum culling
};

// ============================================================================
// RENDER STATE STRUCTURES
// ============================================================================

struct RenderableObject {
    VkBuffer transform_buffer;
    glm::mat4 transform;
    uint32_t mesh_id;
    uint32_t material_id;
    uint32_t entity_id;              // For picking and identification
    bool cast_shadow;
    bool is_dynamic;
};

struct RenderTargets {
    VkImage color_image;
    VkImageView color_view;
    
    VkImage depth_image;
    VkImageView depth_view;
    
    VkFramebuffer framebuffer;
    uint32_t width;
    uint32_t height;
    
    VkFormat color_format;
    VkFormat depth_format;
};

// ============================================================================
// SHADER CONSTANTS
// ============================================================================

struct alignas(256) ViewConstants {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::mat4 inverse_view;
    glm::mat4 inverse_projection;
    glm::mat4 inverse_view_projection;
    glm::vec4 camera_position;       // w = near plane
    glm::vec4 screen_dimensions;     // xy = resolution, zw = 1/resolution
    glm::vec4 time_params;           // x = time, y = delta time, z = frame count
};

struct alignas(256) LightingConstants {
    glm::vec4 ambient_color;         // RGB + intensity
    uint32_t light_count;
    uint32_t max_lights_per_tile;
    uint32_t tile_size_x;
    uint32_t tile_size_y;
    uint32_t cluster_grid_x;
    uint32_t cluster_grid_y;
    uint32_t cluster_grid_z;
    float cluster_size;
};

// ============================================================================
// CONSTANTS
// ============================================================================

namespace Constants {
    constexpr uint32_t TILE_SIZE = 16;
    constexpr uint32_t MAX_LIGHTS_FORWARD_PLUS = 10000;
    constexpr uint32_t MAX_LIGHTS_CLUSTERED = 50000;
    constexpr uint32_t MAX_CLUSTER_X = 32;
    constexpr uint32_t MAX_CLUSTER_Y = 32;
    constexpr uint32_t MAX_CLUSTER_Z = 16;
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    constexpr uint32_t DESCRIPTOR_POOL_SIZE = 500;
    constexpr float LIGHT_ATTENUATION_THRESHOLD = 0.001f; // 0.1% intensity
}

} // namespace rendering_engine
