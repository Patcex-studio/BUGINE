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

#include "../types.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace rendering_engine {

// Forward declarations
struct RenderableObject;
struct Camera;
struct LightData;

/**
 * @brief Base interface for all rendering pipelines
 * 
 * Defines the common interface that all pipeline types must implement.
 * Handles pipeline initialization, frame setup, rendering, and cleanup.
 */
class BasePipeline {
public:
    virtual ~BasePipeline();
    
    /**
     * Get current queued light count
     */
    uint32_t get_light_count() const;

    /**
     * Get current render queue object count
     */
    uint32_t get_render_queue_size() const;
    
    /**
     * Initialize the pipeline with GPU resources
     * @param device Vulkan logical device
     * @param physical_device Vulkan physical device
     * @param graphics_queue Graphics queue for rendering
     * @param command_pool Command pool for buffer allocation
     * @param width Render target width
     * @param height Render target height
     */
    virtual void initialize(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkQueue graphics_queue,
        VkCommandPool command_pool,
        uint32_t width,
        uint32_t height
    ) = 0;
    
    /**
     * Begin a new frame, resetting any frame-local state
     * @param frame_index Frame index for multi-buffering
     */
    virtual void begin_frame(uint32_t frame_index) = 0;
    
    /**
     * Update the camera for this frame
     * @param camera Active camera for rendering
     */
    virtual void update_camera(const Camera& camera) = 0;
    
    /**
     * Update lights for this frame
     * @param lights Array of light data
     */
    virtual void update_lights(const std::vector<LightData>& lights) = 0;
    
    /**
     * Add objects to render queue
     * @param objects Array of renderable objects
     */
    virtual void queue_renderables(const std::vector<RenderableObject>& objects) = 0;
    
    /**
     * Render the complete frame
     * @param command_buffer Command buffer to record into
     * @param target Output render target
     */
    virtual void render(
        VkCommandBuffer command_buffer,
        const RenderTargets& target
    ) = 0;
    
    /**
     * End frame and cleanup render state
     */
    virtual void end_frame() = 0;
    
    /**
     * Resize the pipeline to match new render target dimensions
     * @param width New width
     * @param height New height
     */
    virtual void resize(uint32_t width, uint32_t height) = 0;
    
    /**
     * Get the pipeline type
     * @return Current pipeline type
     */
    virtual RenderPipelineType get_pipeline_type() const = 0;
    
    /**
     * Get current light culling performance metrics
     * @return Time spent in light culling (milliseconds)
     */
    virtual float get_light_culling_time() const = 0;
    
    /**
     * Get current material shading performance metrics
     * @return Time spent in material shading (milliseconds)
     */
    virtual float get_shading_time() const = 0;
    
    /**
     * Shutdown and release all GPU resources
     */
    virtual void shutdown() = 0;
    
    // Performance targets for this pipeline class
    static constexpr float TARGET_LIGHT_CULLING_MS = 2.0f;    // < 2 ms for 10K lights
    static constexpr float TARGET_SHADING_MS = 3.0f;          // < 3 ms per object
    static constexpr float TARGET_TOTAL_MS = 16.667f;         // 60+ FPS (1000/60)

protected:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    
    uint32_t render_width_ = 0;
    uint32_t render_height_ = 0;
    
    Camera current_camera_;
    std::vector<LightData> current_lights_;
    std::vector<RenderableObject> render_queue_;
    
    float light_culling_time_ms_ = 0.0f;
    float shading_time_ms_ = 0.0f;
    uint32_t current_frame_index_ = 0;
    bool is_initialized_ = false;
};

} // namespace rendering_engine
