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

namespace resource_manager {
    class AssetManager;
}

/**
 * @brief Deferred Shading Pipeline Implementation
 * 
 * Deferred Shading separates geometry rendering from lighting computation.
 * First pass renders scene properties (albedo, normal, depth, etc.) to G-buffers.
 * Second pass performs lighting using these cached properties with full PBR support.
 * 
 * Algorithm:
 * 1. G-buffer pass: Render all geometry to multiple render targets
 * 2. Light accumulation: Full-screen quad with light sampling
 * 3. Post-processing: Tonemapping, bloom, SSAO, TAA
 * 4. Forward pass: Transparent objects and decals
 * 
 * Performance targets:
 * - 60+ FPS with 100+ complex materials at 1920x1080
 * - G-buffer memory: < 16 bytes per pixel total
 * - Light evaluation: < 100 GB/s peak bandwidth usage
 * - Physically-based rendering (PBR) with metallic/roughness workflow
 * 
 * Advantages:
 * - Decouples light count from material complexity
 * - Full PBR support with unlimited material variations
 * - Better for complex material compositions
 * - MSAA-compatible with proper encoding
 * 
 * Disadvantages:
 * - Higher memory bandwidth for G-buffer access
 * - Cannot handle complex transparency efficiently
 * - Requires post-processing for TAA, SSAO, bloom
 */
class DeferredShadingPipeline : public BasePipeline {
public:
    DeferredShadingPipeline() = default;
    ~DeferredShadingPipeline() override;
    
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
    RenderPipelineType get_pipeline_type() const override { return RenderPipelineType::DEFERRED; }
    
    float get_light_culling_time() const override { return light_culling_time_ms_; }
    float get_shading_time() const override { return shading_time_ms_; }
    
    void shutdown() override;
    
    /**
     * Get memory usage of G-buffer in bytes
     */
    uint64_t get_gbuffer_memory_usage() const {
        return static_cast<uint64_t>(render_width_) * render_height_ * 16; // 16 bytes per pixel
    }
    
    /**
     * Enable/disable specific G-buffer visualizations for debugging
     */
    void set_debug_visualization(int32_t debug_target);

private:
    // G-buffer resources
    GBufferAttachments gbuffer_;
    VkDeviceMemory gbuffer_albedo_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory gbuffer_normal_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory gbuffer_material_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory gbuffer_depth_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory gbuffer_velocity_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory gbuffer_object_id_memory_ = VK_NULL_HANDLE;
    
    // Additional render targets for post-processing
    struct PostProcessTargets {
        VkImage bloom_hdr;
        VkImageView bloom_hdr_view;
        VkDeviceMemory bloom_hdr_memory;
        
        VkImage lighting_result;
        VkImageView lighting_result_view;
        VkDeviceMemory lighting_result_memory;
    } post_targets_;
    
    // Pipelines
    VkPipeline gbuffer_pipeline_ = VK_NULL_HANDLE;
    VkPipeline lighting_pipeline_ = VK_NULL_HANDLE;
    VkPipeline deferred_transparent_pipeline_ = VK_NULL_HANDLE;
    VkPipeline post_process_pipeline_ = VK_NULL_HANDLE;
    VkPipeline taa_pipeline_ = VK_NULL_HANDLE;
    VkPipeline ssao_pipeline_ = VK_NULL_HANDLE;
    VkPipeline bloom_pipeline_ = VK_NULL_HANDLE;
    
    VkPipelineLayout gbuffer_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout lighting_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout post_process_pipeline_layout_ = VK_NULL_HANDLE;
    
    // Render passes
    VkRenderPass gbuffer_render_pass_ = VK_NULL_HANDLE;
    VkRenderPass lighting_render_pass_ = VK_NULL_HANDLE;
    VkRenderPass post_process_render_pass_ = VK_NULL_HANDLE;
    VkRenderPass transparent_render_pass_ = VK_NULL_HANDLE;
    
    // Framebuffers
    std::vector<VkFramebuffer> gbuffer_framebuffers_;
    std::vector<VkFramebuffer> lighting_framebuffers_;
    
    // Descriptor sets and layouts
    VkDescriptorSetLayout gbuffer_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout lighting_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout post_process_descriptor_layout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> gbuffer_descriptor_sets_;
    std::vector<VkDescriptorSet> lighting_descriptor_sets_;
    VkDescriptorSet post_process_descriptor_set_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkSampler texture_sampler_ = VK_NULL_HANDLE;
    VkFormat swapchain_color_format_ = VK_FORMAT_UNDEFINED;
    
    // Uniform buffers
    std::vector<VkBuffer> view_constant_buffers_;
    std::vector<VkDeviceMemory> view_constant_memories_;
    
    std::vector<VkBuffer> lighting_constant_buffers_;
    std::vector<VkDeviceMemory> lighting_constant_memories_;
    
    // Light data buffers
    VkBuffer light_data_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory light_data_memory_ = VK_NULL_HANDLE;
    
    // Rendering state
    std::vector<RenderableObject> opaque_queue_;
    std::vector<RenderableObject> transparent_queue_;
    
    // Asset manager for mesh/material access
    resource_manager::AssetManager* asset_manager_ = nullptr;
    
    // Debug state
    int32_t debug_visualization_target_ = -1; // -1 = disabled
    
    // Private helper methods
    void create_gbuffer_resources();
    void create_light_data_buffer();
    void create_pipelines();
    void create_render_passes();
    void create_post_process_render_pass(VkFormat swapchain_format);
    void create_post_process_pipeline();
    void create_framebuffers();
    void create_descriptor_sets();
    void create_uniform_buffers();
    void create_post_process_targets();
    
    void perform_gbuffer_pass(VkCommandBuffer cmd_buffer);
    void perform_lighting_pass(VkCommandBuffer cmd_buffer);
    void perform_post_processing(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    void perform_transparent_pass(VkCommandBuffer cmd_buffer);
    void perform_taa(VkCommandBuffer cmd_buffer);
    void perform_bloom(VkCommandBuffer cmd_buffer);
    void perform_ssao(VkCommandBuffer cmd_buffer);
    void perform_tonemapping(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    
    void destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory);
    void destroy_image(VkImage& image, VkImageView& view, VkDeviceMemory& memory);
    void destroy_pipeline(VkPipeline& pipeline);
    void destroy_render_pass(VkRenderPass& render_pass);
    void destroy_framebuffers(std::vector<VkFramebuffer>& framebuffers);
    
    // Shader modules
    VkShaderModule load_shader_module(const std::string& filename);
    void destroy_shader_module(VkShaderModule& shader);
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Timing
    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    uint32_t query_count_ = 0;
    
    // Previous frame velocity for TAA
    VkImage prev_velocity_;
    VkImageView prev_velocity_view_;
    VkDeviceMemory prev_velocity_memory_;
};

} // namespace rendering_engine
