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

#include "base_pipeline.h"
#include "../types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace rendering_engine {

/**
 * @brief Forward+ Rendering Pipeline Implementation
 * 
 * Forward+ (also called Tiled Forward) divides the screen into tiles and assigns
 * lights to each tile using a compute shader. This enables rendering with up to
 * 1000+ dynamic lights efficiently on all Vulkan-compatible hardware.
 * 
 * Algorithm:
 * 1. Depth pre-pass: Z-only rendering for early-Z optimization
 * 2. Light culling: Compute shader assigns lights to screen tiles (16x16 pixels)
 * 3. Main pass: Fragment shader samples only relevant light list per tile
 * 4. Transparent pass: Sorted back-to-front rendering
 * 
 * Performance targets:
 * - 60+ FPS with 1000+ dynamic lights at 1920x1080
 * - No G-buffer memory overhead (only depth pre-pass buffer)
 * - Tile light culling: < 2 ms for 10K lights
 * 
 * Trade-offs:
 * - No complex material deferred options
 * - Transparency limited to single-pass blending
 * - Light evaluation per-fragment (not per-tile for shading)
 */
class ForwardPlusPipeline : public BasePipeline {
public:
    ForwardPlusPipeline() = default;
    ~ForwardPlusPipeline() override;
    
    // BasePipeline implementation
    void initialize(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkQueue graphics_queue,
        VkCommandPool command_pool,
        uint32_t width,
        uint32_t height
    ) override;
    
    void begin_frame(uint32_t frame_index) override;
    void update_camera(const Camera& camera) override;
    void update_lights(const std::vector<LightData>& lights) override;
    void queue_renderables(const std::vector<RenderableObject>& objects) override;
    
    void render(
        VkCommandBuffer command_buffer,
        const RenderTargets& target
    ) override;
    
    void end_frame() override;
    void resize(uint32_t width, uint32_t height) override;
    RenderPipelineType get_pipeline_type() const override { return RenderPipelineType::FORWARD_PLUS; }
    
    float get_light_culling_time() const override { return light_culling_time_ms_; }
    float get_shading_time() const override { return shading_time_ms_; }
    
    void shutdown() override;
    
    /**
     * Get total number of tiles generated for current render target
     */
    uint32_t get_tile_count() const { return tiles_x_ * tiles_y_; }
    
    /**
     * Get tile dimensions in pixels
     */
    void get_tile_dimensions(uint32_t& tile_x, uint32_t& tile_y) const {
        tile_x = tiles_x_;
        tile_y = tiles_y_;
    }

private:
    // Pipeline resources
    VkPipeline depth_prepass_pipeline_ = VK_NULL_HANDLE;
    VkPipeline forward_plus_pipeline_ = VK_NULL_HANDLE;
    VkPipeline transparent_pipeline_ = VK_NULL_HANDLE;
    
    VkPipelineLayout depth_prepass_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout forward_plus_layout_ = VK_NULL_HANDLE;
    
    // Depth pre-pass resources
    VkImage depth_prepass_image_ = VK_NULL_HANDLE;
    VkImageView depth_prepass_view_ = VK_NULL_HANDLE;
    VkDeviceMemory depth_prepass_memory_ = VK_NULL_HANDLE;
    VkFramebuffer depth_prepass_framebuffer_ = VK_NULL_HANDLE;
    VkRenderPass depth_prepass_render_pass_ = VK_NULL_HANDLE;
    
    // Light culling resources
    VkBuffer tile_light_index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tile_light_index_memory_ = VK_NULL_HANDLE;
    
    VkBuffer light_data_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory light_data_memory_ = VK_NULL_HANDLE;
    
    VkBuffer tile_counters_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tile_counters_memory_ = VK_NULL_HANDLE;
    
    VkPipeline light_cull_compute_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout light_cull_layout_ = VK_NULL_HANDLE;
    
    // Descriptor sets
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor_sets_;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    
    // Render pass and framebuffers
    VkRenderPass forward_plus_render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> forward_plus_framebuffers_;
    
    // Uniform buffers
    std::vector<VkBuffer> view_constant_buffers_;
    std::vector<VkDeviceMemory> view_constant_memories_;
    
    std::vector<VkBuffer> lighting_constant_buffers_;
    std::vector<VkDeviceMemory> lighting_constant_memories_;
    
    // Tile information
    uint32_t tiles_x_ = 0;
    uint32_t tiles_y_ = 0;
    
    // Rendering state
    std::vector<RenderableObject> opaque_queue_;
    std::vector<RenderableObject> transparent_queue_;
    
    // Private helper methods
    void create_depth_prepass_resources();
    void create_light_culling_resources();
    void create_pipelines();
    void create_render_passes();
    void create_descriptor_sets();
    void create_uniform_buffers();
    
    void perform_depth_prepass(VkCommandBuffer cmd_buffer);
    void perform_light_culling(VkCommandBuffer cmd_buffer);
    void perform_forward_shading(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    void perform_transparent_pass(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    
    void destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory);
    void destroy_image(VkImage& image, VkImageView& view, VkDeviceMemory& memory);
    void destroy_pipeline(VkPipeline& pipeline);
    void destroy_render_pass(VkRenderPass& render_pass);
    void destroy_framebuffers(std::vector<VkFramebuffer>& framebuffers);
    
    // Shader modules
    VkShaderModule load_shader_module(const std::string& filename);
    void destroy_shader_module(VkShaderModule& shader);
    
    // Timing
    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    uint32_t query_count_ = 0;
};

} // namespace rendering_engine
