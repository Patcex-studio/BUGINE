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

#include "render_pipeline_manager.h"
#include "types.h"
#include "model_system.h"
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <memory>
#include <string>

// Forward declare GLFW window
struct GLFWwindow;

namespace rendering_engine {

// Forward declarations for integration
class PhysicsCore;
class TerrainSystem;
class ParticleSystem;
class ModelSystem;

/**
 * @brief Main Rendering Engine Interface
 * 
 * High-level API for the complete rendering system. Handles:
 * - Vulkan initialization and device management
 * - Pipeline management and switching
 * - Geometry and texture loading
 * - Scene rendering with physics/terrain integration
 * - Performance monitoring
 * 
 * The rendering engine uses a plugin architecture for different rendering
 * techniques, allowing selection of best strategy for each scene:
 * 
 * - Forward+: CPU: 1920x1080, 1000+ lights
 * - Deferred: Complex materials, visual effects
 * - Clustered: 10K+ lights, complex indoor scenes
 * 
 * Example Usage:
 * ```cpp
 * RenderingEngine engine;
 * engine.setup_vulkan();
 * engine.initialize_resources();
 * 
 * while (running) {
 *     engine.sync_physics_to_render(physics_system);
 *     engine.sync_terrain_to_render(terrain_system);
 *     
 *     engine.begin_frame(camera, lights);
 *     engine.queue_renderables(scene_objects);
 *     engine.render(render_target);
 * }
 * ```
 */
class RenderingEngine {
public:
    RenderingEngine() = default;
    ~RenderingEngine();
    
    // ========== INITIALIZATION ==========
    
    /**
     * Initialize Vulkan context and device
     * @param window_width Rendering window width
     * @param window_height Rendering window height
     * @param enable_validation Enable validation layers
     */
    void setup_vulkan(uint32_t window_width, uint32_t window_height, bool enable_validation = false);
    
    /**
     * Initialize rendering resources (pipelines, buffers, etc.)
     */
    void initialize_resources();
    
    /**
     * Shutdown rendering engine and release resources
     */
    void shutdown();
    
    // ========== PIPELINE MANAGEMENT ==========
    
    /**
     * Switch active rendering pipeline
     * @param pipeline_type Type of pipeline to use
     */
    void set_rendering_pipeline(RenderPipelineType pipeline_type);
    
    /**
     * Get currently active pipeline type
     */
    RenderPipelineType get_active_pipeline() const;
    
    /**
     * Get list of available pipelines for this GPU
     */
    std::vector<RenderPipelineType> get_available_pipelines() const;
    
    // ========== FRAME MANAGEMENT ==========
    
    /**
     * Begin a new frame
     * @param camera Active camera
     * @param lights Array of lights in scene
     */
    void begin_frame(const Camera& camera, const std::vector<LightData>& lights);
    
    /**
     * Queue renderable objects for this frame
     * @param objects Array of objects to render
     */
    void queue_renderables(const std::vector<RenderableObject>& objects);
    
    /**
     * Render the current frame
     * @param target Output render target
     */
    void render(const RenderTargets& target);
    
    /**
     * End the current frame
     */
    void end_frame();

    /**
     * Register a UI render callback that will be executed in the command buffer after scene rendering.
     */
    void AddUIRenderCallback(std::function<void(VkCommandBuffer)> callback);
    
    // ========== PHYSICS INTEGRATION ==========
    
    /**
     * Synchronize physics world state to rendering
     * Updates transform matrices, adds destruction effects, etc.
     * @param physics Physics engine instance
     * @param scene Output modified scene
     */
    void sync_physics_to_render(const PhysicsCore& physics);
    
    /**
     * Render visual debugging for physics (collisions, constraints, etc.)
     * @param physics Physics engine instance
     * @param cmd_buffer Command buffer for debug rendering
     */
    void render_physics_debug(const PhysicsCore& physics, VkCommandBuffer cmd_buffer);
    
    // ========== TERRAIN INTEGRATION ==========
    
    /**
     * Render terrain system with proper LOD and streaming
     * @param terrain Terrain system instance
     * @param cmd_buffer Command buffer for rendering
     */
    void render_terrain(const TerrainSystem& terrain, VkCommandBuffer cmd_buffer);
    
    /**
     * Render destruction effects on terrain
     * @param terrain Terrain system with damage data
     */
    void render_terrain_destruction(const TerrainSystem& terrain);
    
    // ========== PARTICLE SYSTEMS ==========
    
    /**
     * Render particle system
     * @param particles Particle system instance
     * @param cmd_buffer Command buffer for rendering
     */
    void render_particles(const ParticleSystem& particles, VkCommandBuffer cmd_buffer);
    
    // ========== SPECIAL EFFECTS ==========
    
    /**
     * Apply thermal/NVG vision overlay
     * @param thermal_data Thermal signature data
     * @param cmd_buffer Command buffer for overlay
     */
    void apply_thermal_overlay(const void* thermal_data, VkCommandBuffer cmd_buffer);
    
    /**
     * Render damage effects (smoke, fire, sparks)
     * @param damage_data Damage state information
     * @param cmd_buffer Command buffer for effects
     */
    void render_damage_effects(const void* damage_data, VkCommandBuffer cmd_buffer);
    
    /**
     * Render weather and atmospheric effects
     * @param weather_data Weather state
     * @param cmd_buffer Command buffer for effects
     */
    void render_weather_effects(const void* weather_data, VkCommandBuffer cmd_buffer);
    
    // ========== RESOURCE MANAGEMENT ==========
    
    /**
     * Load mesh from file
     * @param filename Path to mesh file
     * @return Mesh ID for later referencing
     */
    uint32_t load_mesh(const std::string& filename);
    
    /**
     * Load texture from file
     * @param filename Path to texture file
     * @return Texture ID
     */
    uint32_t load_texture(const std::string& filename);
    
    /**
     * Load material parameters
     * @param filename Path to material file
     * @return Material ID
     */
    uint32_t load_material(const std::string& filename);
    
    /**
     * Unload mesh
     * @param mesh_id Mesh to unload
     */
    void unload_mesh(uint32_t mesh_id);
    
    /**
     * Unload texture
     * @param texture_id Texture to unload
     */
    void unload_texture(uint32_t texture_id);
    
    // ========== PERFORMANCE MONITORING ==========
    
    /**
     * Get performance metrics
     */
    struct PerformanceMetrics {
        float fps;
        float frame_time_ms;
        float gpu_time_ms;
        float cpu_time_ms;
        uint32_t draw_calls;
        uint32_t triangle_count;
        uint64_t memory_usage;
    };
    
    PerformanceMetrics get_performance_metrics() const;
    
    /**
     * Enable/disable performance monitoring
     */
    void set_performance_monitoring(bool enabled);
    
    /**
     * Log performance report
     */
    void print_performance_report();
    
    // ========== DEBUGGING ==========
    
    /**
     * Enable/disable debug visualization
     * @param debug_target Debug visualization target
     */
    void set_debug_visualization(int32_t debug_target);
    
    /**
     * Enable GPU capture for profiling (requires debugging tools)
     */
    void begin_gpu_capture();
    void end_gpu_capture();
    
    // ========== RESIZE HANDLING ==========
    
    /**
     * Handle viewport/swapchain resize
     * @param new_width New width
     * @param new_height New height
     */
    void on_resize(uint32_t new_width, uint32_t new_height);
    
    // ========== DEVICE CAPABILITIES ==========
    
    /**
     * Get GPU capability information
     */
    RenderPipelineManager::GPUCapabilities get_gpu_capabilities() const;
    
    /**
     * Check if GPU supports specific feature
     */
    bool supports_ray_tracing() const;
    bool supports_mesh_shaders() const;
    bool supports_bindless_rendering() const;

private:
    // GLFW Window
    GLFWwindow* window_ = nullptr;
    
    // Vulkan resources
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_image_format_ = VK_FORMAT_UNDEFINED;
    VkRenderPass final_render_pass_ = VK_NULL_HANDLE;
    
    // Swapchain images and views
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    std::vector<VkFramebuffer> swapchain_framebuffers_;
    std::vector<RenderTargets> swapchain_targets_;
    
    // Synchronization primitives
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    uint32_t current_frame_ = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    
    uint32_t render_width_ = 0;
    uint32_t render_height_ = 0;
    bool is_initialized_ = false;
    bool enable_validation_ = false;
    
    // Pipeline management
    std::unique_ptr<RenderPipelineManager> pipeline_manager_;
    std::vector<std::function<void(VkCommandBuffer)>> ui_render_callbacks_;
    
    // Frame state
    uint32_t current_frame_index_ = 0;
    uint32_t frame_count_ = 0;
    Camera current_camera_;  // Store current camera for rendering
    
    // Resource management
    struct ResourceManager {
        std::unordered_map<uint32_t, Mesh> meshes;
        std::unordered_map<uint32_t, VkImage> textures;
        std::unordered_map<uint32_t, MaterialProperties> materials;
        uint32_t next_mesh_id = 1;
        uint32_t next_texture_id = 1;
        uint32_t next_material_id = 1;
    } resource_manager_;
    
    // Model system
    std::unique_ptr<ModelSystem> model_system_;
    
    // Performance tracking
    bool monitor_performance_ = false;
    PerformanceMetrics current_metrics_;
    
    // Initialize Vulkan device and queues
    void init_vulkan_device(uint32_t window_width, uint32_t window_height, bool enable_validation);
    void init_command_pools();
    void init_swapchain(uint32_t width, uint32_t height);
    void create_final_render_pass();
    void create_swapchain_render_targets();
    void recreate_swapchain();
    
    // Helper functions
    bool check_validation_layer_support(const char* layer_name);
    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available);
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available);
    
    // Cleanup helpers
    void cleanup_vulkan();
    void cleanup_resources();
};

} // namespace rendering_engine
