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

#include "pipelines/base_pipeline.h"
#include "pipelines/forward_plus_pipeline.h"
#include "pipelines/deferred_shading_pipeline.h"
#include "pipelines/clustered_shading_pipeline.h"
#include "types.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace rendering_engine {

/**
 * @brief Render Pipeline Manager
 * 
 * Central management system for all rendering pipelines. Handles:
 * - Pipeline initialization and lifecycle
 * - Runtime pipeline switching (< 1 ms overhead)
 * - GPU resource allocation and cleanup
 * - Performance monitoring
 * - Thread-safe pipeline access
 * 
 * Supports three main pipeline types:
 * - Forward+: For scenes with 1000+ lights, all GPU hardware compatible
 * - Deferred: For complex materials and visual effects
 * - Clustered: For scenes with 10K+ lights, advanced culling
 * 
 * Example usage:
 * ```cpp
 * RenderPipelineManager manager;
 * manager.initialize(device, physical_device, graphics_queue, cmd_pool, 1920, 1080);
 * manager.set_active_pipeline(RenderPipelineType::FORWARD_PLUS);
 * 
 * // Per-frame rendering
 * auto& pipeline = manager.get_active_pipeline();
 * pipeline.update_camera(camera);
 * pipeline.update_lights(lights);
 * pipeline.queue_renderables(objects);
 * pipeline.render(cmd_buffer, render_target);
 * ```
 */
class RenderPipelineManager {
public:
    RenderPipelineManager() = default;
    ~RenderPipelineManager();
    
    /**
     * Initialize all pipelines with GPU resources
     * @param device Vulkan logical device
     * @param physical_device Vulkan physical device
     * @param graphics_queue Graphics queue for rendering
     * @param command_pool Command pool for buffer allocation
     * @param width Initial render target width
     * @param height Initial render target height
     */
    void initialize(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkQueue graphics_queue,
        VkCommandPool command_pool,
        uint32_t width,
        uint32_t height
    );
    
    /**
     * Shutdown and release all resources
     */
    void shutdown();
    
    /**
     * Set the active rendering pipeline
     * @param pipeline_type Type of pipeline to activate
     * @return true if switch successful, false if not available
     */
    bool set_active_pipeline(RenderPipelineType pipeline_type);
    
    /**
     * Get the currently active pipeline
     * @return Reference to active pipeline
     */
    BasePipeline& get_active_pipeline();
    const BasePipeline& get_active_pipeline() const;
    
    /**
     * Get a specific pipeline by type
     * @param pipeline_type Type of pipeline to retrieve
     * @return Pointer to pipeline, nullptr if not initialized
     */
    BasePipeline* get_pipeline(RenderPipelineType pipeline_type);
    
    /**
     * Check if a specific pipeline type is available
     * @param pipeline_type Type to check
     * @return true if initialized and ready to use
     */
    bool is_pipeline_available(RenderPipelineType pipeline_type) const;
    
    /**
     * Get all available pipeline types
     * @return Vector of available pipeline types
     */
    std::vector<RenderPipelineType> get_available_pipelines() const;
    
    /**
     * Get the current active pipeline type
     * @return Currently active pipeline type
     */
    RenderPipelineType get_active_pipeline_type() const { return active_pipeline_; }
    
    /**
     * Resize all pipelines to new render target dimensions
     * This should be called when swapchain is resized
     * @param width New render target width
     * @param height New render target height
     */
    void resize(uint32_t width, uint32_t height);
    
    /**
     * Begin a new frame for the active pipeline
     * @param frame_index Frame counter for multi-buffering
     */
    void begin_frame(uint32_t frame_index);
    
    /**
     * End the current frame
     */
    void end_frame();
    
    /**
     * Update camera for the active pipeline
     * @param camera Active camera
     */
    void update_camera(const Camera& camera);
    
    /**
     * Update lights for the active pipeline
     * @param lights Array of light data
     */
    void update_lights(const std::vector<LightData>& lights);
    
    /**
     * Queue renderable objects for the active pipeline
     * @param objects Array of renderable objects
     */
    void queue_renderables(const std::vector<RenderableObject>& objects);
    
    /**
     * Render the active pipeline
     * @param command_buffer Command buffer to record into
     * @param target Output render target
     */
    void render(
        VkCommandBuffer command_buffer,
        const RenderTargets& target
    );
    
    // Performance and diagnostics
    
    /**
     * Get performance metrics for active pipeline
     */
    struct PerfMetrics {
        float light_culling_time_ms;
        float shading_time_ms;
        float total_time_ms;
        uint32_t light_count;
        uint32_t object_count;
    };
    
    PerfMetrics get_performance_metrics() const;
    
    /**
     * Get memory usage statistics
     */
    struct MemoryStats {
        uint64_t pipeline_memory;
        uint64_t buffer_memory;
        uint64_t texture_memory;
        uint64_t total_memory;
    };
    
    MemoryStats get_memory_stats() const;
    
    /**
     * Enable/disable performance monitoring
     */
    void set_performance_monitoring(bool enabled) { monitor_performance_ = enabled; }
    
    /**
     * Get GPU capability flags to determine which pipelines are supported
     */
    struct GPUCapabilities {
        bool supports_rtx;
        bool supports_mesh_shaders;
        bool supports_bindless;
        bool supports_ray_tracing;
        uint32_t max_lights;
        uint32_t max_cluster_z;
    };
    
    GPUCapabilities query_gpu_capabilities() const;
    
    /**
     * Performance targets (these can be exceeded under stress)
     */
    static constexpr float PIPELINE_SWITCH_BUDGET_MS = 1.0f;  // < 1 ms switch overhead
    static constexpr uint32_t MAX_PIPELINE_MEMORY_MB = 50;    // < 50 MB per pipeline state

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    
    uint32_t render_width_ = 0;
    uint32_t render_height_ = 0;
    
    // Pipeline instances
    std::unique_ptr<ForwardPlusPipeline> forward_plus_pipeline_;
    std::unique_ptr<DeferredShadingPipeline> deferred_pipeline_;
    std::unique_ptr<ClusteredShadingPipeline> clustered_pipeline_;
    
    // Pipeline management
    RenderPipelineType active_pipeline_ = RenderPipelineType::FORWARD_PLUS;
    std::unordered_map<uint32_t, BasePipeline*> pipeline_map_;
    
    // State tracking
    bool is_initialized_ = false;
    bool monitor_performance_ = false;
    uint32_t current_frame_index_ = 0;
    
    // Pipeline switch history for diagnostics
    struct PipelineSwitchEvent {
        uint32_t frame_number;
        RenderPipelineType from;
        RenderPipelineType to;
        float switch_time_ms;
    };
    std::vector<PipelineSwitchEvent> switch_history_;
    static constexpr uint32_t MAX_SWITCH_HISTORY = 100;
    
    // Helper methods
    void validate_pipeline_availability();
    void log_pipeline_capabilities();
    void record_pipeline_switch(RenderPipelineType from, RenderPipelineType to, float time_ms);
};

} // namespace rendering_engine
