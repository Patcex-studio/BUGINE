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
#include "world_space_manager.h"
#include "physics_body.h"
#include "integrators.h"
#include "matter_systems.h"
#include "physics_thread_pool.h"
#include "collision_system.h"
#include "constraint_system.h"
#include "soft_body_system.h"
#include "sph_system.h"
#include "terrain_system.h"
#include "environment_system.h"
#include "destruction_system.h"
#include "thermal_system.h"
#include "acoustics_system.h"
#include "crew_damage_system.h"
#include "hybrid_precision.h"
#include <functional>
#include <memory>
#include <vector>

namespace physics_core {

// ============================================================================
// Main Physics Core Interface
// ============================================================================

/**
 * @class PhysicsCore
 * @brief Main interface for the physics engine
 * 
 * Manages:
 * - Coordinate systems (world and local spaces)
 * - Matter subsystems (rigid bodies, fluids, gases)
 * - Physics integration
 * - Multi-threaded execution
 */
class PhysicsCore {
public:
    // Callback types
    using OnBodyAddedCallback = std::function<void(EntityID)>;
    using OnBodyRemovedCallback = std::function<void(EntityID)>;
    using OnCollisionCallback = std::function<void(EntityID, EntityID, const Vec3&)>;

    PhysicsCore();
    ~PhysicsCore();

    // ========== Lifecycle ==========

    /**
     * Initialize physics engine
     * @param thread_count Number of worker threads (0 = auto)
     * @param integrator_type 0=Verlet, 1=RK4, 2=Euler
     */
    void initialize(size_t thread_count = 0, int integrator_type = 0);

    /**
     * Shutdown physics engine
     */
    void shutdown();

    /**
     * Check if engine is initialized
     */
    bool is_initialized() const { return initialized_; }

    // ========== Simulation Control ==========

    /**
     * Update physics simulation
     * @param dt Time step in seconds (typically 1/60 or 1/120)
     */
    void update(float dt);

    /**
     * Set gravity
     */
    void set_gravity(const Vec3& gravity);

    /**
     * Get gravity
     */
    Vec3 get_gravity() const;

    /**
     * Set simulation time scale
     */
    void set_time_scale(float scale) { time_scale_ = scale; }

    /**
     * Pause simulation
     */
    void set_paused(bool paused) { paused_ = paused; }

    /**
     * Get pause state
     */
    bool is_paused() const { return paused_; }

    // ========== Body Management ==========

    /**
     * Create a rigid body
     * @param position Initial world position
     * @param mass Body mass (kg)
     * @param inertia Inertia tensor
     * @return Entity ID
     */
    EntityID create_rigid_body(
        const Vec3& position,
        float mass = 1.0f,
        const Mat3x3& inertia = Mat3x3::identity()
    );

    /**
     * Create a static body (no movement)
     * @param position Position
     * @return Entity ID
     */
    EntityID create_static_body(const Vec3& position);

    /**
     * Create a kinematic body (moves but no dynamics)
     * @param position Position
     * @param mass Effective mass
     * @return Entity ID
     */
    EntityID create_kinematic_body(const Vec3& position, float mass = 1.0f);

    /**
     * Create a fluid particle
     */
    EntityID create_fluid_particle(const Vec3& position, float mass = 1.0f);

    /**
     * Create a fluid source that emits particles
     */
    EntityID create_fluid_source(FluidType type, const Vec3& position, float rate = 1.0f);

    /**
     * Create a gas particle
     */
    EntityID create_gas_particle(const Vec3& position, float mass = 0.1f);

    /**
     * Destroy a body
     */
    void destroy_body(EntityID id);

    /**
     * Get body
     */
    PhysicsBody* get_body(EntityID id);

    /**
     * Get total body count
     */
    size_t get_body_count() const;

    /**
     * Get bodies by subsystem
     */
    const std::vector<PhysicsBody>& get_rigid_bodies() const;
    const std::vector<PhysicsBody>& get_fluid_particles() const;
    const std::vector<PhysicsBody>& get_gas_particles() const;

    size_t create_distance_constraint(
        EntityID body_a,
        EntityID body_b,
        const Vec3& local_anchor_a,
        const Vec3& local_anchor_b,
        float target_distance,
        float compliance = 0.0f
    );

    size_t create_hinge_constraint(
        EntityID body_a,
        EntityID body_b,
        const Vec3& local_anchor_a,
        const Vec3& local_anchor_b,
        const Vec3& hinge_axis_a,
        const Vec3& hinge_axis_b,
        float min_angle_rad,
        float max_angle_rad,
        float compliance = 0.0f
    );

    size_t create_soft_rope(
        uint32_t particle_count,
        float length,
        float particle_radius,
        const Vec3& start,
        const Vec3& direction,
        bool pin_start = true
    );

    size_t create_soft_cloth(
        uint32_t width,
        uint32_t height,
        float spacing,
        const Vec3& origin,
        bool pin_corners = true
    );

    // ========== Collision System Integration ==========

    /**
     * Register a collision shape for narrow-phase queries
     */
    size_t register_collision_shape(CollisionShape&& shape);

    /**
     * Update the broad-phase BVH from registered shapes
     */
    void update_broad_phase();

    /**
     * Solve narrow-phase collisions for the current frame
     */
    void solve_narrow_phase(float dt);

    /**
     * Resolve generated contact manifolds using SIMD-friendly data
     */
    void resolve_contacts_simd(ContactManifolds& contacts);

    // ========== Force and Impulse ==========

    /**
     * Apply force to a body
     * @param id Body ID
     * @param force Force vector (N)
     */
    void apply_force(EntityID id, const Vec3& force);

    /**
     * Apply impulse to a body
     * @param id Body ID
     * @param impulse Impulse vector (kg*m/s)
     * @param point Point of application in world space
     */
    void apply_impulse(EntityID id, const Vec3& impulse, const Vec3& point = Vec3());

    /**
     * Apply torque to a body (rotational force)
     * @param id Body ID
     * @param torque Torque vector (N*m)
     */
    void apply_torque(EntityID id, const Vec3& torque);

    // ========== Environment Integration ==========

    /**
     * Apply environment forces to bodies
     * @param bodies Array of physics bodies
     * @param forces Ground interaction forces
     * @param weather Current weather state
     * @param count Number of bodies/forces
     */
    void apply_environment_forces(
        PhysicsBody* bodies,
        const GroundInteractionForces* forces,
        const WeatherState& weather,
        size_t count
    );

    // ========== Coordinate Systems ==========

    /**
     * Get world space manager
     */
    WorldSpaceManager& get_world_space_manager() { return world_space_; }

    /**
     * Create a local coordinate frame
     */
    EntityID create_local_frame(
        const Vec3& position,
        const Mat3x3& orientation = Mat3x3::identity(),
        double scale = 1.0,
        EntityID parent_id = 0
    );

    /**
     * Destroy a local frame
     */
    void destroy_local_frame(EntityID frame_id);

    // ========== Environment Access ==========

    /**
     * Get terrain system
     */
    TerrainSystem* get_terrain_system() { return terrain_system_.get(); }

    /**
     * Get environment system
     */
    EnvironmentSystem* get_environment_system() { return environment_system_.get(); }

    /**
     * Get destruction system
     */
    DestructionSystem* get_destruction_system() { return destruction_system_.get(); }

    /**
     * Get soft body system
     */
    SoftBodySystem* get_soft_body_system() { return soft_body_system_.get(); }

    /**
     * Get thermal system
     */
    ThermalSystem* get_thermal_system() { return thermal_system_.get(); }

    /**
     * Get acoustics system
     */
    AcousticsSystem* get_acoustics_system() { return acoustics_system_.get(); }

    /**
     * Get crew damage system (for querying crew status)
     */
    CrewDamageSystem* get_crew_damage_system() { return crew_damage_system_.get(); }

    /**
     * Get hybrid precision system for floating origin management
     */
    HybridPrecisionSystem* get_hybrid_precision_system() { return hybrid_precision_.get(); }

    // ========== Callbacks ==========

    /**
     * Register body added callback
     */
    void on_body_added(OnBodyAddedCallback cb) { body_added_cb_ = cb; }

    /**
     * Register body removed callback
     */
    void on_body_removed(OnBodyRemovedCallback cb) { body_removed_cb_ = cb; }

    /**
     * Register collision callback
     */
    void on_collision(OnCollisionCallback cb) { collision_cb_ = cb; }

    // ========== Statistics and Debug ==========

    /**
     * Get performance statistics
     */
    struct Stats {
        size_t total_bodies;
        size_t rigid_bodies;
        size_t fluid_particles;
        size_t gas_particles;
        double last_update_time_ms;
        float avg_frame_time_ms;
        size_t frame_count;
    };

    Stats get_stats() const;

    /**
     * Get memory usage
     */
    size_t get_memory_usage() const;

    /**
     * Enable debug output
     */
    void set_debug_enabled(bool enabled) { debug_enabled_ = enabled; }

private:
    bool initialized_;
    bool paused_;
    float time_scale_;
    bool debug_enabled_;

    // Core systems
    WorldSpaceManager world_space_;
    std::unique_ptr<RigidBodySystem> rigid_body_system_;
    std::unique_ptr<FluidSystem> fluid_system_;
    std::unique_ptr<GasSystem> gas_system_;
    std::unique_ptr<CollisionSystem> collision_system_;
    std::unique_ptr<ConstraintSolver> constraint_solver_;
    std::unique_ptr<SoftBodySystem> soft_body_system_;
    std::unique_ptr<IntegratorBase> integrator_;
    std::unique_ptr<PhysicsThreadPool> thread_pool_;

    // Environment systems
    std::unique_ptr<TerrainSystem> terrain_system_;
    std::unique_ptr<EnvironmentSystem> environment_system_;
    std::unique_ptr<DestructionSystem> destruction_system_;

    // Specialized subsystems
    std::unique_ptr<ThermalSystem> thermal_system_;
    std::unique_ptr<AcousticsSystem> acoustics_system_;
    std::unique_ptr<CrewDamageSystem> crew_damage_system_;
    std::unique_ptr<HybridPrecisionSystem> hybrid_precision_;

    // Callbacks
    OnBodyAddedCallback body_added_cb_;
    OnBodyRemovedCallback body_removed_cb_;
    OnCollisionCallback collision_cb_;

    // Statistics
    mutable Stats stats_;
    mutable double last_update_time_ms_;

    // Helpers
    void update_all_systems(float dt);
    void detect_and_handle_collisions(float dt);
};

}  // namespace physics_core
