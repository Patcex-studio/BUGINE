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
#include "physics_core/physics_core.h"
#include "physics_core/constraint_system.h"
#include "physics_core/soft_body_system.h"
#include "physics_core/profile_macros.h"
#include "common/event_bus.h"
#include <chrono>
#include <cstddef>

namespace physics_core {

PhysicsCore::PhysicsCore()
    : initialized_(false),
      paused_(false),
      time_scale_(1.0f),
      debug_enabled_(false),
      last_update_time_ms_(0.0) {
}

PhysicsCore::~PhysicsCore() {
    shutdown();
}

// ========== Lifecycle ==========

void PhysicsCore::initialize(size_t thread_count, int integrator_type) {
    if (initialized_) {
        return;
    }
    
    // Create thread pool
    thread_pool_ = std::make_unique<PhysicsThreadPool>(thread_count);
    
    // Create matter subsystems
    rigid_body_system_ = std::make_unique<RigidBodySystem>();
    fluid_system_ = std::make_unique<FluidSystem>();
    gas_system_ = std::make_unique<GasSystem>();
    collision_system_ = std::make_unique<CollisionSystem>();
    collision_system_->initialize_async_builder(thread_pool_.get());
    constraint_solver_ = std::make_unique<ConstraintSolver>();
    soft_body_system_ = std::make_unique<SoftBodySystem>();

    // Create environment subsystems
    terrain_system_ = std::make_unique<TerrainSystem>();
    environment_system_ = std::make_unique<EnvironmentSystem>();
    destruction_system_ = std::make_unique<DestructionSystem>();

    // Create specialized subsystems
    thermal_system_ = std::make_unique<ThermalSystem>(fluid_system_.get());
    acoustics_system_ = std::make_unique<AcousticsSystem>();
    crew_damage_system_ = std::make_unique<CrewDamageSystem>();
    hybrid_precision_ = std::make_unique<HybridPrecisionSystem>();

    // Create integrator
    switch (integrator_type) {
        case 0:
            integrator_ = std::make_unique<VerletIntegrator>();
            break;
        case 1:
            integrator_ = std::make_unique<RK4Integrator>();
            break;
        case 2:
        default:
            integrator_ = std::make_unique<EulerIntegrator>();
            break;
    }
    
    if (debug_enabled_) {
        std::cout << "[PhysicsCore] Initialized with integrator: " << integrator_->name() << std::endl;
        std::cout << "[PhysicsCore] Thread pool size: " << thread_pool_->get_thread_count() << std::endl;
    }
    
    // Initialize environment systems
    terrain_system_->initialize(100.0f, 1.0f, 5);
    
    initialized_ = true;
    stats_.frame_count = 0;
}

void PhysicsCore::shutdown() {
    if (!initialized_) {
        return;
    }
    
    rigid_body_system_.reset();
    fluid_system_.reset();
    gas_system_.reset();
    collision_system_.reset();
    constraint_solver_.reset();
    soft_body_system_.reset();
    integrator_.reset();
    thread_pool_.reset();
    
    terrain_system_.reset();
    environment_system_.reset();
    destruction_system_.reset();

    // Shutdown specialized subsystems
    thermal_system_.reset();
    acoustics_system_.reset();
    crew_damage_system_.reset();
    hybrid_precision_.reset();
    
    initialized_ = false;
}

// ========== Simulation Control ==========

void PhysicsCore::update(float dt) {
    if (!initialized_ || paused_) {
        return;
    }
    
    dt *= time_scale_;
    
    PHYSICS_PROFILE_SCOPE("PhysicsCore.Update");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Update all matter subsystems
    update_all_systems(dt);
    
    // Detect and handle collisions
    detect_and_handle_collisions(dt);
    common::EventBus::DispatchAll();
#ifdef PHYSICS_PROFILING
    static int profile_frame_counter = 0;
    if (++profile_frame_counter >= 60) {
        ProfileManager::instance().print_summary(3.6);
        ProfileManager::instance().reset();
        profile_frame_counter = 0;
    }
#endif
    
    // Update statistics
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    if (stats_.frame_count == 0) {
        stats_.avg_frame_time_ms = elapsed_ms;
    } else {
        // Exponential moving average
        stats_.avg_frame_time_ms = stats_.avg_frame_time_ms * 0.95 + elapsed_ms * 0.05;
    }
    
    last_update_time_ms_ = elapsed_ms;
    stats_.last_update_time_ms = elapsed_ms;
    stats_.frame_count++;
}

void PhysicsCore::set_gravity(const Vec3& gravity) {
    if (rigid_body_system_) {
        rigid_body_system_->set_gravity(gravity);
    }
}

Vec3 PhysicsCore::get_gravity() const {
    return rigid_body_system_ ? rigid_body_system_->get_gravity() : Vec3(0, 0, 0);
}

// ========== Body Management ==========

EntityID PhysicsCore::create_rigid_body(
    const Vec3& position,
    float mass,
    const Mat3x3& inertia
) {
    static EntityID next_id = 1;
    EntityID id = next_id++;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(id, position, mass, inertia);
    rigid_body_system_->add_body(body);
    
    if (body_added_cb_) {
        body_added_cb_(id);
    }
    
    return id;
}

EntityID PhysicsCore::create_static_body(const Vec3& position) {
    static EntityID next_id = 100000;
    EntityID id = next_id++;
    
    PhysicsBody body = PhysicsBody::create_static_body(id, position);
    rigid_body_system_->add_body(body);
    
    if (body_added_cb_) {
        body_added_cb_(id);
    }
    
    return id;
}

EntityID PhysicsCore::create_kinematic_body(const Vec3& position, float mass) {
    static EntityID next_id = 200000;
    EntityID id = next_id++;
    
    PhysicsBody body = PhysicsBody::create_kinematic_body(id, position, mass);
    rigid_body_system_->add_body(body);
    
    if (body_added_cb_) {
        body_added_cb_(id);
    }
    
    return id;
}

EntityID PhysicsCore::create_fluid_particle(const Vec3& position, float mass) {
    static EntityID next_id = 300000;
    EntityID id = next_id++;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(id, position, mass, Mat3x3::identity());
    body.material_type = 1;  // Fluid
    fluid_system_->add_body(body);
    
    if (body_added_cb_) {
        body_added_cb_(id);
    }
    
    return id;
}

EntityID PhysicsCore::create_gas_particle(const Vec3& position, float mass) {
    static EntityID next_id = 400000;
    EntityID id = next_id++;
    
    PhysicsBody body = PhysicsBody::create_rigid_body(id, position, mass, Mat3x3::identity());
    body.material_type = 2;  // Gas
    gas_system_->add_body(body);
    
    if (body_added_cb_) {
        body_added_cb_(id);
    }
    
    return id;
}

EntityID PhysicsCore::create_fluid_source(FluidType type, const Vec3& position, float rate) {
    // Create a small emitter body for the fluid source and spawn a burst of particles.
    const uint32_t particle_count = static_cast<uint32_t>(std::max(3.0f, std::min(rate * 4.0f, 12.0f)));
    EntityID source_id = create_static_body(position);
    for (uint32_t i = 0; i < particle_count; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(particle_count)) * 2.0f * 3.14159265f;
        Vec3 offset = Vec3(std::cos(angle), 0.0, std::sin(angle)) * 0.2;
        Vec3 particle_pos = position + offset;
        EntityID particle_id = create_fluid_particle(particle_pos, 0.5f);
        PhysicsBody* body = get_body(particle_id);
        if (body) {
            body->velocity = Vec3(offset.x * 1.5, 0.5, offset.z * 1.5);
        }
    }
    return source_id;
}

void PhysicsCore::destroy_body(EntityID id) {
    if (rigid_body_system_->get_body(id)) {
        rigid_body_system_->remove_body(id);
    } else if (fluid_system_->get_body(id)) {
        fluid_system_->remove_body(id);
    } else if (gas_system_->get_body(id)) {
        gas_system_->remove_body(id);
    }
    
    if (body_removed_cb_) {
        body_removed_cb_(id);
    }
}

PhysicsBody* PhysicsCore::get_body(EntityID id) {
    PhysicsBody* body = rigid_body_system_->get_body(id);
    if (body) return body;
    
    body = fluid_system_->get_body(id);
    if (body) return body;
    
    return gas_system_->get_body(id);
}

size_t PhysicsCore::get_body_count() const {
    return rigid_body_system_->get_body_count() +
           fluid_system_->get_body_count() +
           gas_system_->get_body_count();
}

const std::vector<PhysicsBody>& PhysicsCore::get_rigid_bodies() const {
    return rigid_body_system_->get_all_bodies();
}

const std::vector<PhysicsBody>& PhysicsCore::get_fluid_particles() const {
    return fluid_system_->get_all_bodies();
}

const std::vector<PhysicsBody>& PhysicsCore::get_gas_particles() const {
    return gas_system_->get_all_bodies();
}

size_t PhysicsCore::create_distance_constraint(
    EntityID body_a,
    EntityID body_b,
    const Vec3& local_anchor_a,
    const Vec3& local_anchor_b,
    float target_distance,
    float compliance
) {
    return constraint_solver_ ? constraint_solver_->add_distance_constraint(
        body_a,
        body_b,
        local_anchor_a,
        local_anchor_b,
        target_distance,
        compliance
    ) : SIZE_MAX;
}

size_t PhysicsCore::create_hinge_constraint(
    EntityID body_a,
    EntityID body_b,
    const Vec3& local_anchor_a,
    const Vec3& local_anchor_b,
    const Vec3& hinge_axis_a,
    const Vec3& hinge_axis_b,
    float min_angle_rad,
    float max_angle_rad,
    float compliance
) {
    return constraint_solver_ ? constraint_solver_->add_hinge_constraint(
        body_a,
        body_b,
        local_anchor_a,
        local_anchor_b,
        hinge_axis_a,
        hinge_axis_b,
        min_angle_rad,
        max_angle_rad,
        compliance
    ) : SIZE_MAX;
}

size_t PhysicsCore::create_soft_rope(
    uint32_t particle_count,
    float length,
    float particle_radius,
    const Vec3& start,
    const Vec3& direction,
    bool pin_start
) {
    return soft_body_system_ ? soft_body_system_->create_rope(particle_count, length, particle_radius, start, direction, pin_start) : SIZE_MAX;
}

size_t PhysicsCore::create_soft_cloth(
    uint32_t width,
    uint32_t height,
    float spacing,
    const Vec3& origin,
    bool pin_corners
) {
    return soft_body_system_ ? soft_body_system_->create_cloth(width, height, spacing, origin, pin_corners) : SIZE_MAX;
}

size_t PhysicsCore::register_collision_shape(CollisionShape&& shape) {
    return collision_system_ ? collision_system_->register_collision_shape(std::move(shape)) : 0;
}

void PhysicsCore::update_broad_phase() {
    if (collision_system_) {
        collision_system_->update_broad_phase();
    }
}

void PhysicsCore::solve_narrow_phase(float dt) {
    if (collision_system_) {
        collision_system_->solve_narrow_phase(dt);
    }
}

void PhysicsCore::resolve_contacts_simd(ContactManifolds& contacts) {
    if (collision_system_) {
        collision_system_->resolve_contacts_simd(contacts);
    }
}

// ========== Force and Impulse ==========

void PhysicsCore::apply_force(EntityID id, const Vec3& force) {
    if (rigid_body_system_->get_body(id)) {
        rigid_body_system_->apply_force(id, force);
    }
}

void PhysicsCore::apply_impulse(EntityID id, const Vec3& impulse, const Vec3& point) {
    if (rigid_body_system_->get_body(id)) {
        rigid_body_system_->apply_impulse(id, impulse, point);
    }
}

void PhysicsCore::apply_torque(EntityID id, const Vec3& torque) {
    if (rigid_body_system_->get_body(id)) {
        rigid_body_system_->apply_torque(id, torque);
    }
}

// ========== Coordinate Systems ==========

EntityID PhysicsCore::create_local_frame(
    const Vec3& position,
    const Mat3x3& orientation,
    double scale,
    EntityID parent_id
) {
    return world_space_.create_local_frame(position, orientation, scale, parent_id);
}

void PhysicsCore::destroy_local_frame(EntityID frame_id) {
    world_space_.destroy_local_frame(frame_id);
}

// ========== Environment ==========

void PhysicsCore::apply_environment_forces(
    PhysicsBody* bodies,
    const GroundInteractionForces* forces,
    const WeatherState& weather,
    size_t count
) {
    if (environment_system_) {
        environment_system_->apply_environment_forces(bodies, forces, weather, count);
    }
}

// ========== Statistics and Debug ==========

PhysicsCore::Stats PhysicsCore::get_stats() const {
    stats_.total_bodies = get_body_count();
    stats_.rigid_bodies = rigid_body_system_->get_body_count();
    stats_.fluid_particles = fluid_system_->get_body_count();
    stats_.gas_particles = gas_system_->get_body_count();
    
    return stats_;
}

size_t PhysicsCore::get_memory_usage() const {
    size_t memory = sizeof(PhysicsCore);
    memory += world_space_.get_memory_usage();
    
    // Rough estimate of subsystems
    memory += rigid_body_system_->get_body_count() * sizeof(PhysicsBody);
    memory += fluid_system_->get_body_count() * sizeof(PhysicsBody);
    memory += gas_system_->get_body_count() * sizeof(PhysicsBody);
    
    return memory;
}

// ========== Private Helpers ==========

void PhysicsCore::update_all_systems(float dt) {
    rigid_body_system_->update(dt);
    if (soft_body_system_) {
        soft_body_system_->update(dt, *collision_system_, *rigid_body_system_);
    }
    fluid_system_->update(dt);
    gas_system_->update(dt);
    
    // Update specialized subsystems
    if (thermal_system_) {
        thermal_system_->update(dt);
    }
    
    if (acoustics_system_) {
        acoustics_system_->update(dt, dt);
    }
    
    // Sync floating origin for large-map stability
    if (hybrid_precision_) {
        hybrid_precision_->apply_pending_shift();
    }
}

void PhysicsCore::detect_and_handle_collisions(float dt) {
    if (!collision_system_) {
        return;
    }

    PHYSICS_PROFILE_SCOPE("BroadPhase");
    collision_system_->update_broad_phase();

    PHYSICS_PROFILE_SCOPE("NarrowPhase");
    collision_system_->solve_narrow_phase(dt);

    ContactManifolds contacts;
    collision_system_->resolve_contacts_simd(contacts);

    if (constraint_solver_) {
        PHYSICS_PROFILE_SCOPE("Constraints");
        constraint_solver_->solve(dt, *rigid_body_system_, contacts);
    }

    if (collision_cb_) {
        for (const auto& manifold : contacts) {
            for (int i = 0; i < manifold.point_count; ++i) {
                const ContactPoint& point = manifold.points[i];
                collision_cb_(point.entity_a, point.entity_b, point.normal);
            }
        }
    }

    for (const auto& manifold : contacts) {
        for (int i = 0; i < manifold.point_count; ++i) {
            const ContactPoint& point = manifold.points[i];
            auto* body_a = get_body(point.entity_a);
            auto* body_b = get_body(point.entity_b);
            float relative_speed = 0.0f;
            if (body_a && body_b) {
                relative_speed = static_cast<float>((body_a->velocity - body_b->velocity).magnitude());
            }
            float impulse = point.penetration * 100.0f + relative_speed * 0.1f;

            common::EventBus::Publish(common::CollisionEvent{
                point.entity_a,
                point.entity_b,
                point.position,
                point.normal,
                impulse,
                relative_speed
            });
        }
    }
}

}  // namespace physics_core
