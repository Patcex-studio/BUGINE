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
#include "rendering_engine/rendering_engine.h"
#include <chrono>
#include <stdexcept>
#include <cstring>
#include <iostream>

#ifdef RENDERING_ENGINE_WITH_GLFW
#include <GLFW/glfw3.h>
#endif

namespace rendering_engine {

RenderingEngine::~RenderingEngine() {
    if (is_initialized_) {
        shutdown();
    }
}

void RenderingEngine::setup_vulkan(uint32_t window_width, uint32_t window_height, bool enable_validation) {
    init_vulkan_device(window_width, window_height, enable_validation);
    init_command_pools();
    init_swapchain(window_width, window_height);
}

void RenderingEngine::initialize_resources() {
    // Create pipeline manager
    pipeline_manager_ = std::make_unique<RenderPipelineManager>();
    
    // Initialize with current device and queues
    pipeline_manager_->initialize(
        device_,
        physical_device_,
        graphics_queue_,
        command_pool_,
        render_width_,
        render_height_
    );
    
    // Initialize model system
    model_system_ = std::make_unique<ModelSystem>();
    model_system_->initialize(this, nullptr, nullptr); // TODO: Pass actual physics and assembly systems
    
    is_initialized_ = true;
}

void RenderingEngine::shutdown() {
    if (!is_initialized_) return;
    
    vkDeviceWaitIdle(device_);
    
    cleanup_resources();
    cleanup_vulkan();
    
    is_initialized_ = false;
}

void RenderingEngine::set_rendering_pipeline(RenderPipelineType pipeline_type) {
    if (pipeline_manager_) {
        pipeline_manager_->set_active_pipeline(pipeline_type);
    }
}

RenderPipelineType RenderingEngine::get_active_pipeline() const {
    if (pipeline_manager_) {
        return pipeline_manager_->get_active_pipeline_type();
    }
    return RenderPipelineType::FORWARD_PLUS;
}

std::vector<RenderPipelineType> RenderingEngine::get_available_pipelines() const {
    if (pipeline_manager_) {
        return pipeline_manager_->get_available_pipelines();
    }
    return {};
}

void RenderingEngine::begin_frame(const Camera& camera, const std::vector<LightData>& lights) {
    if (!pipeline_manager_) return;
    
    current_frame_index_ = frame_count_++;
    current_camera_ = camera;  // Store camera for later use
    
    pipeline_manager_->begin_frame(current_frame_index_);
    pipeline_manager_->update_camera(camera);
    pipeline_manager_->update_lights(lights);
}

void RenderingEngine::queue_renderables(const std::vector<RenderableObject>& objects) {
    if (pipeline_manager_) {
        pipeline_manager_->queue_renderables(objects);
    }
}

void RenderingEngine::render(const RenderTargets& target) {
    if (!pipeline_manager_ || !model_system_) return;
    
    // Allocate command buffer for this frame
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer cmd_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &cmd_buffer);
    
    // Begin command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd_buffer, &begin_info);
    
    // Prepare model system rendering (queues renderables)
    model_system_->prepare_instanced_rendering(cmd_buffer, current_camera_);
    
    // Render with the active pipeline
    pipeline_manager_->render(cmd_buffer, target);

    for (auto& callback : ui_render_callbacks_) {
        if (callback) {
            callback(cmd_buffer);
        }
    }
    
    // End and submit command buffer
    vkEndCommandBuffer(cmd_buffer);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    
    vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_); // For simplicity, wait for completion
    
    // Free command buffer
    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buffer);
}

void RenderingEngine::AddUIRenderCallback(std::function<void(VkCommandBuffer)> callback) {
    if (callback) {
        ui_render_callbacks_.push_back(std::move(callback));
    }
}

void RenderingEngine::end_frame() {
    if (pipeline_manager_) {
        pipeline_manager_->end_frame();
    }
}

void RenderingEngine::sync_physics_to_render(const PhysicsCore& physics) {
    // Would synchronize transforms, add destruction effects, etc.
}

void RenderingEngine::render_physics_debug(const PhysicsCore& physics, VkCommandBuffer cmd_buffer) {
    // Would render collision shapes, constraints, etc.
}

void RenderingEngine::render_terrain(const TerrainSystem& terrain, VkCommandBuffer cmd_buffer) {
    // Would render terrain with LOD and streaming
}

void RenderingEngine::render_terrain_destruction(const TerrainSystem& terrain) {
    // Would render crater effects and deformation
}

void RenderingEngine::render_particles(const ParticleSystem& particles, VkCommandBuffer cmd_buffer) {
    // Would render particle system
}

void RenderingEngine::apply_thermal_overlay(const void* thermal_data, VkCommandBuffer cmd_buffer) {
    // Would apply thermal vision overlay
}

void RenderingEngine::render_damage_effects(const void* damage_data, VkCommandBuffer cmd_buffer) {
    // Would render smoke, fire, sparks
}

void RenderingEngine::render_weather_effects(const void* weather_data, VkCommandBuffer cmd_buffer) {
    // Would render rain, snow, fog
}

uint32_t RenderingEngine::load_mesh(const std::string& filename) {
    // Load mesh from file
    uint32_t mesh_id = resource_manager_.next_mesh_id++;
    // resource_manager_.meshes[mesh_id] = loaded_mesh;
    return mesh_id;
}

uint32_t RenderingEngine::load_texture(const std::string& filename) {
    // Load texture from file
    uint32_t texture_id = resource_manager_.next_texture_id++;
    // resource_manager_.textures[texture_id] = loaded_texture;
    return texture_id;
}

uint32_t RenderingEngine::load_material(const std::string& filename) {
    // Load material from file
    uint32_t material_id = resource_manager_.next_material_id++;
    // resource_manager_.materials[material_id] = loaded_material;
    return material_id;
}

void RenderingEngine::unload_mesh(uint32_t mesh_id) {
    resource_manager_.meshes.erase(mesh_id);
}

void RenderingEngine::unload_texture(uint32_t texture_id) {
    resource_manager_.textures.erase(texture_id);
}

RenderingEngine::PerformanceMetrics RenderingEngine::get_performance_metrics() const {
    return current_metrics_;
}

void RenderingEngine::set_performance_monitoring(bool enabled) {
    monitor_performance_ = enabled;
    if (pipeline_manager_) {
        pipeline_manager_->set_performance_monitoring(enabled);
    }
}

void RenderingEngine::print_performance_report() {
    // Print performance metrics to console/log
}

void RenderingEngine::set_debug_visualization(int32_t debug_target) {
    // Enable debug visualization
}

void RenderingEngine::begin_gpu_capture() {
    // Begin GPU capture for profiling
}

void RenderingEngine::end_gpu_capture() {
    // End GPU capture
}

void RenderingEngine::on_resize(uint32_t new_width, uint32_t new_height) {
    render_width_ = new_width;
    render_height_ = new_height;
    
    if (pipeline_manager_) {
        pipeline_manager_->resize(new_width, new_height);
    }
    
    // Rebuild framebuffers, etc.
}

RenderPipelineManager::GPUCapabilities RenderingEngine::get_gpu_capabilities() const {
    if (pipeline_manager_) {
        return pipeline_manager_->query_gpu_capabilities();
    }
    return {};
}

bool RenderingEngine::supports_ray_tracing() const {
    return get_gpu_capabilities().supports_ray_tracing;
}

bool RenderingEngine::supports_mesh_shaders() const {
    return get_gpu_capabilities().supports_mesh_shaders;
}

bool RenderingEngine::supports_bindless_rendering() const {
    return get_gpu_capabilities().supports_bindless;
}

// Private implementation methods

bool RenderingEngine::check_validation_layer_support(const char* layer_name) {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    
    for (const auto& layer : available_layers) {
        if (strcmp(layer_name, layer.layerName) == 0) {
            return true;
        }
    }
    return false;
}

VkSurfaceFormatKHR RenderingEngine::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available) {
    // Try to find SRGB 32-bit format with SRGB color space
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && 
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }
    
    // If not found, try general 32-bit format
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM && 
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }
    
    // Fallback to first available format
    return available[0];
}

VkPresentModeKHR RenderingEngine::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available) {
    // Try MAILBOX for low latency without tearing
    for (auto mode : available) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    
    // Fall back to FIFO (always available)
    return VK_PRESENT_MODE_FIFO_KHR;
}

void RenderingEngine::init_vulkan_device(uint32_t window_width, uint32_t window_height, bool enable_validation) {
    enable_validation_ = enable_validation;
    render_width_ = window_width;
    render_height_ = window_height;
    
#ifdef RENDERING_ENGINE_WITH_GLFW
    // Initialize GLFW
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    // GLFW window creation hint - no client API since we'll use Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    // Create window
    window_ = glfwCreateWindow(static_cast<int>(window_width), static_cast<int>(window_height), 
                               "The Honest World - Vulkan Rendering", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
#else
    window_ = nullptr;
    std::cout << "[WARN] GLFW not available - running without window\n";
#endif
    
    // ========== CREATE VULKAN INSTANCE ==========
    std::vector<const char*> enabled_layers;
    if (enable_validation) {
        if (check_validation_layer_support("VK_LAYER_KHRONOS_validation")) {
            enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
            std::cout << "[INFO] Validation layer enabled\n";
        } else {
            std::cerr << "[WARN] Validation layer requested but not available\n";
        }
    }
    
    // Get GLFW required extensions (or use minimal set if GLFW not available)
    std::vector<const char*> instance_extensions;
    
#ifdef RENDERING_ENGINE_WITH_GLFW
    uint32_t glfw_ext_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    instance_extensions.assign(glfw_extensions, glfw_extensions + glfw_ext_count);
#else
    // Minimal extensions without GLFW
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#endif
    
    // Add additional extensions if needed (debugging, etc.)
    if (enable_validation) {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "The Honest World";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
    instance_info.ppEnabledLayerNames = enabled_layers.data();
    instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    instance_info.ppEnabledExtensionNames = instance_extensions.data();
    
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
#endif
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    
    // ========== CREATE SURFACE ==========
#ifdef RENDERING_ENGINE_WITH_GLFW
    result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance_, nullptr);
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to create Vulkan surface");
    }
#else
    // When GLFW is not available, create a headless surface if supported
    // For now, we skip surface creation - can be extended later
    surface_ = VK_NULL_HANDLE;
    
    // Try to use VK_EXT_headless_surface if available
    VkHeadlessSurfaceCreateInfoEXT headless_info{};
    headless_info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    
    auto vkCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)
        vkGetInstanceProcAddr(instance_, "vkCreateHeadlessSurfaceEXT");
    
    if (vkCreateHeadlessSurfaceEXT) {
        vkCreateHeadlessSurfaceEXT(instance_, &headless_info, nullptr, &surface_);
    }
    
    if (surface_ == VK_NULL_HANDLE) {
        std::cout << "[WARN] Could not create surface - running headless\n";
    }
#endif
    
    // ========== SELECT PHYSICAL DEVICE ==========
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    
    if (device_count == 0) {
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
#endif
        vkDestroyInstance(instance_, nullptr);
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
#endif
        throw std::runtime_error("No Vulkan-capable GPU found");
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());
    
    // Find suitable physical device (prefer discrete GPU, but accept any with graphics support)
    physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDevice discrete_device = VK_NULL_HANDLE;
    
    for (auto device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        
        // Check queue family support
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
        
        // Find queue family supporting both graphics and presentation
        uint32_t graphics_family = UINT32_MAX;
        uint32_t present_family = UINT32_MAX;
        
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family = i;
            }
            
            // Check presentation support only if surface exists
            if (surface_ != VK_NULL_HANDLE) {
                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present_support);
                if (present_support) {
                    present_family = i;
                }
            } else {
                present_family = i; // When headless, any queue can present
            }
            
            if (graphics_family != UINT32_MAX && present_family != UINT32_MAX) {
                break;
            }
        }
        
        if (graphics_family == UINT32_MAX) {
            continue; // This device doesn't support graphics
        }
        
        if (surface_ != VK_NULL_HANDLE && present_family == UINT32_MAX) {
            continue; // Need presentation support if surface exists
        }
        
        // Prefer discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            discrete_device = device;
            break;
        }
        
        // Use first suitable if no discrete available yet
        if (physical_device_ == VK_NULL_HANDLE) {
            physical_device_ = device;
        }
    }
    
    // Use discrete device if found
    if (discrete_device != VK_NULL_HANDLE) {
        physical_device_ = discrete_device;
    }
    
    if (physical_device_ == VK_NULL_HANDLE) {
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
#endif
        vkDestroyInstance(instance_, nullptr);
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
#endif
        throw std::runtime_error("No suitable physical device found");
    }
    
    // ========== CREATE LOGICAL DEVICE ==========
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());
    
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;
    
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
        }
        
        // Only check presentation if surface exists
        if (surface_ != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, surface_, &present_support);
            if (present_support) {
                present_family = i;
            }
        } else {
            present_family = i; // Headless - any queue works
        }
        
        if (graphics_family != UINT32_MAX && present_family != UINT32_MAX) {
            break;
        }
    }
    
    // Find a queue family that supports both graphics and presentation
    uint32_t queue_family_to_use = UINT32_MAX;
    if (graphics_family == present_family) {
        queue_family_to_use = graphics_family;
    } else if (graphics_family != UINT32_MAX) {
        queue_family_to_use = graphics_family; // Use graphics if presentation is not critical
    } else if (present_family != UINT32_MAX) {
        queue_family_to_use = present_family;
    }
    
    // For surfaced mode, look for single queue supporting both
    if (surface_ != VK_NULL_HANDLE && queue_family_to_use == UINT32_MAX) {
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, surface_, &present_support);
            
            if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
                queue_family_to_use = i;
                break;
            }
        }
    }
    
    if (queue_family_to_use == UINT32_MAX) {
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
#endif
        vkDestroyInstance(instance_, nullptr);
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
#endif
        throw std::runtime_error("No suitable queue family found");
    }
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_to_use;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;
    
    // Build device extensions list
    std::vector<const char*> device_extensions;
    if (surface_ != VK_NULL_HANDLE) {
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    
    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.pEnabledFeatures = &device_features;
    device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
    device_info.ppEnabledLayerNames = enabled_layers.data();
    
    result = vkCreateDevice(physical_device_, &device_info, nullptr, &device_);
    if (result != VK_SUCCESS) {
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
#endif
        vkDestroyInstance(instance_, nullptr);
#ifdef RENDERING_ENGINE_WITH_GLFW
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
#endif
        throw std::runtime_error("Failed to create logical device");
    }
    
    // Get graphics queue
    vkGetDeviceQueue(device_, queue_family_to_use, 0, &graphics_queue_);
    
    std::cout << "[INFO] Vulkan device initialized successfully\n";
}

void RenderingEngine::init_command_pools() {
    if (!device_ || graphics_queue_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Device or queue not initialized before creating command pool");
    }
    
    // Find graphics queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());
    
    uint32_t graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
            break;
        }
    }
    
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = graphics_family;
    
    VkResult result = vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    
    std::cout << "[INFO] Command pool created\n";
}

void RenderingEngine::create_final_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_image_format_;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    VkResult result = vkCreateRenderPass(device_, &render_pass_info, nullptr, &final_render_pass_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
    
    std::cout << "[INFO] Final render pass created\n";
}

void RenderingEngine::create_swapchain_render_targets() {
    swapchain_targets_.resize(swapchain_images_.size());
    swapchain_framebuffers_.resize(swapchain_images_.size());
    
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        // Create framebuffer
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = final_render_pass_;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &swapchain_image_views_[i];
        framebuffer_info.width = render_width_;
        framebuffer_info.height = render_height_;
        framebuffer_info.layers = 1;
        
        VkResult result = vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &swapchain_framebuffers_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
        
        // Fill RenderTargets structure
        RenderTargets& target = swapchain_targets_[i];
        target.color_image = swapchain_images_[i];
        target.color_view = swapchain_image_views_[i];
        target.framebuffer = swapchain_framebuffers_[i];
        target.width = render_width_;
        target.height = render_height_;
        target.color_format = swapchain_image_format_;
        target.depth_format = VK_FORMAT_UNDEFINED;
        target.depth_image = VK_NULL_HANDLE;
        target.depth_view = VK_NULL_HANDLE;
    }
    
    std::cout << "[INFO] Swapchain render targets created (" << swapchain_targets_.size() << " targets)\n";
}

void RenderingEngine::init_swapchain(uint32_t width, uint32_t height) {
    if (!device_) {
        throw std::runtime_error("Device not initialized");
    }
    
    if (surface_ == VK_NULL_HANDLE) {
        std::cout << "[WARN] No surface available - skipping swapchain initialization (headless mode)\n";
        return;
    }
    
    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);
    
    // Get surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());
    
    if (formats.empty()) {
        throw std::runtime_error("No surface formats available");
    }
    
    // Get present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, present_modes.data());
    
    // Choose format and present mode
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(present_modes);
    
    swapchain_image_format_ = surface_format.format;
    
    // Set swapchain extent
    VkExtent2D extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        extent.width = std::max(1u, std::min(width, capabilities.maxImageExtent.width));
        extent.height = std::max(1u, std::min(height, capabilities.maxImageExtent.height));
    }
    
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface_;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;
    
    VkResult result = vkCreateSwapchainKHR(device_, &swapchain_info, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }
    
    render_width_ = extent.width;
    render_height_ = extent.height;
    
    // Get swapchain images
    uint32_t actual_image_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_image_count, nullptr);
    swapchain_images_.resize(actual_image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_image_count, swapchain_images_.data());
    
    // Create image views
    swapchain_image_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_image_format_;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        result = vkCreateImageView(device_, &view_info, nullptr, &swapchain_image_views_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }
    
    // Create render pass and framebuffers
    create_final_render_pass();
    create_swapchain_render_targets();
    
    // Create synchronization primitives
    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't wait
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore");
        }
        
        result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore");
        }
        
        result = vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence");
        }
    }
    
    std::cout << "[INFO] Swapchain initialized with " << swapchain_images_.size() << " images\n";
}

void RenderingEngine::recreate_swapchain() {
    if (surface_ == VK_NULL_HANDLE) {
        std::cout << "[WARN] Attempted to recreate swapchain in headless mode\n";
        return;
    }
    
    vkDeviceWaitIdle(device_);
    
    // Cleanup old resources
    for (auto& fb : swapchain_framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    swapchain_framebuffers_.clear();
    swapchain_targets_.clear();
    
    vkDestroyRenderPass(device_, final_render_pass_, nullptr);
    final_render_pass_ = VK_NULL_HANDLE;
    
    for (auto& view : swapchain_image_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_image_views_.clear();
    
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    
#ifdef RENDERING_ENGINE_WITH_GLFW
    // Wait for window to be valid size
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }
    
    // Reinitialize
    init_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
#else
    // Reinitialize with cached dimensions
    init_swapchain(render_width_, render_height_);
#endif
}

void RenderingEngine::cleanup_vulkan() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    
    vkDeviceWaitIdle(device_);
    
    // Destroy synchronization primitives
    for (auto& sem : image_available_semaphores_) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, sem, nullptr);
        }
    }
    image_available_semaphores_.clear();
    
    for (auto& sem : render_finished_semaphores_) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, sem, nullptr);
        }
    }
    render_finished_semaphores_.clear();
    
    for (auto& fence : in_flight_fences_) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device_, fence, nullptr);
        }
    }
    in_flight_fences_.clear();
    
    // Destroy swapchain resources (only if we have a surface)
    if (surface_ != VK_NULL_HANDLE) {
        for (auto& fb : swapchain_framebuffers_) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device_, fb, nullptr);
            }
        }
        swapchain_framebuffers_.clear();
        swapchain_targets_.clear();
        
        if (final_render_pass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, final_render_pass_, nullptr);
            final_render_pass_ = VK_NULL_HANDLE;
        }
        
        for (auto& view : swapchain_image_views_) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view, nullptr);
            }
        }
        swapchain_image_views_.clear();
        swapchain_images_.clear();
        
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
        
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    
    // Destroy command pool
    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
    
    // Destroy device
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    
    // Destroy instance
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    
    // Cleanup GLFW
#ifdef RENDERING_ENGINE_WITH_GLFW
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
#endif
    
    std::cout << "[INFO] Vulkan cleanup completed\n";
}

void RenderingEngine::cleanup_resources() {
    if (pipeline_manager_) {
        pipeline_manager_->shutdown();
        pipeline_manager_.reset();
    }
    
    if (model_system_) {
        model_system_.reset();
    }
}

} // namespace rendering_engine
