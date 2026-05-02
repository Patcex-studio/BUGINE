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

#include "types.h"
#include "resource_management.h"
#include <immintrin.h>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <glm/glm.hpp>

// Include for BoundingBox
#include "../../modules/model_generation_system/include/blueprint_engine.h"

// Include for physics types
#include "physics_core/types.h"
#include "physics_core/physics_core.h"

namespace rendering_engine {

using physics_core::EntityID;
using model_generation::BoundingBox;

// Forward declarations
class RenderingEngine;
class AssemblySystem;

// ============================================================================
// MODEL SYSTEM CONSTANTS
// ============================================================================

// Render flags for ModelInstance
const uint32_t RENDER_FLAG_SKIP = 1 << 0;        // Skip rendering this instance
const uint32_t RENDER_FLAG_SHADOWS = 1 << 1;     // Cast shadows
const uint32_t RENDER_FLAG_REFLECTIONS = 1 << 2; // Appear in reflections

// ============================================================================
// MODEL SYSTEM TYPES AND STRUCTURES
// ============================================================================

/**
 * @brief SIMD transform matrix (4x4 float matrix in SIMD registers)
 */
union SIMDTransform {
    __m256 data[4];  // 4 rows of __m256 (8 floats each, but we use 4 per row)
    float f32[16];
    struct {
        __m256 row0, row1, row2, row3;
    };
};

/**
 * @brief Socket connection for component assembly
 */
struct SocketConnection {
    uint32_t parent_socket_id;
    uint32_t child_socket_id;
    glm::mat4 relative_transform;
};

/**
 * @brief BVH node for collision detection
 */
struct BVHNode {
    BoundingBox bounds;             // AABB for this node
    uint32_t left_child;            // Index of left child (-1 if leaf)
    uint32_t right_child;           // Index of right child (-1 if leaf)
    uint32_t component_index;       // Component index if leaf node
    uint32_t padding;               // Alignment
};

/**
 * @brief Collision BVH for efficient physics queries
 */
struct CollisionBVH {
    std::vector<BVHNode> nodes;     // BVH tree nodes
    uint32_t root_index;            // Root node index
};

/**
 * @brief Component blueprint for procedural generation
 */
struct ComponentBlueprint {
    AssetID mesh_id;                // Base mesh asset
    AssetID collision_mesh_id;      // Physics collision mesh
    __m256 local_transform;         // Position/orientation relative to parent (SIMD)
    uint32_t component_type;        // TURRET, ENGINE, TRACK, WEAPON, etc.
    uint32_t attachment_points[8];  // Socket IDs for child components
    uint32_t material_overrides[4]; // Material variations
    float health_max;               // Component health
    float armor_thickness;          // Armor properties
    bool is_destructible;           // Can be damaged/destroyed
};

/**
 * @brief Vehicle blueprint for procedural assembly
 */
struct VehicleBlueprint {
    std::string vehicle_name;       // Human-readable name
    uint32_t vehicle_type;          // TANK, APC, ARTILLERY, AIRCRAFT
    std::vector<ComponentBlueprint> components; // All components
    std::vector<SocketConnection> connections;  // Parent-child relationships
    BoundingBox world_bounds;       // Precomputed world-space bounds
    float mass_total;               // Total vehicle mass
};

/**
 * @brief Material override for runtime customization
 */
struct MaterialOverride {
    uint32_t material_slot;
    AssetID material_id;
    glm::vec4 color_override;
};

/**
 * @brief Component state for damage visualization
 */
struct ComponentState {
    uint32_t component_index;
    float health_current;
    bool is_destroyed;
    std::vector<MaterialOverride> damage_overrides;
};

/**
 * @brief LOD level definition
 */
struct LODLevel {
    uint32_t level_index;           // 0 = highest detail, N = lowest
    float max_distance;             // Max distance for this LOD
    float triangle_count;           // Triangle count for this LOD
    AssetID mesh_id;                // LOD-specific mesh asset
    AssetID material_id;            // Simplified materials
    uint32_t bone_count;            // For skinned meshes
    bool has_physics_proxy;         // Simplified collision for distant objects
    float screen_coverage_threshold; // Screen space percentage threshold
};

/**
 * @brief Hierarchical LOD system
 */
struct ModelLODSystem {
    std::vector<LODLevel> lod_levels_; // Sorted by distance (closest first)
    float current_distance_;        // Current camera distance
    uint32_t active_lod_;           // Currently active LOD level
    std::atomic<bool> needs_update_; // Dirty flag for LOD transition
};

/**
 * @brief Model instance for rendering
 */
struct ModelInstance {
    EntityID entity_id;             // Associated game entity
    AssetID base_model_id;          // Original model blueprint
    SIMDTransform world_transform;  // World position/orientation/scale (SIMD 4x4 matrix)
    uint32_t current_lod_level;     // Active LOD level
    std::vector<MaterialOverride> material_overrides; // Runtime material changes
    std::vector<ComponentState> component_states;     // Damage states per component
    std::vector<glm::mat4> component_world_transforms; // World transform for each component
    std::vector<Vertex> visual_vertices;              // Generated visual geometry
    std::vector<uint32_t> visual_indices;
    std::vector<Vertex> collision_vertices;           // Generated collision geometry
    std::vector<uint32_t> collision_indices;
    CollisionBVH collision_bvh;                       // BVH for collision queries
    BoundingBox world_bounds;       // Cached world-space bounds
    uint32_t render_flags;          // Visibility, shadows, reflection flags

    bool is_deformable = false;     // Soft body or destructible object
    AssetID dynamic_mesh_asset_ids[2] = {0, 0}; // Double-buffered runtime mesh assets
    std::atomic<uint8_t> current_dynamic_mesh_index{0};

    ModelInstance() = default;
    ModelInstance(ModelInstance&& other) noexcept
        : entity_id(other.entity_id),
          base_model_id(other.base_model_id),
          world_transform(other.world_transform),
          current_lod_level(other.current_lod_level),
          material_overrides(std::move(other.material_overrides)),
          component_states(std::move(other.component_states)),
          component_world_transforms(std::move(other.component_world_transforms)),
          visual_vertices(std::move(other.visual_vertices)),
          visual_indices(std::move(other.visual_indices)),
          collision_vertices(std::move(other.collision_vertices)),
          collision_indices(std::move(other.collision_indices)),
          collision_bvh(std::move(other.collision_bvh)),
          world_bounds(other.world_bounds),
          render_flags(other.render_flags),
          is_deformable(other.is_deformable),
          dynamic_mesh_asset_ids{other.dynamic_mesh_asset_ids[0], other.dynamic_mesh_asset_ids[1]},
          current_dynamic_mesh_index(other.current_dynamic_mesh_index.load())
    {
    }

    ModelInstance& operator=(ModelInstance&& other) noexcept {
        if (this != &other) {
            entity_id = other.entity_id;
            base_model_id = other.base_model_id;
            world_transform = other.world_transform;
            current_lod_level = other.current_lod_level;
            material_overrides = std::move(other.material_overrides);
            component_states = std::move(other.component_states);
            component_world_transforms = std::move(other.component_world_transforms);
            visual_vertices = std::move(other.visual_vertices);
            visual_indices = std::move(other.visual_indices);
            collision_vertices = std::move(other.collision_vertices);
            collision_indices = std::move(other.collision_indices);
            collision_bvh = std::move(other.collision_bvh);
            world_bounds = other.world_bounds;
            render_flags = other.render_flags;
            is_deformable = other.is_deformable;
            dynamic_mesh_asset_ids[0] = other.dynamic_mesh_asset_ids[0];
            dynamic_mesh_asset_ids[1] = other.dynamic_mesh_asset_ids[1];
            current_dynamic_mesh_index.store(other.current_dynamic_mesh_index.load());
        }
        return *this;
    }
};

// ============================================================================
// MODEL SYSTEM CLASSES
// ============================================================================

/**
 * @brief Procedural vehicle generator
 */
class ProceduralGenerator {
public:
    ProceduralGenerator();
    ~ProceduralGenerator();

    /**
     * Generate vehicle from blueprint
     * @param blueprint Vehicle blueprint
     * @param customization Runtime customization
     * @param output_instance Generated model instance
     * @return true if successful
     */
    bool generate_vehicle_from_blueprint(
        const VehicleBlueprint& blueprint,
        const std::vector<MaterialOverride>& customization,
        ModelInstance& output_instance
    );

    /**
     * Create cube primitive mesh
     * @param size Cube size (width, height, depth)
     * @param output_vertices Output vertex array
     * @param output_indices Output index array
     */
    void create_cube_primitive(
        const glm::vec3& size,
        std::vector<Vertex>& output_vertices,
        std::vector<uint32_t>& output_indices
    );

    /**
     * Create cylinder primitive mesh
     * @param radius Cylinder radius
     * @param height Cylinder height
     * @param segments Number of segments around circumference
     * @param output_vertices Output vertex array
     * @param output_indices Output index array
     */
    void create_cylinder_primitive(
        float radius,
        float height,
        uint32_t segments,
        std::vector<Vertex>& output_vertices,
        std::vector<uint32_t>& output_indices
    );

    /**
     * Create sphere primitive mesh
     * @param radius Sphere radius
     * @param segments Number of segments in both directions
     * @param output_vertices Output vertex array
     * @param output_indices Output index array
     */
    void create_sphere_primitive(
        float radius,
        uint32_t segments,
        std::vector<Vertex>& output_vertices,
        std::vector<uint32_t>& output_indices
    );

    /**
     * Validate blueprint integrity
     * @param blueprint Blueprint to validate
     * @return true if valid
     */
    bool validate_blueprint(const VehicleBlueprint& blueprint) const;

private:
    // Internal generation helpers
    void apply_hierarchical_transforms(const VehicleBlueprint& blueprint, ModelInstance& instance);
    void generate_collision_mesh(const VehicleBlueprint& blueprint, ModelInstance& instance);
    void apply_material_customization(const std::vector<MaterialOverride>& customization, ModelInstance& instance);
    BoundingBox compute_world_bounds(const ModelInstance& instance) const;
};

/**
 * @brief Dynamic LOD selection system
 */
class LODManager {
public:
    LODManager();
    ~LODManager();

    /**
     * Update LOD selections for instances
     * @param instances Model instances to update
     * @param camera Camera for distance calculations
     * @param frame_time Frame time for hysteresis
     */
    void update_lod_selections(
        std::vector<ModelInstance*>& instances,
        const glm::vec3& camera_position,
        float frame_time
    );

    /**
     * Add LOD level to system
     * @param level LOD level definition
     */
    void add_lod_level(const LODLevel& level);

    /**
     * Get active LOD for distance
     * @param distance Distance from camera
     * @return LOD level index
     */
    uint32_t get_lod_for_distance(float distance) const;

private:
    std::vector<LODLevel> lod_levels_;
    float hysteresis_buffer_; // 20% distance buffer
};

/**
 * @brief Instance management system
 */
class InstanceManager {
public:
    InstanceManager();
    ~InstanceManager();

    /**
     * Create new model instance
     * @param entity_id Associated entity
     * @param base_model_id Base model asset
     * @return Instance ID or 0 on failure
     */
    uint64_t create_instance(EntityID entity_id, AssetID base_model_id);

    /**
     * Destroy model instance
     * @param instance_id Instance to destroy
     */
    void destroy_instance(uint64_t instance_id);

    /**
     * Get model instance
     * @param instance_id Instance ID
     * @return Pointer to instance or nullptr
     */
    ModelInstance* get_instance(uint64_t instance_id);

    /**
     * Find instance by entity ID
     * @param entity_id Associated entity
     * @return Pointer to instance or nullptr
     */
    ModelInstance* get_instance_by_entity(EntityID entity_id);

    /**
     * Update instance transform
     * @param instance_id Instance ID
     * @param transform New world transform
     */
    void update_transform(uint64_t instance_id, const SIMDTransform& transform);

    /**
     * Get all visible instances
     * @param camera Camera for frustum culling
     * @return Vector of visible instances
     */
    std::vector<ModelInstance*> get_visible_instances(const glm::mat4& view_proj_matrix);

private:
    /**
     * Extract frustum planes from view-projection matrix
     * @param view_proj_matrix Combined view-projection matrix
     * @param planes Output array of 6 planes (a,b,c,d)
     */
    void extract_frustum_planes(const glm::mat4& view_proj_matrix, glm::vec4 planes[6]);

    /**
     * Check if AABB is outside the frustum
     * @param aabb Bounding box to test
     * @param planes Frustum planes
     * @return true if AABB is completely outside frustum
     */
    bool is_aabb_outside_frustum(const BoundingBox& aabb, const glm::vec4 planes[6]);

    std::vector<ModelInstance> instances_;
    std::vector<uint64_t> free_slots_;
};

/**
 * @brief Main model system interface
 */
class ModelSystem {
public:
    ModelSystem();
    ~ModelSystem();

    /**
     * Initialize model system
     * @param rendering_engine Rendering engine reference
     * @param physics_core Physics core reference
     * @param assembly_system Assembly system reference
     */
    void initialize(
        RenderingEngine* rendering_engine,
        physics_core::PhysicsCore* physics_core,
        AssemblySystem* assembly_system
    );

    /**
     * Generate vehicle from blueprint
     * @param blueprint Vehicle blueprint
     * @param customization Runtime customization
     * @param output_instance Generated model instance
     * @return true if successful
     */
    bool generate_vehicle_from_blueprint(
        const VehicleBlueprint& blueprint,
        const std::vector<MaterialOverride>& customization,
        ModelInstance& output_instance
    );

    /**
     * Update LOD selections
     * @param camera_position Camera position
     * @param frame_time Frame time
     */
    void update_lod_selections(const glm::vec3& camera_position, float frame_time);

    /**
     * Prepare instanced rendering
     * @param cmd_buffer Vulkan command buffer
     * @param camera Current camera for frustum culling
     */
    void prepare_instanced_rendering(VkCommandBuffer cmd_buffer, const Camera& camera);

    /**
     * Sync model to physics
     * @param instance Model instance
     * @param physics Physics core
     */
    void sync_model_to_physics(const ModelInstance& instance, physics_core::PhysicsCore& physics);

    /**
     * Create model from assembly
     * @param assembly Assembly blueprint
     * @param output Output instance
     * @return true if successful
     */
    bool create_model_from_assembly(const void* assembly, ModelInstance& output);

    /**
     * Submit instance to renderer
     * @param instance Model instance
     * @param renderer Rendering engine
     */
    void submit_to_renderer(const ModelInstance& instance, RenderingEngine& renderer);

    /**
     * Initialize buffers for a deformable model instance.
     * @param instance Model instance
     * @return true if initialization succeeded
     */
    bool initialize_deformable_instance(ModelInstance& instance);

    /**
     * Update runtime deformable meshes from soft body simulation
     */
    void update_deformable_meshes(physics_core::PhysicsCore& physics);

    /**
     * Create model instance
     * @param entity_id Associated entity
     * @param base_model_id Base model asset
     * @return Instance ID or 0 on failure
     */
    uint64_t create_instance(EntityID entity_id, AssetID base_model_id);

    /**
     * Destroy model instance
     * @param instance_id Instance to destroy
     */
    void destroy_instance(uint64_t instance_id);

    /**
     * Get model instance
     * @param instance_id Instance ID
     * @return Pointer to instance or nullptr
     */
    ModelInstance* get_instance(uint64_t instance_id);

    /**
     * Get visible instances after frustum culling
     * @param view_proj_matrix View-projection matrix
     * @return Vector of visible model instances
     */
    std::vector<ModelInstance*> get_visible_instances(const glm::mat4& view_proj_matrix);

    /**
     * Apply damage visualization
     * @param vehicle_id Vehicle entity ID
     * @param damage_event Damage event
     * @param instance Model instance
     */
    void apply_damage_visualization(
        EntityID vehicle_id,
        const void* damage_event,
        ModelInstance& instance
    );

private:
    RenderingEngine* rendering_engine_;
    physics_core::PhysicsCore* physics_core_;
    AssemblySystem* assembly_system_;

    ProceduralGenerator procedural_generator_;
    LODManager lod_manager_;
    InstanceManager instance_manager_;

    std::vector<ModelInstance*> visible_instances_;
};

}  // namespace rendering_engine