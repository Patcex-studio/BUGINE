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
#include "rendering_engine/pipelines/forward_plus_pipeline.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstring>

namespace rendering_engine {

ForwardPlusPipeline::~ForwardPlusPipeline() {
    if (is_initialized_) {
        shutdown();
    }
}

void ForwardPlusPipeline::initialize(
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
    
    // Calculate tile configuration
    tiles_x_ = (render_width_ + Constants::TILE_SIZE - 1) / Constants::TILE_SIZE;
    tiles_y_ = (render_height_ + Constants::TILE_SIZE - 1) / Constants::TILE_SIZE;
    
    // Initialize Vulkan resources
    create_depth_prepass_resources();
    create_light_culling_resources();
    create_uniform_buffers();
    create_render_passes();
    create_descriptor_sets();
    create_pipelines();
    
    is_initialized_ = true;
}

void ForwardPlusPipeline::begin_frame(uint32_t frame_index) {
    current_frame_index_ = frame_index;
    render_queue_.clear();
    opaque_queue_.clear();
    transparent_queue_.clear();
    light_culling_time_ms_ = 0.0f;
    shading_time_ms_ = 0.0f;
}

void ForwardPlusPipeline::update_camera(const Camera& camera) {
    current_camera_ = camera;
    
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
            .time_params = glm::vec4(0.0f) // Will be filled by frame time
        };
        
        // Copy to buffer (would use vkMapMemory in real implementation)
    }
}

void ForwardPlusPipeline::update_lights(const std::vector<LightData>& lights) {
    current_lights_ = lights;
    
    // Clamp to max lights for this pipeline
    if (current_lights_.size() > Constants::MAX_LIGHTS_FORWARD_PLUS) {
        current_lights_.resize(Constants::MAX_LIGHTS_FORWARD_PLUS);
    }
    
    // Update light data buffer
    if (!light_data_buffer_) {
        return;
    }
    
    // Copy light data to GPU buffer (would use vkMapMemory in real implementation)
    // For now just store locally
}

void ForwardPlusPipeline::queue_renderables(const std::vector<RenderableObject>& objects) {
    for (const auto& obj : objects) {
        if (obj.is_dynamic) {
            transparent_queue_.push_back(obj);
        } else {
            opaque_queue_.push_back(obj);
        }
    }
    render_queue_ = objects;
}

void ForwardPlusPipeline::render(
    VkCommandBuffer command_buffer,
    const RenderTargets& target
) {
    if (!is_initialized_) return;
    
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    // Perform depth pre-pass
    perform_depth_prepass(command_buffer);
    
    // Perform light culling
    auto cull_start = std::chrono::high_resolution_clock::now();
    perform_light_culling(command_buffer);
    auto cull_end = std::chrono::high_resolution_clock::now();
    light_culling_time_ms_ = std::chrono::duration<float, std::milli>(cull_end - cull_start).count();
    
    // Perform main shading pass
    auto shade_start = std::chrono::high_resolution_clock::now();
    perform_forward_shading(command_buffer, target);
    auto shade_end = std::chrono::high_resolution_clock::now();
    shading_time_ms_ = std::chrono::duration<float, std::milli>(shade_end - shade_start).count();
    
    // Perform transparent pass
    perform_transparent_pass(command_buffer, target);
}

void ForwardPlusPipeline::end_frame() {
    // Optional frame cleanup
}

void ForwardPlusPipeline::resize(uint32_t width, uint32_t height) {
    if (width == render_width_ && height == render_height_) {
        return; // No actual resize needed
    }
    
    render_width_ = width;
    render_height_ = height;
    
    // Recalculate tile grid
    tiles_x_ = (render_width_ + Constants::TILE_SIZE - 1) / Constants::TILE_SIZE;
    tiles_y_ = (render_height_ + Constants::TILE_SIZE - 1) / Constants::TILE_SIZE;
    
    // Rebuild GPU resources (depth buffer, framebuffers, etc.)
    // Implementation would recreate depth resources here
}

void ForwardPlusPipeline::shutdown() {
    if (!device_) return;
    
    vkDeviceWaitIdle(device_);
    
    // Destroy pipelines
    if (depth_prepass_pipeline_) vkDestroyPipeline(device_, depth_prepass_pipeline_, nullptr);
    if (forward_plus_pipeline_) vkDestroyPipeline(device_, forward_plus_pipeline_, nullptr);
    if (light_cull_compute_pipeline_) vkDestroyPipeline(device_, light_cull_compute_pipeline_, nullptr);
    
    // Destroy pipeline layouts
    if (depth_prepass_layout_) vkDestroyPipelineLayout(device_, depth_prepass_layout_, nullptr);
    if (forward_plus_layout_) vkDestroyPipelineLayout(device_, forward_plus_layout_, nullptr);
    
    // Destroy render passes
    if (depth_prepass_render_pass_) vkDestroyRenderPass(device_, depth_prepass_render_pass_, nullptr);
    if (forward_plus_render_pass_) vkDestroyRenderPass(device_, forward_plus_render_pass_, nullptr);
    
    // Destroy framebuffers
    for (auto fb : forward_plus_framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    
    // Destroy buffers
    destroy_buffer(light_data_buffer_, light_data_memory_);
    destroy_buffer(tile_light_index_buffer_, tile_light_index_memory_);
    destroy_buffer(tile_counters_buffer_, tile_counters_memory_);
    
    for (size_t i = 0; i < view_constant_buffers_.size(); ++i) {
        destroy_buffer(view_constant_buffers_[i], view_constant_memories_[i]);
    }
    
    for (size_t i = 0; i < lighting_constant_buffers_.size(); ++i) {
        destroy_buffer(lighting_constant_buffers_[i], lighting_constant_memories_[i]);
    }
    
    // Destroy images
    destroy_image(depth_prepass_image_, depth_prepass_view_, depth_prepass_memory_);
    
    // Destroy descriptor pool
    if (descriptor_pool_) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    if (descriptor_layout_) vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
    
    // Destroy query pool
    if (query_pool_) vkDestroyQueryPool(device_, query_pool_, nullptr);
    
    is_initialized_ = false;
}

// Private implementation methods

void ForwardPlusPipeline::create_depth_prepass_resources() {
    // Create depth pre-pass image, framebuffer, and render pass
    // This would contain full Vulkan resource creation code
}

void ForwardPlusPipeline::create_light_culling_resources() {
    // Create tile light index buffer and light data buffer
    // This would contain buffer allocation and initialization
    
    // Example structure:
    // tile_light_index_buffer: stores light indices for each tile
    // light_data_buffer: stores LightData structs
    // tile_counters_buffer: stores light count per tile
}

void ForwardPlusPipeline::create_pipelines() {
    // Create graphics pipelines for:
    // - Depth pre-pass
    // - Forward+ shading
    // - Transparent rendering
    
    // Create compute pipeline for light culling
}

void ForwardPlusPipeline::create_render_passes() {
    // Create render pass for depth pre-pass
    // Create render pass for forward+ shading
}

void ForwardPlusPipeline::create_descriptor_sets() {
    // Create descriptor set layouts
    // Allocate descriptor sets from pool
}

void ForwardPlusPipeline::create_uniform_buffers() {
    // Create view constant buffers (one per frame in flight)
    // Create lighting constant buffers (one per frame in flight)
}

void ForwardPlusPipeline::perform_depth_prepass(VkCommandBuffer cmd_buffer) {
    // Record depth pre-pass commands
    // Benefits: early-Z optimization, prevents shading invisible pixels
}

void ForwardPlusPipeline::perform_light_culling(VkCommandBuffer cmd_buffer) {
    // Dispatch compute shader to cull lights into tiles
    // Build light index lists for each 16x16 pixel tile
}

void ForwardPlusPipeline::perform_forward_shading(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    // Render all opaque objects with Forward+ lighting
    // Fragment shader samples light list from tile
}

void ForwardPlusPipeline::perform_transparent_pass(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    // Render transparent objects sorted back-to-front
}

void ForwardPlusPipeline::destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory) {
        vkFreeMemory(device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void ForwardPlusPipeline::destroy_image(VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
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

void ForwardPlusPipeline::destroy_pipeline(VkPipeline& pipeline) {
    if (pipeline) {
        vkDestroyPipeline(device_, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}

void ForwardPlusPipeline::destroy_render_pass(VkRenderPass& render_pass) {
    if (render_pass) {
        vkDestroyRenderPass(device_, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }
}

void ForwardPlusPipeline::destroy_framebuffers(std::vector<VkFramebuffer>& framebuffers) {
    for (auto& fb : framebuffers) {
        if (fb) {
            vkDestroyFramebuffer(device_, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    framebuffers.clear();
}

VkShaderModule ForwardPlusPipeline::load_shader_module(const std::string& filename) {
    // Load SPIR-V shader from file
    // Create VkShaderModule
    return VK_NULL_HANDLE; // Placeholder
}

void ForwardPlusPipeline::destroy_shader_module(VkShaderModule& shader) {
    if (shader) {
        vkDestroyShaderModule(device_, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }
}

} // namespace rendering_engine
