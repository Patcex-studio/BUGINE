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
#include "physics_body.h"
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace physics_core {
<<<<<<< HEAD
=======
class PhysicsCore;
>>>>>>> c308d63 (Helped the rabbits find a home)

// ============================================================================
// SPH Boundary Particles (for Fluid-Solid Coupling)
// ============================================================================

/**
 * @struct BoundaryParticleSoA
 * @brief Structure of Arrays for boundary particles representing solid surfaces
 * 
 * Boundary particles are static (or kinematically) placed on rigid body surfaces
 * to enable two-way coupling between fluids and solids. They participate in
 * SPH neighbor queries but don't move independently - they follow their parent body.
 * 
 * Memory layout optimized for AVX-512 (64-byte alignment):
 * - Positions updated when bodies move
 * - Normals used for wetting and contact calculations
 * - Links back to parent body for force accumulation
 */
struct alignas(64) BoundaryParticleSoA {
    // Position arrays (updated on body transformation)
    alignas(64) std::vector<float> pos_x;
    alignas(64) std::vector<float> pos_y;
    alignas(64) std::vector<float> pos_z;
    
    // Surface normals (for wetting and reflection)
    alignas(64) std::vector<float> norm_x;
    alignas(64) std::vector<float> norm_y;
    alignas(64) std::vector<float> norm_z;
    
    // Correspondence to rigid body
    alignas(64) std::vector<uint32_t> body_id;          // Parent RigidBody ID
    alignas(64) std::vector<uint32_t> local_index;      // Index in body's local coordinates
    
    // Particle properties
    alignas(64) std::vector<float> mass;                // Typically large for stability
    alignas(64) std::vector<uint8_t> flags;             // Boundary flags
    
    size_t count = 0;
    
    /**
     * Allocate space for boundary particles
     */
    void resize(size_t n) {
        pos_x.resize(n), pos_y.resize(n), pos_z.resize(n);
        norm_x.resize(n), norm_y.resize(n), norm_z.resize(n);
        body_id.resize(n), local_index.resize(n);
        mass.resize(n);
        flags.resize(n);
        count = n;
    }
    
    /**
     * Get particle position at index
     */
    Vec3 get_position(size_t idx) const {
        return Vec3{pos_x[idx], pos_y[idx], pos_z[idx]};
    }
    
    /**
     * Set particle position at index
     */
    void set_position(size_t idx, const Vec3& pos) {
        pos_x[idx] = pos.x;
        pos_y[idx] = pos.y;
        pos_z[idx] = pos.z;
    }
};

/**
 * @class SPHBoundarySystem
 * @brief Manages boundary particles for fluid-solid two-way coupling
 * 
 * Responsibilities:
 * - Generate boundary particles from rigid body meshes (one-time at creation)
 * - Update positions when bodies move/rotate
 * - Insert into SPH spatial grid for neighbor queries
 * - Handle force application back to rigid bodies
 */
class SPHBoundarySystem {
public:
    SPHBoundarySystem(float particle_spacing = 0.01f);
    ~SPHBoundarySystem();
    
    /**
     * Generate boundary particles from a mesh
     * @param body_id ID of the rigid body
     * @param vertices List of mesh vertices (world space)
     * @param particle_spacing Initial spacing between boundary particles
     */
    void generate_from_mesh(
        uint32_t body_id,
        const std::vector<Vec3>& vertices,
        float particle_spacing = 0.01f
    );
    
    /**
     * Update boundary particle positions when body transforms
     * @param body_id ID of the rigid body
     * @param position New world position
     * @param orientation New rotation matrix
     * @param local_vertices Original vertices in body-local coordinates
     */
    void update_positions(
        uint32_t body_id,
        const Vec3& position,
        const Mat3x3& orientation,
        const std::vector<Vec3>& local_vertices
    );
    
    /**
     * Get all boundary particles
     */
    const BoundaryParticleSoA& get_particles() const { return particles_; }
    
    /**
     * Get mutable boundary particles (for modifications)
     */
    BoundaryParticleSoA& get_particles_mut() { return particles_; }
    
    /**
     * Get particles for a specific body
     */
    std::vector<uint32_t> get_particles_for_body(uint32_t body_id) const;
    
    /**
     * Add force to a boundary particle (will be transferred back to body)
     * @param particle_idx Index in boundary particle array
     * @param force Force to apply
     */
    void add_particle_force(uint32_t particle_idx, const Vec3& force);
    
    /**
     * Get accumulated forces for a body and reset buffer
     */
    Vec3 get_and_clear_body_force(uint32_t body_id);
<<<<<<< HEAD
=======

    /**
     * Apply accumulated boundary body forces to the physics core
     */
    void apply_accumulated_body_forces(PhysicsCore& physics);
>>>>>>> c308d63 (Helped the rabbits find a home)
    
    /**
     * Get accumulated impulses for a body and reset buffer
     */
    Vec3 get_and_clear_body_impulse(uint32_t body_id);
    
    /**
     * Total boundary particle count
     */
    size_t get_particle_count() const { return particles_.count; }
    
    /**
     * Check if body has boundary particles
     */
    bool has_particles(uint32_t body_id) const;
    
    /**
     * Clear all boundary particles
     */
    void clear();
    
private:
    BoundaryParticleSoA particles_;
    
    // Per-body force accumulation
    std::unordered_map<uint32_t, Vec3> body_force_accum_;
    std::unordered_map<uint32_t, Vec3> body_impulse_accum_;
    
    // Per-body particle indices
    std::unordered_map<uint32_t, std::vector<uint32_t>> body_particle_indices_;
    
    float particle_spacing_;
};

} // namespace physics_core
