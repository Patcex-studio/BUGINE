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

#include <memory>
#include <vector>
#include "blueprint_engine.h"
#include "procedural_geometry_generator.h"
#include "texture_synthesis_system.h"
#include <vulkan/vulkan.h>

namespace model_generation {

// Forward declarations
struct GenerationOptions;
struct GeneratedMesh;
struct ModelInstance;
struct RuntimeCustomization;
struct HistoricalVehicleSpecs;
struct VehicleBlueprint;
struct DamageEvent;
struct EnvironmentState;
struct PerformanceBudget;

// Main system class
class ModelGenerationSystem {
public:
    ModelGenerationSystem(VkDevice device, VkPhysicalDevice physical_device, 
                         VkCommandPool command_pool, VkQueue queue);
    ~ModelGenerationSystem();

    // Core generation functions
    bool generate_mesh_from_blueprint(
        const VehicleBlueprintDefinition& blueprint,
        const GenerationOptions& options,
        GeneratedMesh& output_mesh
    );

    bool generate_historical_textures(
        const HistoricalVehicleSpecs& vehicle_specs,
        const TextureSynthesisParameters& params,
        GeneratedTextureSet& output_textures
    );

    bool apply_runtime_customization(
        uint64_t vehicle_entity,
        const RuntimeCustomization& customization,
        ModelInstance& updated_instance
    );

    // Integration functions
    bool create_model_from_historical_specs(
        const HistoricalVehicleSpecs& specs,
        ModelInstance& output_instance
    );

    bool generate_modular_vehicle_model(
        const VehicleBlueprint& modular_blueprint,
        ModelInstance& output_instance
    );

    bool prepare_for_rendering(
        ModelInstance& instance,
        class RenderingEngine& renderer
    );

    // Specialized modes
    bool optimize_battlefield_models(
        const std::vector<uint64_t>& battlefield_vehicles,
        const PerformanceBudget& budget
    );

    bool update_damage_visualization(
        uint64_t vehicle_entity,
        const DamageEvent& damage_event
    );

    bool apply_environmental_effects(
        uint64_t vehicle_entity,
        const EnvironmentState& environment
    );

private:
    std::unique_ptr<ProceduralGeometryGenerator> geometry_generator_;
    std::unique_ptr<TextureSynthesisSystem> texture_synthesis_;
    
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VkCommandPool command_pool_;
    VkQueue queue_;

    // Internal caches and pools
    std::vector<std::unique_ptr<GeneratedMesh>> mesh_cache_;
    std::vector<GeneratedTextureSet> texture_cache_;

    // Helper methods
    bool validate_blueprint(const VehicleBlueprintDefinition& blueprint);
    bool traverse_blueprint_tree(const VehicleBlueprintDefinition& blueprint, 
                                uint32_t node_index, GeneratedMesh& mesh);
    bool generate_collision_mesh(const GeneratedMesh& visual_mesh, GeneratedMesh& collision_mesh);
    bool create_lod_levels(GeneratedMesh& mesh);
};

// Supporting structures
struct GenerationOptions {
    uint32_t max_lod_levels = 4;
    uint32_t target_triangle_count = 10000;
    bool generate_collision_mesh = true;
    bool optimize_topology = true;
    float simplification_factor = 0.8f;
};

struct GeneratedMesh {
    std::vector<std::unique_ptr<Mesh>> lod_meshes;
    std::unique_ptr<Mesh> collision_mesh;
    BoundingBox bounds;
    uint64_t vertex_count;
    uint64_t triangle_count;
};

struct ModelInstance {
    uint64_t entity_id;
    GeneratedMesh mesh;
    GeneratedTextureSet textures;
    std::vector<MaterialSlot> material_overrides;
    uint32_t current_lod_level;
};

struct RuntimeCustomization {
    std::vector<MaterialSlot> material_changes;
    float damage_level;
    float wear_level;
    bool apply_environmental_dirt;
};

// Additional structures for integration
struct VehicleBlueprint {
    // Placeholder - integrate with assembly system
    std::string name;
};

struct DamageEvent {
    uint64_t entity_id;
    float damage_amount;
    uint32_t damage_type; // PENETRATION, EXPLOSION, etc.
    __m128 damage_location;
};

struct EnvironmentState {
    uint32_t weather_type; // CLEAR, RAIN, SNOW, MUD
    float dirt_level;
    float moisture_level;
};

struct PerformanceBudget {
    uint32_t max_vehicles;
    uint32_t target_fps;
    float lod_distance_factor;
};

} // namespace model_generation