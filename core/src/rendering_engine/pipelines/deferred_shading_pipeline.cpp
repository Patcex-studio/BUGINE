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
#include "rendering_engine/pipelines/deferred_shading_pipeline.h"
#include "rendering_engine/resource_management.h"
#include "rendering_engine/resource_manager/asset_manager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace rendering_engine {

DeferredShadingPipeline::~DeferredShadingPipeline() {
    if (is_initialized_) {
        shutdown();
    }
}

void DeferredShadingPipeline::initialize(
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
    
    // Get asset manager from singleton
    asset_manager_ = &resource_manager::ResourceManager::get_instance().get_asset_manager();
    
    // Initialize Vulkan resources
    create_gbuffer_resources();
    create_light_data_buffer();
    create_post_process_targets();
    create_uniform_buffers();
    create_render_passes();
    create_descriptor_sets();
    create_pipelines();
    create_framebuffers();
    
    is_initialized_ = true;
}

void DeferredShadingPipeline::begin_frame(uint32_t frame_index) {
    current_frame_index_ = frame_index;
    render_queue_.clear();
    opaque_queue_.clear();
    transparent_queue_.clear();
    light_culling_time_ms_ = 0.0f;
    shading_time_ms_ = 0.0f;

    if (query_pool_ && query_count_ > 0) {
        vkResetQueryPool(device_, query_pool_, 0, query_count_);
    }
}

void DeferredShadingPipeline::update_camera(const Camera& camera) {
    current_camera_ = camera;

    auto normalize_plane = [](const glm::vec4& plane) {
        float inv_length = 1.0f / glm::length(glm::vec3(plane));
        return plane * inv_length;
    };

    const glm::mat4& m = camera.view_projection_matrix;
    current_camera_.frustum_planes[0] = normalize_plane(glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0])); // left
    current_camera_.frustum_planes[1] = normalize_plane(glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0])); // right
    current_camera_.frustum_planes[2] = normalize_plane(glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1])); // bottom
    current_camera_.frustum_planes[3] = normalize_plane(glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1])); // top
    current_camera_.frustum_planes[4] = normalize_plane(glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2])); // near
    current_camera_.frustum_planes[5] = normalize_plane(glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2])); // far

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
            .screen_dimensions = glm::vec4(static_cast<float>(render_width_), static_cast<float>(render_height_), 1.0f / static_cast<float>(render_width_), 1.0f / static_cast<float>(render_height_)),
            .time_params = glm::vec4(0.0f, 0.0f, static_cast<float>(current_frame_index_), 0.0f)
        };

        void* data;
        vkMapMemory(device_, view_constant_memories_[current_frame_index_], 0, sizeof(ViewConstants), 0, &data);
        memcpy(data, &constants, sizeof(ViewConstants));
        vkUnmapMemory(device_, view_constant_memories_[current_frame_index_]);
    }
}

void DeferredShadingPipeline::update_lights(const std::vector<LightData>& lights) {
    current_lights_ = lights;

    uint32_t light_count = static_cast<uint32_t>(lights.size() > Constants::MAX_LIGHTS_FORWARD_PLUS ? Constants::MAX_LIGHTS_FORWARD_PLUS : lights.size());
    if (light_count > 0 && light_data_buffer_) {
        void* data;
        vkMapMemory(device_, light_data_memory_, 0, light_count * sizeof(LightData), 0, &data);
        memcpy(data, lights.data(), light_count * sizeof(LightData));
        vkUnmapMemory(device_, light_data_memory_);
    }

    LightingConstants lightingConstants{};
    lightingConstants.ambient_color = glm::vec4(0.02f, 0.02f, 0.02f, 1.0f);
    lightingConstants.light_count = light_count;
    lightingConstants.max_lights_per_tile = Constants::MAX_LIGHTS_FORWARD_PLUS;
    lightingConstants.tile_size_x = 16;
    lightingConstants.tile_size_y = 16;
    lightingConstants.cluster_grid_x = 0;
    lightingConstants.cluster_grid_y = 0;
    lightingConstants.cluster_grid_z = 0;
    lightingConstants.cluster_size = 0.0f;

    if (current_frame_index_ < lighting_constant_buffers_.size()) {
        void* buffer_data;
        vkMapMemory(device_, lighting_constant_memories_[current_frame_index_], 0, sizeof(LightingConstants), 0, &buffer_data);
        memcpy(buffer_data, &lightingConstants, sizeof(LightingConstants));
        vkUnmapMemory(device_, lighting_constant_memories_[current_frame_index_]);
    }
}

void DeferredShadingPipeline::queue_renderables(const std::vector<RenderableObject>& objects) {
    opaque_queue_.clear();
    transparent_queue_.clear();

    for (const auto& obj : objects) {
        // All objects are opaque by default until material flags are available.
        opaque_queue_.push_back(obj);
    }

    render_queue_ = objects;
}

void DeferredShadingPipeline::render(
    VkCommandBuffer command_buffer,
    const RenderTargets& target
) {
    if (!is_initialized_) return;
    
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    // G-buffer pass
    perform_gbuffer_pass(command_buffer);
    
    // Lighting pass
    auto lighting_start = std::chrono::high_resolution_clock::now();
    perform_lighting_pass(command_buffer);
    auto lighting_end = std::chrono::high_resolution_clock::now();
    light_culling_time_ms_ = std::chrono::duration<float, std::milli>(lighting_end - lighting_start).count();
    
    // Optional post-processing passes
    perform_ssao(command_buffer);
    perform_transparent_pass(command_buffer);
    perform_taa(command_buffer);
    perform_bloom(command_buffer);
    perform_tonemapping(command_buffer, target);
    
    auto frame_end = std::chrono::high_resolution_clock::now();
    shading_time_ms_ = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
}

void DeferredShadingPipeline::end_frame() {
    // Optional frame cleanup
}

void DeferredShadingPipeline::resize(uint32_t width, uint32_t height) {
    if (width == render_width_ && height == render_height_) {
        return;
    }
    
    render_width_ = width;
    render_height_ = height;
    
    // Rebuild G-buffer and render targets
    shutdown();
    initialize(device_, physical_device_, graphics_queue_, command_pool_, width, height);
}

void DeferredShadingPipeline::set_debug_visualization(int32_t debug_target) {
    debug_visualization_target_ = debug_target;
}

void DeferredShadingPipeline::shutdown() {
    if (!device_) return;
    
    vkDeviceWaitIdle(device_);
    
    // Destroy pipelines
    if (gbuffer_pipeline_) vkDestroyPipeline(device_, gbuffer_pipeline_, nullptr);
    if (lighting_pipeline_) vkDestroyPipeline(device_, lighting_pipeline_, nullptr);
    if (post_process_pipeline_) vkDestroyPipeline(device_, post_process_pipeline_, nullptr);
    if (taa_pipeline_) vkDestroyPipeline(device_, taa_pipeline_, nullptr);
    if (ssao_pipeline_) vkDestroyPipeline(device_, ssao_pipeline_, nullptr);
    if (bloom_pipeline_) vkDestroyPipeline(device_, bloom_pipeline_, nullptr);
    
    // Destroy pipeline layouts
    if (gbuffer_layout_) vkDestroyPipelineLayout(device_, gbuffer_layout_, nullptr);
    if (lighting_layout_) vkDestroyPipelineLayout(device_, lighting_layout_, nullptr);
    if (post_process_pipeline_layout_) vkDestroyPipelineLayout(device_, post_process_pipeline_layout_, nullptr);
    
    // Destroy render passes
    if (gbuffer_render_pass_) vkDestroyRenderPass(device_, gbuffer_render_pass_, nullptr);
    if (lighting_render_pass_) vkDestroyRenderPass(device_, lighting_render_pass_, nullptr);
    if (post_process_render_pass_) vkDestroyRenderPass(device_, post_process_render_pass_, nullptr);
    if (transparent_render_pass_) vkDestroyRenderPass(device_, transparent_render_pass_, nullptr);
    
    // Destroy framebuffers
    for (auto fb : gbuffer_framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    for (auto fb : lighting_framebuffers_) {
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    
    // Destroy G-buffer images
    destroy_image(gbuffer_.albedo_roughness, gbuffer_.albedo_roughness_view, gbuffer_albedo_memory_);
    destroy_image(gbuffer_.normal, gbuffer_.normal_view, gbuffer_normal_memory_);
    destroy_image(gbuffer_.material_data, gbuffer_.material_data_view, gbuffer_material_memory_);
    destroy_image(gbuffer_.depth, gbuffer_.depth_view, gbuffer_depth_memory_);
    destroy_image(gbuffer_.velocity, gbuffer_.velocity_view, gbuffer_velocity_memory_);
    destroy_image(gbuffer_.object_id, gbuffer_.object_id_view, gbuffer_object_id_memory_);
    
    // Destroy post-process targets
    destroy_image(post_targets_.bloom_hdr, post_targets_.bloom_hdr_view, post_targets_.bloom_hdr_memory);
    destroy_image(post_targets_.lighting_result, post_targets_.lighting_result_view, post_targets_.lighting_result_memory);
    
    // Destroy buffers
    destroy_buffer(light_data_buffer_, light_data_memory_);
    
    for (size_t i = 0; i < view_constant_buffers_.size(); ++i) {
        destroy_buffer(view_constant_buffers_[i], view_constant_memories_[i]);
    }
    
    for (size_t i = 0; i < lighting_constant_buffers_.size(); ++i) {
        destroy_buffer(lighting_constant_buffers_[i], lighting_constant_memories_[i]);
    }
    
    // Destroy descriptor pool and layouts
    if (descriptor_pool_) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    if (gbuffer_descriptor_layout_) vkDestroyDescriptorSetLayout(device_, gbuffer_descriptor_layout_, nullptr);
    if (lighting_descriptor_layout_) vkDestroyDescriptorSetLayout(device_, lighting_descriptor_layout_, nullptr);
    if (post_process_descriptor_layout_) vkDestroyDescriptorSetLayout(device_, post_process_descriptor_layout_, nullptr);
    if (texture_sampler_) vkDestroySampler(device_, texture_sampler_, nullptr);
    
    // Destroy query pool
    if (query_pool_) vkDestroyQueryPool(device_, query_pool_, nullptr);
    
    is_initialized_ = false;
}

// Private implementation methods

void DeferredShadingPipeline::create_gbuffer_resources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = render_width_;
    imageInfo.extent.height = render_height_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    
    // Albedo/Roughness
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.albedo_roughness);
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, gbuffer_.albedo_roughness, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_albedo_memory_);
    vkBindImageMemory(device_, gbuffer_.albedo_roughness, gbuffer_albedo_memory_, 0);
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = gbuffer_.albedo_roughness;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.albedo_roughness_view);
    
    // Normal
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.normal);
    vkGetImageMemoryRequirements(device_, gbuffer_.normal, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_normal_memory_);
    vkBindImageMemory(device_, gbuffer_.normal, gbuffer_normal_memory_, 0);
    viewInfo.image = gbuffer_.normal;
    viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.normal_view);
    
    // Material data
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.material_data);
    vkGetImageMemoryRequirements(device_, gbuffer_.material_data, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_material_memory_);
    vkBindImageMemory(device_, gbuffer_.material_data, gbuffer_material_memory_, 0);
    viewInfo.image = gbuffer_.material_data;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.material_data_view);
    
    // Depth
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.depth);
    vkGetImageMemoryRequirements(device_, gbuffer_.depth, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_depth_memory_);
    vkBindImageMemory(device_, gbuffer_.depth, gbuffer_depth_memory_, 0);
    viewInfo.image = gbuffer_.depth;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.depth_view);
    
    // Velocity
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.velocity);
    vkGetImageMemoryRequirements(device_, gbuffer_.velocity, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_velocity_memory_);
    vkBindImageMemory(device_, gbuffer_.velocity, gbuffer_velocity_memory_, 0);
    viewInfo.image = gbuffer_.velocity;
    viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.velocity_view);
    
    // Object ID
    imageInfo.format = VK_FORMAT_R32_UINT;
    vkCreateImage(device_, &imageInfo, nullptr, &gbuffer_.object_id);
    vkGetImageMemoryRequirements(device_, gbuffer_.object_id, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gbuffer_object_id_memory_);
    vkBindImageMemory(device_, gbuffer_.object_id, gbuffer_object_id_memory_, 0);
    viewInfo.image = gbuffer_.object_id;
    viewInfo.format = VK_FORMAT_R32_UINT;
    vkCreateImageView(device_, &viewInfo, nullptr, &gbuffer_.object_id_view);
}

void DeferredShadingPipeline::create_light_data_buffer() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = Constants::MAX_LIGHTS_FORWARD_PLUS * sizeof(LightData);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    vkCreateBuffer(device_, &bufferInfo, nullptr, &light_data_buffer_);
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, light_data_buffer_, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    vkAllocateMemory(device_, &allocInfo, nullptr, &light_data_memory_);
    vkBindBufferMemory(device_, light_data_buffer_, light_data_memory_, 0);
}

void DeferredShadingPipeline::create_pipelines() {
    // Load shaders
    VkShaderModule gbufferVert = load_shader_module("core/shaders/deferred/gbuffer.vert.spv");
    VkShaderModule gbufferFrag = load_shader_module("core/shaders/deferred/gbuffer.frag.spv");
    VkShaderModule lightingVert = load_shader_module("core/shaders/deferred/lighting.vert.spv");
    VkShaderModule lightingFrag = load_shader_module("core/shaders/deferred/lighting.frag.spv");
    
    // G-buffer pipeline
    VkPipelineShaderStageCreateInfo gbufferStages[2] = {};
    gbufferStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    gbufferStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    gbufferStages[0].module = gbufferVert;
    gbufferStages[0].pName = "main";
    gbufferStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    gbufferStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    gbufferStages[1].module = gbufferFrag;
    gbufferStages[1].pName = "main";
    
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::array<VkVertexInputAttributeDescription, 5> attrDescs = {};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, normal);
    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[2].offset = offsetof(Vertex, tangent);
    attrDescs[3].binding = 0;
    attrDescs[3].location = 3;
    attrDescs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[3].offset = offsetof(Vertex, texcoord);
    attrDescs[4].binding = 0;
    attrDescs[4].location = 4;
    attrDescs[4].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrDescs[4].offset = offsetof(Vertex, color);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = attrDescs.size();
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)render_width_;
    viewport.height = (float)render_height_;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {render_width_, render_height_};
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    std::array<VkPipelineColorBlendAttachmentState, 5> colorBlendAttachments{};
    for (auto& attachment : colorBlendAttachments) {
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = VK_FALSE;
    }
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = colorBlendAttachments.size();
    colorBlending.pAttachments = colorBlendAttachments.data();
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &gbuffer_descriptor_layout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4) + sizeof(glm::vec4) * 2;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    
    vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &gbuffer_layout_);
    
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = gbufferStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = gbuffer_layout_;
    pipelineInfo.renderPass = gbuffer_render_pass_;
    pipelineInfo.subpass = 0;
    
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gbuffer_pipeline_);
    
    // Lighting pipeline (fullscreen quad)
    VkPipelineShaderStageCreateInfo lightingStages[2] = {};
    lightingStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lightingStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    lightingStages[0].module = lightingVert;
    lightingStages[0].pName = "main";
    lightingStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lightingStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    lightingStages[1].module = lightingFrag;
    lightingStages[1].pName = "main";
    
    VkPipelineVertexInputStateCreateInfo lightingVertexInput{};
    lightingVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    VkPipelineInputAssemblyStateCreateInfo lightingInputAssembly{};
    lightingInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    lightingInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    VkPipelineRasterizationStateCreateInfo lightingRasterizer = rasterizer;
    lightingRasterizer.cullMode = VK_CULL_MODE_NONE;
    
    VkPipelineDepthStencilStateCreateInfo lightingDepthStencil{};
    lightingDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    lightingDepthStencil.depthTestEnable = VK_FALSE;
    lightingDepthStencil.depthWriteEnable = VK_FALSE;
    
    VkPipelineColorBlendAttachmentState lightingBlendAttachment{};
    lightingBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    lightingBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo lightingColorBlending{};
    lightingColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    lightingColorBlending.attachmentCount = 1;
    lightingColorBlending.pAttachments = &lightingBlendAttachment;
    
    VkPipelineLayoutCreateInfo lightingLayoutInfo{};
    lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lightingLayoutInfo.setLayoutCount = 1;
    lightingLayoutInfo.pSetLayouts = &lighting_descriptor_layout_;
    
    vkCreatePipelineLayout(device_, &lightingLayoutInfo, nullptr, &lighting_layout_);
    
    VkGraphicsPipelineCreateInfo lightingPipelineInfo{};
    lightingPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    lightingPipelineInfo.stageCount = 2;
    lightingPipelineInfo.pStages = lightingStages;
    lightingPipelineInfo.pVertexInputState = &lightingVertexInput;
    lightingPipelineInfo.pInputAssemblyState = &lightingInputAssembly;
    lightingPipelineInfo.pViewportState = &viewportState;
    lightingPipelineInfo.pRasterizationState = &lightingRasterizer;
    lightingPipelineInfo.pMultisampleState = &multisampling;
    lightingPipelineInfo.pDepthStencilState = &lightingDepthStencil;
    lightingPipelineInfo.pColorBlendState = &lightingColorBlending;
    lightingPipelineInfo.layout = lighting_layout_;
    lightingPipelineInfo.renderPass = lighting_render_pass_;
    lightingPipelineInfo.subpass = 0;
    
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &lightingPipelineInfo, nullptr, &lighting_pipeline_);
    
    VkPipelineLayoutCreateInfo tonemapPipelineLayoutInfo{};
    tonemapPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    tonemapPipelineLayoutInfo.setLayoutCount = 1;
    tonemapPipelineLayoutInfo.pSetLayouts = &post_process_descriptor_layout_;
    
    vkCreatePipelineLayout(device_, &tonemapPipelineLayoutInfo, nullptr, &post_process_pipeline_layout_);
    
    post_process_pipeline_ = VK_NULL_HANDLE;
    
    // Destroy shader modules
    destroy_shader_module(gbufferVert);
    destroy_shader_module(gbufferFrag);
    destroy_shader_module(lightingVert);
    destroy_shader_module(lightingFrag);
    
    // Other pipelines are stubs
    taa_pipeline_ = VK_NULL_HANDLE;
    ssao_pipeline_ = VK_NULL_HANDLE;
    bloom_pipeline_ = VK_NULL_HANDLE;
    deferred_transparent_pipeline_ = VK_NULL_HANDLE;
}

void DeferredShadingPipeline::create_render_passes() {
    // G-buffer render pass
    std::array<VkAttachmentDescription, 6> attachments = {};
    
    // Albedo/Roughness
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    // Normal
    attachments[1] = attachments[0];
    attachments[1].format = VK_FORMAT_R16G16_SFLOAT;
    
    // Material
    attachments[2] = attachments[0];
    attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    
    // Depth
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    // Velocity
    attachments[4] = attachments[0];
    attachments[4].format = VK_FORMAT_R16G16_SFLOAT;
    
    // Object ID
    attachments[5] = attachments[0];
    attachments[5].format = VK_FORMAT_R32_UINT;
    
    std::array<VkAttachmentReference, 5> colorRefs = {};
    for (uint32_t i = 0; i < 5; ++i) {
        colorRefs[i].attachment = i;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    
    VkAttachmentReference depthRef{};
    depthRef.attachment = 3;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = colorRefs.size();
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    
    vkCreateRenderPass(device_, &renderPassInfo, nullptr, &gbuffer_render_pass_);
    
    // Lighting render pass
    VkAttachmentDescription lightingAttachment{};
    lightingAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    lightingAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    lightingAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    lightingAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    lightingAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    lightingAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    lightingAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    lightingAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkAttachmentReference lightingColorRef{};
    lightingColorRef.attachment = 0;
    lightingColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription lightingSubpass{};
    lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lightingSubpass.colorAttachmentCount = 1;
    lightingSubpass.pColorAttachments = &lightingColorRef;
    
    VkRenderPassCreateInfo lightingRenderPassInfo{};
    lightingRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    lightingRenderPassInfo.attachmentCount = 1;
    lightingRenderPassInfo.pAttachments = &lightingAttachment;
    lightingRenderPassInfo.subpassCount = 1;
    lightingRenderPassInfo.pSubpasses = &lightingSubpass;
    
    vkCreateRenderPass(device_, &lightingRenderPassInfo, nullptr, &lighting_render_pass_);
    post_process_render_pass_ = VK_NULL_HANDLE;
}

void DeferredShadingPipeline::create_post_process_render_pass(VkFormat swapchain_format) {
    if (post_process_render_pass_ != VK_NULL_HANDLE && swapchain_color_format_ == swapchain_format) {
        return;
    }

    if (post_process_render_pass_ != VK_NULL_HANDLE) {
        destroy_render_pass(post_process_render_pass_);
    }

    swapchain_color_format_ = swapchain_format;

    VkAttachmentDescription postProcessAttachment{};
    postProcessAttachment.format = swapchain_color_format_;
    postProcessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    postProcessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    postProcessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    postProcessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    postProcessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    postProcessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    postProcessAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &postProcessAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    vkCreateRenderPass(device_, &renderPassInfo, nullptr, &post_process_render_pass_);
    if (post_process_pipeline_) {
        vkDestroyPipeline(device_, post_process_pipeline_, nullptr);
        post_process_pipeline_ = VK_NULL_HANDLE;
    }
    create_post_process_pipeline();
}

void DeferredShadingPipeline::create_post_process_pipeline() {
    if (post_process_pipeline_ && post_process_render_pass_) {
        return;
    }
    if (!post_process_render_pass_) {
        return;
    }

    VkShaderModule tonemapVert = load_shader_module("core/shaders/deferred/tonemap.vert.spv");
    VkShaderModule tonemapFrag = load_shader_module("core/shaders/deferred/tonemap.frag.spv");
    if (!tonemapVert || !tonemapFrag) {
        return;
    }

    VkPipelineShaderStageCreateInfo tonemapStages[2] = {};
    tonemapStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tonemapStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    tonemapStages[0].module = tonemapVert;
    tonemapStages[0].pName = "main";
    tonemapStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tonemapStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    tonemapStages[1].module = tonemapFrag;
    tonemapStages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(render_width_);
    viewport.height = static_cast<float>(render_height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {render_width_, render_height_};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = tonemapStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = post_process_pipeline_layout_;
    pipelineInfo.renderPass = post_process_render_pass_;
    pipelineInfo.subpass = 0;

    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &post_process_pipeline_);

    destroy_shader_module(tonemapVert);
    destroy_shader_module(tonemapFrag);
}

void DeferredShadingPipeline::create_framebuffers() {
    gbuffer_framebuffers_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    
    std::array<VkImageView, 6> gbufferAttachments = {
        gbuffer_.albedo_roughness_view,
        gbuffer_.normal_view,
        gbuffer_.material_data_view,
        gbuffer_.depth_view,
        gbuffer_.velocity_view,
        gbuffer_.object_id_view
    };
    
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = gbuffer_render_pass_;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
    framebufferInfo.pAttachments = gbufferAttachments.data();
    framebufferInfo.width = render_width_;
    framebufferInfo.height = render_height_;
    framebufferInfo.layers = 1;
    
    for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &gbuffer_framebuffers_[i]);
    }
    
    lighting_framebuffers_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    
    VkImageView lightingAttachment = post_targets_.lighting_result_view;
    
    VkFramebufferCreateInfo lightingFramebufferInfo{};
    lightingFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    lightingFramebufferInfo.renderPass = lighting_render_pass_;
    lightingFramebufferInfo.attachmentCount = 1;
    lightingFramebufferInfo.pAttachments = &lightingAttachment;
    lightingFramebufferInfo.width = render_width_;
    lightingFramebufferInfo.height = render_height_;
    lightingFramebufferInfo.layers = 1;
    
    for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateFramebuffer(device_, &lightingFramebufferInfo, nullptr, &lighting_framebuffers_[i]);
    }
}

void DeferredShadingPipeline::create_descriptor_sets() {
    VkDescriptorSetLayoutBinding gbufferBindings[1] = {};
    gbufferBindings[0].binding = 0;
    gbufferBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    gbufferBindings[0].descriptorCount = 1;
    gbufferBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo gbufferLayoutInfo{};
    gbufferLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    gbufferLayoutInfo.bindingCount = 1;
    gbufferLayoutInfo.pBindings = gbufferBindings;
    vkCreateDescriptorSetLayout(device_, &gbufferLayoutInfo, nullptr, &gbuffer_descriptor_layout_);
    
    VkDescriptorSetLayoutBinding lightingBindings[7] = {};
    for (uint32_t i = 0; i < 4; ++i) {
        lightingBindings[i].binding = i;
        lightingBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lightingBindings[i].descriptorCount = 1;
        lightingBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    lightingBindings[4].binding = 4;
    lightingBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightingBindings[4].descriptorCount = 1;
    lightingBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lightingBindings[5].binding = 5;
    lightingBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingBindings[5].descriptorCount = 1;
    lightingBindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lightingBindings[6].binding = 6;
    lightingBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingBindings[6].descriptorCount = 1;
    lightingBindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo lightingLayoutInfo{};
    lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lightingLayoutInfo.bindingCount = 7;
    lightingLayoutInfo.pBindings = lightingBindings;
    vkCreateDescriptorSetLayout(device_, &lightingLayoutInfo, nullptr, &lighting_descriptor_layout_);
    
    VkDescriptorSetLayoutBinding postProcessBinding{};
    postProcessBinding.binding = 0;
    postProcessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessBinding.descriptorCount = 1;
    postProcessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo postProcessLayoutInfo{};
    postProcessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postProcessLayoutInfo.bindingCount = 1;
    postProcessLayoutInfo.pBindings = &postProcessBinding;
    vkCreateDescriptorSetLayout(device_, &postProcessLayoutInfo, nullptr, &post_process_descriptor_layout_);
    
    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = Constants::MAX_FRAMES_IN_FLIGHT * 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = Constants::MAX_FRAMES_IN_FLIGHT;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = Constants::MAX_FRAMES_IN_FLIGHT * 4 + 1;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = Constants::MAX_FRAMES_IN_FLIGHT * 3 + 1;
    vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptor_pool_);
    
    gbuffer_descriptor_sets_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    lighting_descriptor_sets_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    
    std::vector<VkDescriptorSetLayout> layouts(Constants::MAX_FRAMES_IN_FLIGHT, gbuffer_descriptor_layout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptor_pool_;
    allocInfo.descriptorSetCount = Constants::MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();
    vkAllocateDescriptorSets(device_, &allocInfo, gbuffer_descriptor_sets_.data());
    
    layouts.assign(Constants::MAX_FRAMES_IN_FLIGHT, lighting_descriptor_layout_);
    allocInfo.pSetLayouts = layouts.data();
    vkAllocateDescriptorSets(device_, &allocInfo, lighting_descriptor_sets_.data());
    
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &post_process_descriptor_layout_;
    vkAllocateDescriptorSets(device_, &allocInfo, &post_process_descriptor_set_);
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    vkCreateSampler(device_, &samplerInfo, nullptr, &texture_sampler_);
    
    for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = view_constant_buffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ViewConstants);
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = gbuffer_descriptor_sets_[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        
        VkDescriptorImageInfo imageInfos[4] = {};
        for (uint32_t j = 0; j < 4; ++j) {
            imageInfos[j].sampler = texture_sampler_;
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        imageInfos[0].imageView = gbuffer_.albedo_roughness_view;
        imageInfos[1].imageView = gbuffer_.normal_view;
        imageInfos[2].imageView = gbuffer_.material_data_view;
        imageInfos[3].imageView = gbuffer_.depth_view;
        
        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = light_data_buffer_;
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = Constants::MAX_LIGHTS_FORWARD_PLUS * sizeof(LightData);
        
        VkDescriptorBufferInfo viewConstantsBufferInfo{};
        viewConstantsBufferInfo.buffer = view_constant_buffers_[i];
        viewConstantsBufferInfo.offset = 0;
        viewConstantsBufferInfo.range = sizeof(ViewConstants);
        
        VkDescriptorBufferInfo lightingConstantsBufferInfo{};
        lightingConstantsBufferInfo.buffer = lighting_constant_buffers_[i];
        lightingConstantsBufferInfo.offset = 0;
        lightingConstantsBufferInfo.range = sizeof(LightingConstants);
        
        VkWriteDescriptorSet lightingWrites[7] = {};
        for (uint32_t j = 0; j < 4; ++j) {
            lightingWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            lightingWrites[j].dstSet = lighting_descriptor_sets_[i];
            lightingWrites[j].dstBinding = j;
            lightingWrites[j].descriptorCount = 1;
            lightingWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            lightingWrites[j].pImageInfo = &imageInfos[j];
        }
        lightingWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[4].dstSet = lighting_descriptor_sets_[i];
        lightingWrites[4].dstBinding = 4;
        lightingWrites[4].descriptorCount = 1;
        lightingWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightingWrites[4].pBufferInfo = &lightBufferInfo;
        
        lightingWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[5].dstSet = lighting_descriptor_sets_[i];
        lightingWrites[5].dstBinding = 5;
        lightingWrites[5].descriptorCount = 1;
        lightingWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightingWrites[5].pBufferInfo = &viewConstantsBufferInfo;
        
        lightingWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[6].dstSet = lighting_descriptor_sets_[i];
        lightingWrites[6].dstBinding = 6;
        lightingWrites[6].descriptorCount = 1;
        lightingWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightingWrites[6].pBufferInfo = &lightingConstantsBufferInfo;
        
        vkUpdateDescriptorSets(device_, 7, lightingWrites, 0, nullptr);
    }
    
    VkDescriptorImageInfo postProcessImageInfo{};
    postProcessImageInfo.sampler = texture_sampler_;
    postProcessImageInfo.imageView = post_targets_.lighting_result_view;
    postProcessImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkWriteDescriptorSet postProcessWrite{};
    postProcessWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postProcessWrite.dstSet = post_process_descriptor_set_;
    postProcessWrite.dstBinding = 0;
    postProcessWrite.descriptorCount = 1;
    postProcessWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessWrite.pImageInfo = &postProcessImageInfo;
    vkUpdateDescriptorSets(device_, 1, &postProcessWrite, 0, nullptr);
}

void DeferredShadingPipeline::create_uniform_buffers() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(ViewConstants);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = sizeof(ViewConstants);
    
    view_constant_buffers_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    view_constant_memories_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateBuffer(device_, &bufferInfo, nullptr, &view_constant_buffers_[i]);
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device_, view_constant_buffers_[i], &memReqs);
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &allocInfo, nullptr, &view_constant_memories_[i]);
        vkBindBufferMemory(device_, view_constant_buffers_[i], view_constant_memories_[i], 0);
    }
    
    // Lighting constants (placeholder)
    bufferInfo.size = sizeof(LightingConstants);
    lighting_constant_buffers_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    lighting_constant_memories_.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateBuffer(device_, &bufferInfo, nullptr, &lighting_constant_buffers_[i]);
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device_, lighting_constant_buffers_[i], &memReqs);
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &allocInfo, nullptr, &lighting_constant_memories_[i]);
        vkBindBufferMemory(device_, lighting_constant_buffers_[i], lighting_constant_memories_[i], 0);
    }
}

void DeferredShadingPipeline::create_post_process_targets() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = render_width_;
    imageInfo.extent.height = render_height_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    vkCreateImage(device_, &imageInfo, nullptr, &post_targets_.lighting_result);
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, post_targets_.lighting_result, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &post_targets_.lighting_result_memory);
    vkBindImageMemory(device_, post_targets_.lighting_result, post_targets_.lighting_result_memory, 0);
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = post_targets_.lighting_result;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device_, &viewInfo, nullptr, &post_targets_.lighting_result_view);
    
    post_targets_.bloom_hdr = VK_NULL_HANDLE;
    post_targets_.bloom_hdr_view = VK_NULL_HANDLE;
    post_targets_.bloom_hdr_memory = VK_NULL_HANDLE;
}
    void DeferredShadingPipeline::perform_gbuffer_pass(VkCommandBuffer cmd_buffer) {
    if (!gbuffer_render_pass_ || !gbuffer_pipeline_ || gbuffer_framebuffers_.empty()) {
        return;
    }

    std::array<VkClearValue, 6> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[3].depthStencil = {1.0f, 0};
    clearValues[4].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[5].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = gbuffer_render_pass_;
    renderPassInfo.framebuffer = gbuffer_framebuffers_[current_frame_index_];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {render_width_, render_height_};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer_pipeline_);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer_layout_, 0, 1, &gbuffer_descriptor_sets_[current_frame_index_], 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(render_width_);
    viewport.height = static_cast<float>(render_height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {render_width_, render_height_}};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    for (const auto& obj : opaque_queue_) {
        // Get mesh from asset manager
        auto mesh = asset_manager_->get_mesh(obj.mesh_id);
        if (!mesh) {
            continue; // Skip if mesh not available
        }

        // Get material from asset manager
        auto material = asset_manager_->get_material(obj.material_id);
        if (!material) {
            continue; // Skip if material not available
        }

        struct PushConstants {
            glm::mat4 model;
            glm::vec4 albedo;
            glm::vec4 material_params; // roughness, metallic, ao, emissive
        } pushConstants;

        pushConstants.model = obj.transform;
        pushConstants.albedo = material->albedo_color;
        pushConstants.material_params = glm::vec4(
            material->roughness,
            material->metallic,
            material->ambient_occlusion,
            0.0f // emissive strength
        );

        vkCmdPushConstants(cmd_buffer, gbuffer_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

        // Bind vertex and index buffers
        VkBuffer vertexBuffers[] = {mesh->vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd_buffer, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw the mesh
        vkCmdDrawIndexed(cmd_buffer, mesh->index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd_buffer);
}

void DeferredShadingPipeline::perform_lighting_pass(VkCommandBuffer cmd_buffer) {
    if (!lighting_render_pass_ || !lighting_pipeline_ || lighting_framebuffers_.empty()) {
        return;
    }

    std::array<VkClearValue, 1> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = lighting_render_pass_;
    renderPassInfo.framebuffer = lighting_framebuffers_[current_frame_index_];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {render_width_, render_height_};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_pipeline_);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_layout_, 0, 1, &lighting_descriptor_sets_[current_frame_index_], 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(render_width_);
    viewport.height = static_cast<float>(render_height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {render_width_, render_height_}};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buffer);
}

void DeferredShadingPipeline::perform_post_processing(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    // Deferred pipeline does not currently implement a separate post-processing pass.
}

void DeferredShadingPipeline::perform_transparent_pass(VkCommandBuffer cmd_buffer) {
    // Transparent rendering is not implemented in this version of the deferred pipeline.
}

void DeferredShadingPipeline::perform_taa(VkCommandBuffer cmd_buffer) {
    // Temporal anti-aliasing is not implemented in this version of the deferred pipeline.
}

void DeferredShadingPipeline::perform_bloom(VkCommandBuffer cmd_buffer) {
    // Bloom is not implemented in this version of the deferred pipeline.
}

void DeferredShadingPipeline::perform_ssao(VkCommandBuffer cmd_buffer) {
    // SSAO is not implemented in this version of the deferred pipeline.
}

void DeferredShadingPipeline::perform_tonemapping(VkCommandBuffer cmd_buffer, const RenderTargets& target) {
    if (post_targets_.lighting_result == VK_NULL_HANDLE || target.framebuffer == VK_NULL_HANDLE) {
        return;
    }

    create_post_process_render_pass(target.color_format);
    if (!post_process_render_pass_ || !post_process_pipeline_ || post_process_descriptor_set_ == VK_NULL_HANDLE) {
        return;
    }

    std::array<VkClearValue, 1> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = post_process_render_pass_;
    renderPassInfo.framebuffer = target.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {target.width, target.height};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline_);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline_layout_, 0, 1, &post_process_descriptor_set_, 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(target.width);
    viewport.height = static_cast<float>(target.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {target.width, target.height}};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buffer);
}

void DeferredShadingPipeline::destroy_buffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory) {
        vkFreeMemory(device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void DeferredShadingPipeline::destroy_image(VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
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

void DeferredShadingPipeline::destroy_pipeline(VkPipeline& pipeline) {
    if (pipeline) {
        vkDestroyPipeline(device_, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}

void DeferredShadingPipeline::destroy_render_pass(VkRenderPass& render_pass) {
    if (render_pass) {
        vkDestroyRenderPass(device_, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }
}

void DeferredShadingPipeline::destroy_framebuffers(std::vector<VkFramebuffer>& framebuffers) {
    for (auto& fb : framebuffers) {
        if (fb) {
            vkDestroyFramebuffer(device_, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    framebuffers.clear();
}

void DeferredShadingPipeline::destroy_shader_module(VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }
}

VkShaderModule DeferredShadingPipeline::load_shader_module(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Failed to open shader module: " << filename
                  << " (cwd: " << std::filesystem::current_path() << ")\n";
        return VK_NULL_HANDLE;
    }
    
    size_t fileSize = (size_t)file.tellg();
    if (fileSize == 0) {
        std::cerr << "[ERROR] Shader module file is empty: " << filename << "\n";
        return VK_NULL_HANDLE;
    }
    
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "[ERROR] Failed to create shader module: " << filename << "\n";
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

uint32_t DeferredShadingPipeline::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace rendering_engine
