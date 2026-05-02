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
#include "rendering_engine/render_pipeline_manager.h"
#include <chrono>

namespace rendering_engine {

RenderPipelineManager::~RenderPipelineManager() {
    if (is_initialized_) {
        shutdown();
    }
}

void RenderPipelineManager::initialize(
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
    
    // Create all pipeline instances
    forward_plus_pipeline_ = std::make_unique<ForwardPlusPipeline>();
    deferred_pipeline_ = std::make_unique<DeferredShadingPipeline>();
    clustered_pipeline_ = std::make_unique<ClusteredShadingPipeline>();
    
    // Initialize all pipelines
    forward_plus_pipeline_->initialize(device, physical_device, graphics_queue, command_pool, width, height);
    deferred_pipeline_->initialize(device, physical_device, graphics_queue, command_pool, width, height);
    clustered_pipeline_->initialize(device, physical_device, graphics_queue, command_pool, width, height);
    
    // Build pipeline map
    pipeline_map_[static_cast<uint32_t>(RenderPipelineType::FORWARD_PLUS)] = forward_plus_pipeline_.get();
    pipeline_map_[static_cast<uint32_t>(RenderPipelineType::DEFERRED)] = deferred_pipeline_.get();
    pipeline_map_[static_cast<uint32_t>(RenderPipelineType::CLUSTERED)] = clustered_pipeline_.get();
    
    is_initialized_ = true;
    
    validate_pipeline_availability();
    log_pipeline_capabilities();
}

void RenderPipelineManager::shutdown() {
    if (!is_initialized_) return;
    
    // Shutdown all pipelines
    if (forward_plus_pipeline_) forward_plus_pipeline_->shutdown();
    if (deferred_pipeline_) deferred_pipeline_->shutdown();
    if (clustered_pipeline_) clustered_pipeline_->shutdown();
    
    forward_plus_pipeline_.reset();
    deferred_pipeline_.reset();
    clustered_pipeline_.reset();
    
    pipeline_map_.clear();
    is_initialized_ = false;
}

bool RenderPipelineManager::set_active_pipeline(RenderPipelineType pipeline_type) {
    auto it = pipeline_map_.find(static_cast<uint32_t>(pipeline_type));
    if (it == pipeline_map_.end() || it->second == nullptr) {
        return false; // Pipeline not available
    }
    
    auto switch_start = std::chrono::high_resolution_clock::now();
    
    RenderPipelineType old_pipeline = active_pipeline_;
    active_pipeline_ = pipeline_type;
    
    auto switch_end = std::chrono::high_resolution_clock::now();
    float switch_time = std::chrono::duration<float, std::milli>(switch_end - switch_start).count();
    
    record_pipeline_switch(old_pipeline, pipeline_type, switch_time);
    return true;
}

BasePipeline& RenderPipelineManager::get_active_pipeline() {
    auto it = pipeline_map_.find(static_cast<uint32_t>(active_pipeline_));
    if (it != pipeline_map_.end() && it->second != nullptr) {
        return *it->second;
    }
    
    // Fallback to Forward+ if active pipeline is invalid
    return *forward_plus_pipeline_;
}

const BasePipeline& RenderPipelineManager::get_active_pipeline() const {
    auto it = pipeline_map_.find(static_cast<uint32_t>(active_pipeline_));
    if (it != pipeline_map_.end() && it->second != nullptr) {
        return *it->second;
    }
    
    // Fallback to Forward+ if active pipeline is invalid
    return *forward_plus_pipeline_;
}

BasePipeline* RenderPipelineManager::get_pipeline(RenderPipelineType pipeline_type) {
    auto it = pipeline_map_.find(static_cast<uint32_t>(pipeline_type));
    if (it != pipeline_map_.end()) {
        return it->second;
    }
    return nullptr;
}

bool RenderPipelineManager::is_pipeline_available(RenderPipelineType pipeline_type) const {
    auto it = pipeline_map_.find(static_cast<uint32_t>(pipeline_type));
    return it != pipeline_map_.end() && it->second != nullptr;
}

std::vector<RenderPipelineType> RenderPipelineManager::get_available_pipelines() const {
    std::vector<RenderPipelineType> available;
    if (forward_plus_pipeline_) available.push_back(RenderPipelineType::FORWARD_PLUS);
    if (deferred_pipeline_) available.push_back(RenderPipelineType::DEFERRED);
    if (clustered_pipeline_) available.push_back(RenderPipelineType::CLUSTERED);
    return available;
}

void RenderPipelineManager::resize(uint32_t width, uint32_t height) {
    render_width_ = width;
    render_height_ = height;
    
    if (forward_plus_pipeline_) forward_plus_pipeline_->resize(width, height);
    if (deferred_pipeline_) deferred_pipeline_->resize(width, height);
    if (clustered_pipeline_) clustered_pipeline_->resize(width, height);
}

void RenderPipelineManager::begin_frame(uint32_t frame_index) {
    current_frame_index_ = frame_index;
    get_active_pipeline().begin_frame(frame_index);
}

void RenderPipelineManager::end_frame() {
    get_active_pipeline().end_frame();
}

void RenderPipelineManager::update_camera(const Camera& camera) {
    get_active_pipeline().update_camera(camera);
}

void RenderPipelineManager::update_lights(const std::vector<LightData>& lights) {
    get_active_pipeline().update_lights(lights);
}

void RenderPipelineManager::queue_renderables(const std::vector<RenderableObject>& objects) {
    get_active_pipeline().queue_renderables(objects);
}

void RenderPipelineManager::render(
    VkCommandBuffer command_buffer,
    const RenderTargets& target
) {
    get_active_pipeline().render(command_buffer, target);
}

RenderPipelineManager::PerfMetrics RenderPipelineManager::get_performance_metrics() const {
    const auto& pipeline = get_active_pipeline();
    
    return PerfMetrics{
        .light_culling_time_ms = pipeline.get_light_culling_time(),
        .shading_time_ms = pipeline.get_shading_time(),
        .total_time_ms = pipeline.get_light_culling_time() + pipeline.get_shading_time(),
        .light_count = pipeline.get_light_count(),
        .object_count = pipeline.get_render_queue_size()
    };
}

RenderPipelineManager::MemoryStats RenderPipelineManager::get_memory_stats() const {
    MemoryStats stats{};
    
    // Aggregate memory from all pipelines
    // This is a placeholder - real implementation would query Vulkan
    
    return stats;
}

RenderPipelineManager::GPUCapabilities RenderPipelineManager::query_gpu_capabilities() const {
    GPUCapabilities caps{
        .supports_rtx = false,                        // Would check VkPhysicalDeviceProperties
        .supports_mesh_shaders = false,              // Check device extensions
        .supports_bindless = false,                  // Check descriptor indexing
        .supports_ray_tracing = false,               // Check ray tracing extensions
        .max_lights = Constants::MAX_LIGHTS_FORWARD_PLUS,
        .max_cluster_z = Constants::MAX_CLUSTER_Z
    };
    
    return caps;
}

void RenderPipelineManager::validate_pipeline_availability() {
    // Check GPU capabilities and disable unavailable pipelines
    // For now, all pipelines are available
}

void RenderPipelineManager::log_pipeline_capabilities() {
    // Log which pipelines are available and their capabilities
    // Would use proper logging framework in production
}

void RenderPipelineManager::record_pipeline_switch(RenderPipelineType from, RenderPipelineType to, float time_ms) {
    if (switch_history_.size() >= MAX_SWITCH_HISTORY) {
        switch_history_.erase(switch_history_.begin()); // Remove oldest
    }
    
    switch_history_.push_back({
        .frame_number = current_frame_index_,
        .from = from,
        .to = to,
        .switch_time_ms = time_ms
    });
}

} // namespace rendering_engine
