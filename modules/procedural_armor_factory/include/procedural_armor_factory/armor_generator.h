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

#include "parametric_template.h"
#include <rendering_engine/model_system.h>
#include <physics_core/physics_core.h>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

namespace procedural_armor_factory {

using rendering_engine::Vertex;
using rendering_engine::ModelInstance;
using rendering_engine::MaterialOverride;
using physics_core::Vec3;
using physics_core::Mat3x3;
using physics_core::EntityID;

/**
 * @struct MaterialColor
 * @brief Color scheme for different tank components
 */
struct MaterialColor {
    glm::vec4 hull;              // hull color
    glm::vec4 turret;            // turret color
    glm::vec4 wheels;            // wheel/track color
    glm::vec4 gun;               // gun barrel color
    glm::vec4 details;           // misc details color
};

/**
 * @struct TankComponent
 * @brief Tank component type enumeration
 */
enum class TankComponent : uint8_t {
    HULL = 0,
    TURRET = 1,
    GUN = 2,
    WHEEL_FL = 3,           // front-left
    WHEEL_FR = 4,           // front-right
    WHEEL_RL = 5,           // rear-left
    WHEEL_RR = 6,           // rear-right
    TRACK_L = 7,            // left track
    TRACK_R = 8,            // right track
    ENGINE = 9,
    FUEL_TANK = 10,
    COUNT = 11
};

/**
 * @struct DamageComponentMapping
 * @brief Maps a vertex range to a tank component for damage
 */
struct DamageComponentMapping {
    TankComponent component;
    float health;               // HP of component
    float armor_thickness;      // mm
    uint32_t vertex_start;      // start index in ModelInstance::visual_vertices
    uint32_t vertex_count;      // number of vertices
};

/**
 * @struct PhysicsMapping
 * @brief Physics parameters for a tank model
 */
struct PhysicsMapping {
    float mass;                         // kg
    glm::vec3 center_of_mass;          // local coordinates
    glm::mat3 inertia_tensor;          // 3x3 inertia tensor
    std::vector<glm::vec3> convex_hull; // simplified collision hull vertices
};

/**
 * @class ProceduralArmorGenerator
 * @brief Generates 3D tank models from parametric templates
 *
 * Does NOT allocate in hot paths. Uses precomputed vertex pools.
 * Deterministic RNG for multiplay synchronization.
 */
class ProceduralArmorGenerator {
public:
    ProceduralArmorGenerator();
    ~ProceduralArmorGenerator();

    /**
     * Generate a complete ModelInstance from template
     * @param tmpl Tank template with historical specs
     * @param materials Material color scheme
     * @return ModelInstance ready for rendering and physics
     */
    ModelInstance generate(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials = default_materials()
    );

    /**
     * Compute physics mapping for a template
     * @param tmpl Tank template
     * @return Physics parameters (mass, inertia, CoM)
     */
    static PhysicsMapping compute_physics_mapping(const ParametricTankTemplate& tmpl);

    /**
     * Create damage mapping from generated model
     * @param instance Generated ModelInstance
     * @param tmpl Original template
     * @return Vector of component damage mappings
     */
    static std::vector<DamageComponentMapping> create_damage_mapping(
        const ModelInstance& instance,
        const ParametricTankTemplate& tmpl
    );

    /**
     * Default material colors (Soviet tank scheme)
     */
    static MaterialColor default_materials();

private:
    // === Generation Helpers ===

    /**
     * Generate hull (main body) geometry
     * @param tmpl Tank template
     * @param materials Color scheme
     * @param out_vertices Output vertex buffer
     * @param out_indices Output index buffer
     */
    void generate_hull(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials,
        std::vector<Vertex>& out_vertices,
        std::vector<uint32_t>& out_indices
    );

    /**
     * Generate road wheels and tracks
     */
    void generate_suspension(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials,
        std::vector<Vertex>& out_vertices,
        std::vector<uint32_t>& out_indices
    );

    /**
     * Generate turret (rotating tower)
     */
    void generate_turret(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials,
        std::vector<Vertex>& out_vertices,
        std::vector<uint32_t>& out_indices
    );

    /**
     * Generate gun barrel and accessories
     */
    void generate_gun(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials,
        std::vector<Vertex>& out_vertices,
        std::vector<uint32_t>& out_indices
    );

    /**
     * Generate miscellaneous details (fuel cans, tools, etc.)
     */
    void generate_details(
        const ParametricTankTemplate& tmpl,
        const MaterialColor& materials,
        std::vector<Vertex>& out_vertices,
        std::vector<uint32_t>& out_indices
    );

    /**
     * Deterministic random number generator (LCG)
     */
    class DeterministicRNG {
        uint64_t state_;
    public:
        explicit DeterministicRNG(uint64_t seed) noexcept
            : state_(seed ^ 0x9E3779B97F4A7C15ULL) {}

        float next_float() noexcept {
            state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<float>((state_ >> 32) & 0xFFFFFF) / 16777216.0f;
        }

        int next_int(int min, int max) noexcept {
            return min + static_cast<int>(next_float() * (max - min + 1));
        }

        glm::vec3 next_vec3(float range) noexcept {
            return glm::vec3(
                (next_float() * 2.0f - 1.0f) * range,
                (next_float() * 2.0f - 1.0f) * range,
                (next_float() * 2.0f - 1.0f) * range
            );
        }
    };

    // Reusable vertex/index buffers (no allocations in hot path)
    std::vector<Vertex> vertex_pool_;
    std::vector<uint32_t> index_pool_;
    size_t vertex_offset_;
    size_t index_offset_;

    /**
     * Allocate block from vertex pool without allocation
     */
    std::vector<Vertex>::iterator allocate_vertices(size_t count);

    /**
     * Allocate block from index pool
     */
    std::vector<uint32_t>::iterator allocate_indices(size_t count);

    /**
     * Reset pools for next model
     */
    void reset_pools();
};

} // namespace procedural_armor_factory
