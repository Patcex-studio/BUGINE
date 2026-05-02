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

#include "physics_body.h"
#include "physics_container.h"
#include "simd_processor.h"
#include "sph_system.h"
#include <vector>
#include <memory>

namespace physics_core {

// ============================================================================
// Matter Subsystem Base
// ============================================================================

/**
 * @class MatterSubsystem
 * @brief Abstract base class for matter subsystems
 */
class MatterSubsystem {
public:
    virtual ~MatterSubsystem() = default;

    /**
     * Update subsystem physics
     * @param dt Time step in seconds
     */
    virtual void update(float dt) = 0;

    /**
     * Add body to subsystem
     */
    virtual EntityID add_body(const PhysicsBody& body) = 0;

    /**
     * Remove body from subsystem
     */
    virtual void remove_body(EntityID id) = 0;

    /**
     * Get body by ID
     */
    virtual PhysicsBody* get_body(EntityID id) = 0;

    /**
     * Get number of bodies in subsystem
     */
    virtual size_t get_body_count() const = 0;

    /**
     * Get all bodies
     */
    virtual const std::vector<PhysicsBody>& get_all_bodies() const = 0;

    /**
     * Get subsystem name
     */
    virtual const char* name() const = 0;
};

// ============================================================================
// Rigid Body System
// ============================================================================

/**
 * @class RigidBodySystem
 * @brief Manages rigid body physics
 * 
 * Criteria:
 * - Properties: mass, inertia, friction, restitution
 * - Interaction: momentum transfer, friction
 * - Universal interface for access
 */
class RigidBodySystem : public MatterSubsystem {
public:
    RigidBodySystem();
    ~RigidBodySystem();

    void update(float dt) override;
    EntityID add_body(const PhysicsBody& body) override;
    void remove_body(EntityID id) override;
    PhysicsBody* get_body(EntityID id) override;
    size_t get_body_count() const override { return bodies_.get_entity_count(); }
    const std::vector<PhysicsBody>& get_all_bodies() const override { return bodies_.get_all_bodies(); }
    const char* name() const override { return "RigidBodySystem"; }

    // ========== Rigid Body Specific Methods ==========

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
     * @param point Point of application (world space)
     */
    void apply_impulse(EntityID id, const Vec3& impulse, const Vec3& point);

    /**
     * Set gravity for all bodies
     * @param gravity Acceleration due to gravity
     */
    void set_gravity(const Vec3& gravity);

    /**
     * Get gravity
     */
    Vec3 get_gravity() const { return gravity_; }

    /**
     * Enable/disable gravity for specific body
     */
    void set_gravity_enabled(EntityID id, bool enabled);

private:
    PhysicsContainer bodies_;
    
    // SoA force arrays for SIMD processing
    alignas(64) std::vector<float> force_x_;
    alignas(64) std::vector<float> force_y_;
    alignas(64) std::vector<float> force_z_;
    alignas(64) std::vector<float> torque_x_;
    alignas(64) std::vector<float> torque_y_;
    alignas(64) std::vector<float> torque_z_;
    
    Vec3 gravity_;

    std::unique_ptr<SIMDProcessor> simd_processor_;

    void update_forces_and_torques(float dt);
    void integrate_motion(float dt);
};

// ============================================================================
// Fluid System
// ============================================================================

/**
 * @class FluidSystem
 * @brief Manages fluid particle physics using SPH
 * 
 * Criteria:
 * - Properties: density, viscosity, pressure
 * - Interaction: pressure forces, viscous damping
 * - Incompressibility constraints
 * - SPH simulation with grid-based optimization
 */
class FluidSystem : public MatterSubsystem {
public:
    FluidSystem();
    ~FluidSystem();

    void update(float dt) override;
    EntityID add_body(const PhysicsBody& body) override;
    void remove_body(EntityID id) override;
    PhysicsBody* get_body(EntityID id) override;
    size_t get_body_count() const override { return sph_system_->get_particles().size(); }
    const std::vector<PhysicsBody>& get_all_bodies() const override { return dummy_bodies_; }
    const char* name() const override { return "FluidSystem"; }

    // ========== Fluid Specific Methods ==========

    /**
     * Initialize fluid with positions
     */
    void initialize_fluid(FluidType type, const std::vector<std::array<float, 3>>& positions);

    /**
     * Set viscosity coefficient
     */
    void set_viscosity(float viscosity) {
        viscosity_ = viscosity;
        if (sph_system_) {
            sph_system_->set_fluid_properties(current_fluid_type_, sph_system_->get_particles().empty() ? 1000.0f : sph_system_->get_particles()[0].density, viscosity_);
        }
    }

    /**
     * Set surface tension
     */
    void set_surface_tension(float tension) {
        surface_tension_ = tension;
        // SPH system handles surface tension implicitly through particle interactions.
    }

    /**
     * Get average density
     */
    float get_average_density() const;

    /**
     * Get SPH particles (for rendering/integration)
     */
    const std::vector<SPHParticle>& get_sph_particles() const;

private:
    std::unique_ptr<SPHSystem> sph_system_;
    FluidType current_fluid_type_ = FluidType::WATER;
    float viscosity_;
    float surface_tension_;
    std::vector<PhysicsBody> dummy_bodies_; // For compatibility with MatterSubsystem interface
};

// ============================================================================
// Gas System
// ============================================================================

/**
 * @class GasSystem
 * @brief Manages gas particle physics
 * 
 * Criteria:
 * - Properties: compressibility, temperature, diffusion
 * - Interaction: pressure, temperature gradients
 * - Equation of state
 */
class GasSystem : public MatterSubsystem {
public:
    GasSystem();
    ~GasSystem();

    void update(float dt) override;
    EntityID add_body(const PhysicsBody& body) override;
    void remove_body(EntityID id) override;
    PhysicsBody* get_body(EntityID id) override;
    size_t get_body_count() const override { return particles_.get_entity_count(); }
    const std::vector<PhysicsBody>& get_all_bodies() const override { return particles_.get_all_bodies(); }
    const char* name() const override { return "GasSystem"; }

    // ========== Gas Specific Methods ==========

    /**
     * Set temperature for all particles
     */
    void set_temperature(float temperature) { temperature_ = temperature; }

    /**
     * Set diffusion coefficient
     */
    void set_diffusion(float diffusion) { diffusion_ = diffusion; }

    /**
     * Get average temperature
     */
    float get_average_temperature() const { return temperature_; }

private:
    PhysicsContainer particles_;
    float temperature_;
    float diffusion_;
    float compressibility_;

    std::vector<double> pressure_cache_;
    std::vector<double> temperature_cache_;
    std::vector<Vec3> force_cache_;

    std::unique_ptr<SIMDProcessor> simd_processor_;

    void compute_pressures();
    void compute_expansion_forces();
    void compute_diffusion(float dt);
    void integrate_particles(float dt);
};

}  // namespace physics_core
