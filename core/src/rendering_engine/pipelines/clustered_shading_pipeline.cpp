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
#include "rendering_engine/pipelines/clustered_shading_pipeline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace rendering_engine {

ClusteredShadingPipeline::~ClusteredShadingPipeline() {
    if (is_initialized_) {
        shutdown();
    }
}

void ClusteredShadingPipeline::initialize(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkQueue graphics_queue,
    VkCommandPool command_pool,
    uint32_t width,
    uint32_t height
) {
    device_ = device;
    physical_device_ = physical_device;
    graphics_queue_ = graphics_queue;
    command_pool_ = command_pool;
    render_width_ = width;
    render_height_ = height;
    
    // Compute cluster grid dimensions
    compute_cluster_grid_dimensions();
    
    // Initialize Vulkan resources
    create_cluster_resources();
    create_light_data_buffer();
    create_uniform_buffers();
    create_compute_pipelines();
    create_render_passes();
    create_descriptor_sets();
    create_shading_pipelines();
    create_framebuffers();
    
    is_initialized_ = true;
}

void ClusteredShadingPipeline::begin_frame(uint32_t frame_index) {
    current_frame_index_ = frame_index;
    render_queue_.clear();
    opaque_queue_.clear();
    transparent_queue_.clear();
    light_culling_time_ms_ = 0.0f;
    shading_time_ms_ = 0.0f;
}

void ClusteredShadingPipeline::update_camera(const Camera& camera) {
    current_camera_ = camera;
    
    // Store frustum bounds for cluster computation
    frustum_bounds_.near_plane_center = camera.position + camera.forward * camera.near_plane;
    frustum_bounds_.far_plane_center = camera.position + camera.forward * camera.far_plane;
    frustum_bounds_.near_distance = camera.near_plane;
    frustum_bounds_.far_distance = camera.far_plane;
    
    // Update view constants buffer
    if (current_frame_index_ < view_constant_buffers_.size()) {
        ViewConstants constants{
            .view = camera.view_matrix,
            .projection = camera.projection_matrix,
            .view_projection = camera.view_projection_matrix,
            .inverse_view = camera.inverse_view_matrix,
            .inverse_projection = camera.inverse_projection_matrix,
            .inverse_view_projection = camera.inverse_view_projection_matrix,
            .camera_position = glm::vec4(camera.position, camera.near_plane),
            .screen_dimensions = glm::vec4(render_width_, render_height_, 1.0f / render_width_, 1.0f / render_height_),
            .time_params = glm::vec4(0.0f)
        };
        
        // Copy to buffer
    }
}

void ClusteredShadingPipeline::update_lights(const std::vector<LightData>& lights) {
    current_lights_ = lights;
    
    // Update light data buffer
    if (!light_data_buffer_) {
        return;
    }
}

void ClusteredShadingPipeline::queue_renderables(const std::vector<RenderableObject>& objects) {
    for (const auto& obj : objects) {
        opaque_queue_.push_back(obj);
    }
    render_queue_ = objects;
}

void ClusteredShadingPipeline::render(
    VkCommandBuffer command_buffer,
    const RenderTargets& target
) {
    if (!is_initialized_) return;
    
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    // Rebuild clusters if camera changed significantly
    rebuild_clusters_if_needed();
    
    // Perform cluster culling
    auto cull_start = std::chrono::high_resolution_clock::now();
    perform_cluster_culling(command_buffer);
    auto cull_end = std::chrono::high_resolution_clock::now();
    light_culling_time_ms_ = std::chrono::duration<float, std::milli>(cull_end - cull_start).count();
    
    // Perform light assignment to clusters
    auto assign_start = std::chrono::high_resolution_clock::now();
    perform_light_assignment(command_buffer);
    auto assign_end = std::chrono::high_resolution_clock::now();
    light_culling_time_ms_ += std::chrono::duration<float, std::milli>(assign_end - assign_start).count();
    
    // Perform clustered shading
    auto shade_start = std::chrono::high_resolution_clock::now();
    perform_clustered_shading(command_buffer, target);
    auto shade_end = std::chrono::high_resolution_clock::now();
    shading_time_ms_ = std::chrono::duration<float, std::milli>(shade_end - shade_start).count();
    
    // Perform transparent pass
    perform_transparent_pass(command_buffer, target);
}

void ClusteredShadingPipeline::end_frame() {
    // Optional frame cleanup
}

void ClusteredShadingPipeline::resize(uint32_t width, uint32_t height) {
    if (width == render_width_ && height == render_height_) {
        return;
    }
    
    render_width_ = width;
    render_height_ = height;
    
    // Recalculate cluster grid
    compute_cluster_grid_dimensions();
    
    // Rebuild GPU resources
    shutdown();
    initialize(device_, physical_device_, graphics_queue_, command_pool_, width, height);
}

void ClusteredShadingPipeline::shutdown() {
    if (!device_) return;
    
    vkDeviceWaitIdle(device_);
    
    // Destroy pipelines
    if (clustered_shading_pipeline_) vkDestroyPipeline(device_, clustered_shading_pipeline_, nullptr);
    if (cluster_culling_compute_pipeline_) vkDestroyPipeline(device_, cluster_culling_compute_pipeline_, nullptr);
    if (cluster_light_assignment_pipeline_) vkDestroyPipeline(device_, cluster_light_assignment_pipeline_, nullptr);
    
    // Destroy pipeline layouts
    if (cluster_cull_layout_) vkDestroyPipelineLayout(device_, cluster_cull_layout_, nullptr);
    if (clustered_shading_layout_) vkDestroyPipelineLayout(device_, clustered_shading_layout_, nullptr);
    
    // Destroy render passes
    if (clustered_render_pass_) vkDestroyRenderPass(device_, clustered_render_pass_, nullptr);
    if (transparent_render_pass_) vkDestroyRenderPass(device_, transparent_render_pass_, nullptr);
    
    // Destroy framebuffers
    for (auto fb : clustered_framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    for (auto fb : transparent_framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    
    // Destroy cluster buffers
    destroy_buffer(cluster_light_indices_buffer_, cluster_light_indices_memory_);
    destroy_buffer(cluster_light_counts_buffer_, cluster_light_counts_memory_);
    destroy_buffer(cluster_aabb_buffer_, cluster_aabb_memory_);
    destroy_buffer(light_data_buffer_, light_data_memory_);
    
    // Destroy uniform buffers
    for (size_t i = 0; i < view_constant_buffers_.size(); ++i) {
        destroy_buffer(view_constant_buffers_[i], view_constant_memories_[i]);
    }
    
    for (size_t i = 0; i < lighting_constant_buffers_.size(); ++i) {
        destroy_buffer(lighting_constant_buffers_[i], lighting_constant_memories_[i]);
    }
    
    // Destroy debug resources
    destroy_image(cluster_debug_image_, cluster_debug_view_, cluster_debug_memory_);
    
    // Destroy descriptor pool
    if (descriptor_pool_) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    if (cluster_cull_descriptor_layout_) vkDestroyDescriptorSetLayout(device_, cluster_cull_descriptor_layout_, nullptr);
    if (clustered_shading_descriptor_layout_) vkDestroyDescriptorSetLayout(device_, clustered_shading_descriptor_layout_, nullptr);
    
    // Destroy query pool
    if (query_pool_) vkDestroyQueryPool(device_, query_pool_, nullptr);
    
    is_initialized_ = false;
}

// Private implementation methods

void ClusteredShadingPipeline::create_cluster_resources() {
    // Create cluster light indices buffer
    // Create cluster light counts buffer
    // Create cluster AABB buffer
}

void ClusteredShadingPipeline::create_light_data_buffer() {
    // Create buffer for light data structures
}

void ClusteredShadingPipeline::create_compute_pipelines() {
    // Create cluster culling compute shader
    // Create light assignment compute shader
}

void ClusteredShadingPipeline::create_shading_pipelines() {
    // Create clustered shading graphics pipeline
    // Create transparent rendering pipeline
}

void ClusteredShadingPipeline::create_render_passes() {
    // Create main render pass for clustered shading
    // Create render pass for transparent objects
}

void ClusteredShadingPipeline::create_framebuffers() {
    // Create framebuffers for main pass
    // Create framebuffers for transparent pass
}

void ClusteredShadingPipeline::create_descriptor_sets() {
    // Create descriptor sets for compute shaders
    // Create descriptor sets for shading
}

void ClusteredShadingPipeline::create_uniform_buffers() {
    // Create view constant buffers
    // Create lighting constant buffers
}

void ClusteredShadingPipeline::compute_cluster_grid_dimensions() {
    // Keep cluster grid dimensions reasonable
    // Typically: 32x32 for XY (screen space), 16 for Z (depth)
    cluster_grid_x_ = std::min(32u, Constants::MAX_CLUSTER_X);
    cluster_grid_y_ = std::min(32u, Constants::MAX_CLUSTER_Y);
    cluster_grid_z_ = Constants::MAX_CLUSTER_Z;
}

void ClusteredShadingPipeline::compute_cluster_aabb() {
    // Compute axis-aligned bounding boxes for each cluster
    // in world space
}

void ClusteredShadingPipeline::perform_cluster_culling(VkCommandBuffer cmd_buffer) {
    // Dispatch compute shader to frustum subdivide
    // Create cluster AABBs
}

void ClusteredShadingPipeline::perform_light_assignment(VkCommandBuffer cmd_buffer) {
    // Dispatch compute shader to assign lights to clusters
    // Build per-cluster light index lists
}

void ClusteredShadingPipeline::perform_clustered_shading(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    // Render all opaque objects using clustered shading
    // Fragment shader samples cluster light list
}

void ClusteredShadingPipeline::perform_transparent_pass(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    // Render transparent objects
}

void ClusteredShadingPipeline::rebuild_clusters_if_needed() {
    // Check if camera moved significantly enough to require rebuilding
    // Implement frustum change detection
}

bool ClusteredShadingPipeline::is_cluster_rebuild_needed() const {
    // Determine if clusters need rebuilding
    // Return true if camera frustum changed significantly
    return false; // Placeholder
}

void ClusteredShadingPipeline::destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory) {
        vkFreeMemory(device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void ClusteredShadingPipeline::destroy_image(VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
    if (view) {
        vkDestroyImageView(device_, view, nullptr);
        view = VK_NULL_HANDLE;
    }
    if (image) {
        vkDestroyImage(device_, image, nullptr);
        image = VK_NULL_HANDLE;
    }
    if (memory) {
        vkFreeMemory(device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void ClusteredShadingPipeline::destroy_pipeline(VkPipeline& pipeline) {
    if (pipeline) {
        vkDestroyPipeline(device_, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}

void ClusteredShadingPipeline::destroy_render_pass(VkRenderPass& render_pass) {
    if (render_pass) {
        vkDestroyRenderPass(device_, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }
}

void ClusteredShadingPipeline::destroy_framebuffers(std::vector<VkFramebuffer>& framebuffers) {
    for (auto& fb : framebuffers) {
        if (fb) {
            vkDestroyFramebuffer(device_, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    framebuffers.clear();
}

VkShaderModule ClusteredShadingPipeline::load_shader_module(const std::string& filename) {
    // Load SPIR-V shader from file
    return VK_NULL_HANDLE; // Placeholder
}

void ClusteredShadingPipeline::destroy_shader_module(VkShaderModule& shader) {
    if (shader) {
        vkDestroyShaderModule(device_, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }
}

} // namespace rendering_engine
