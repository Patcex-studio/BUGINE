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
#include "spatial_hash.h"
#include <vector>
#include <unordered_map>
#include <cstdlib>

namespace physics_core {

// Legacy struct for API compatibility
struct DEMParticle {
    Vec3 position;
    Vec3 velocity;
    Vec3 angular_velocity;
    float radius;
    float inv_mass;
    float shear_modulus;
    Vec3 accumulated_force;
    Vec3 accumulated_torque;
    uint32_t hash_key;
};

/**
 * @struct DEMParticleSoA
 * @brief Structure-of-Arrays format for DEM particles
 * 
 * Memory layout optimized for SIMD and cache efficiency:
 * - Each field in separate aligned(64) array
 * - Batch processing by 8 (AVX2) or 16 (AVX-512)
 * - False sharing prevention via 64-byte alignment
 */
struct DEMParticleSoA {
    static constexpr size_t ALIGNMENT = 64;
    static constexpr size_t BATCH_SIZE = 8;

    // Position components (separate arrays for cache locality)
    alignas(ALIGNMENT) float* pos_x = nullptr;
    alignas(ALIGNMENT) float* pos_y = nullptr;
    alignas(ALIGNMENT) float* pos_z = nullptr;

    // Velocity components
    alignas(ALIGNMENT) float* vel_x = nullptr;
    alignas(ALIGNMENT) float* vel_y = nullptr;
    alignas(ALIGNMENT) float* vel_z = nullptr;

    // Angular velocity components
    alignas(ALIGNMENT) float* ang_vel_x = nullptr;
    alignas(ALIGNMENT) float* ang_vel_y = nullptr;
    alignas(ALIGNMENT) float* ang_vel_z = nullptr;

    // Accumulated forces
    alignas(ALIGNMENT) float* force_x = nullptr;
    alignas(ALIGNMENT) float* force_y = nullptr;
    alignas(ALIGNMENT) float* force_z = nullptr;

    // Accumulated torques
    alignas(ALIGNMENT) float* torque_x = nullptr;
    alignas(ALIGNMENT) float* torque_y = nullptr;
    alignas(ALIGNMENT) float* torque_z = nullptr;

    // Scalar properties (batch-friendly)
    alignas(ALIGNMENT) float* radius = nullptr;
    alignas(ALIGNMENT) float* inv_mass = nullptr;
    alignas(ALIGNMENT) float* shear_modulus = nullptr;

    size_t count = 0;
    size_t capacity = 0;

    DEMParticleSoA() = default;
    ~DEMParticleSoA();

    // Prevent copies, allow moves
    DEMParticleSoA(const DEMParticleSoA&) = delete;
    DEMParticleSoA& operator=(const DEMParticleSoA&) = delete;
    DEMParticleSoA(DEMParticleSoA&&) noexcept;
    DEMParticleSoA& operator=(DEMParticleSoA&&) noexcept;

    void resize(size_t new_count);
    void clear();
    void add_particle(const DEMParticle& particle);

private:
    void allocate_aligned(size_t new_capacity);
    void deallocate_aligned();
};

class DEMSystem {
public:
    DEMSystem();
    ~DEMSystem();

    void add_particle(const DEMParticle& particle);
    void update(float dt);

    // Legacy API - converts on demand
    const std::vector<DEMParticle>& get_particles() const;
    std::vector<DEMParticle>& get_particles();

    // Direct SoA access for performance-critical code
    const DEMParticleSoA& get_particles_soa() const { return particles_soa_; }
    DEMParticleSoA& get_particles_soa() { return particles_soa_; }

    size_t get_particle_count() const { return particles_soa_.count; }

private:
    DEMParticleSoA particles_soa_;
    mutable std::vector<DEMParticle> particles_aos_cache_;  // Lazy conversion for compatibility

    // Параметры материала
    float youngs_modulus_;
    float poisson_ratio_;
    float friction_coeff_;

    // Spatial indexing
    HierarchicalGrid spatial_grid_;
    float cell_size_;

    void build_spatial_hash();
    void compute_contact_forces_simd();
    void integrate_particles_simd(float dt);

    float hertz_normal_force(float overlap, float radius1, float radius2) const;
    
    // SIMD batch processing
    void compute_contact_forces_batch(
        size_t start_idx,
        size_t batch_size,
        const float* pos_x,
        const float* pos_y,
        const float* pos_z,
        const float* radius,
        const float* inv_mass
    );

    // Synchronize SoA cache
    void invalidate_aos_cache() { particles_aos_cache_.clear(); }
};

} // namespace physics_core