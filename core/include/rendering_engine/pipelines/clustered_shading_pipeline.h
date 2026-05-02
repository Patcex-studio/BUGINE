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
 * @brief Clustered Shading Pipeline Implementation
 * 
 * Clustered Shading is an advanced technique that divides the view frustum into
 * 3D clusters and assigns lights to each cluster. This is superior to Forward+
 * for very large light counts and complex scenes with spatially-complex lighting.
 * 
 * Algorithm:
 * 1. Frustum subdivision: 3D grid aligned with camera frustum
 * 2. Cluster creation: Compute shader builds light lists per cluster
 * 3. Visibility culling: Per-cluster light visibility testing
 * 4. Shading pass: Fragment shader samples cluster light lists
 * 
 * Performance targets:
 * - 60+ FPS with 10K+ lights at 1920x1080
 * - Compact cluster data structures (< 100MB for full scene)
 * - Real-time cluster updates for dynamic light changes
 * - Better scalability than Forward+ with 5K+ lights
 * 
 * Advantages:
 * - Handles 10K+ lights efficiently (better than Forward+ at scale)
 * - 3D culling more accurate than screen-space tiles
 * - Works well with complex scenes (buildings, indoors)
 * - Enables advanced rendering techniques (volumetric lighting)
 * 
 * Disadvantages:
 * - Higher initial setup cost than Forward+
 * - Complex cluster management and updates
 * - Requires more advanced shader logic
 * - Not ideal for outdoor open-world scenes
 */
class ClusteredShadingPipeline : public BasePipeline {
public:
    ClusteredShadingPipeline() = default;
    ~ClusteredShadingPipeline() override;
    
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
    RenderPipelineType get_pipeline_type() const override { return RenderPipelineType::CLUSTERED; }
    
    float get_light_culling_time() const override { return light_culling_time_ms_; }
    float get_shading_time() const override { return shading_time_ms_; }
    
    void shutdown() override;
    
    /**
     * Get total number of clusters in the 3D grid
     */
    uint32_t get_cluster_count() const { return cluster_grid_x_ * cluster_grid_y_ * cluster_grid_z_; }
    
    /**
     * Get cluster grid dimensions
     */
    void get_cluster_dimensions(uint32_t& x, uint32_t& y, uint32_t& z) const {
        x = cluster_grid_x_;
        y = cluster_grid_y_;
        z = cluster_grid_z_;
    }

private:
    // Cluster grid configuration
    uint32_t cluster_grid_x_ = Constants::MAX_CLUSTER_X;
    uint32_t cluster_grid_y_ = Constants::MAX_CLUSTER_Y;
    uint32_t cluster_grid_z_ = Constants::MAX_CLUSTER_Z;
    float cluster_size_ = 0.1f; // Adjustable cluster size
    
    // Cluster culling resources
    struct ClusterData {
        std::vector<uint32_t> light_indices;    // Per-cluster light indices
        std::vector<uint32_t> light_counts;     // Per-cluster light counts
    };
    ClusterData cluster_data_;
    
    VkBuffer cluster_light_indices_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory cluster_light_indices_memory_ = VK_NULL_HANDLE;
    
    VkBuffer cluster_light_counts_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory cluster_light_counts_memory_ = VK_NULL_HANDLE;
    
    VkBuffer light_data_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory light_data_memory_ = VK_NULL_HANDLE;
    
    VkBuffer cluster_aabb_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory cluster_aabb_memory_ = VK_NULL_HANDLE;
    
    // Compute pipelines for cluster operations
    VkPipeline cluster_culling_compute_pipeline_ = VK_NULL_HANDLE;
    VkPipeline cluster_light_assignment_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout cluster_cull_layout_ = VK_NULL_HANDLE;
    
    // Main rendering pipelines
    VkPipeline clustered_shading_pipeline_ = VK_NULL_HANDLE;
    VkPipeline clustered_transparent_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout clustered_shading_layout_ = VK_NULL_HANDLE;
    
    // Render passes
    VkRenderPass clustered_render_pass_ = VK_NULL_HANDLE;
    VkRenderPass transparent_render_pass_ = VK_NULL_HANDLE;
    
    // Framebuffers
    std::vector<VkFramebuffer> clustered_framebuffers_;
    std::vector<VkFramebuffer> transparent_framebuffers_;
    
    // Descriptor sets and layouts
    VkDescriptorSetLayout cluster_cull_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout clustered_shading_descriptor_layout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> cluster_cull_descriptor_sets_;
    std::vector<VkDescriptorSet> clustered_shading_descriptor_sets_;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    
    // Uniform buffers
    std::vector<VkBuffer> view_constant_buffers_;
    std::vector<VkDeviceMemory> view_constant_memories_;
    
    std::vector<VkBuffer> lighting_constant_buffers_;
    std::vector<VkDeviceMemory> lighting_constant_memories_;
    
    // Rendering state
    std::vector<RenderableObject> opaque_queue_;
    std::vector<RenderableObject> transparent_queue_;
    
    // Debug resources
    VkImage cluster_debug_image_ = VK_NULL_HANDLE;
    VkImageView cluster_debug_view_ = VK_NULL_HANDLE;
    VkDeviceMemory cluster_debug_memory_ = VK_NULL_HANDLE;
    VkFramebuffer cluster_debug_framebuffer_ = VK_NULL_HANDLE;
    
    // Private helper methods
    void create_cluster_resources();
    void create_light_data_buffer();
    void create_compute_pipelines();
    void create_shading_pipelines();
    void create_render_passes();
    void create_framebuffers();
    void create_descriptor_sets();
    void create_uniform_buffers();
    
    void compute_cluster_grid_dimensions();
    void compute_cluster_aabb();
    
    void perform_cluster_culling(VkCommandBuffer cmd_buffer);
    void perform_light_assignment(VkCommandBuffer cmd_buffer);
    void perform_clustered_shading(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    void perform_transparent_pass(VkCommandBuffer cmd_buffer, const RenderTargets& target);
    
    void rebuild_clusters_if_needed();
    bool is_cluster_rebuild_needed() const;
    
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
    
    // Camera frustum for clustering
    struct FrustumBounds {
        glm::vec3 near_plane_center;
        glm::vec3 far_plane_center;
        float near_distance;
        float far_distance;
    } frustum_bounds_;
};

} // namespace rendering_engine
