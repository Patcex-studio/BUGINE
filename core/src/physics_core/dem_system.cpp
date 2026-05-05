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
#include "physics_core/dem_system.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <immintrin.h>

namespace physics_core {

// ============================================================================
// DEMParticleSoA Implementation
// ============================================================================

DEMParticleSoA::~DEMParticleSoA() {
    deallocate_aligned();
}

DEMParticleSoA::DEMParticleSoA(DEMParticleSoA&& other) noexcept
    : pos_x(other.pos_x), pos_y(other.pos_y), pos_z(other.pos_z),
      vel_x(other.vel_x), vel_y(other.vel_y), vel_z(other.vel_z),
      ang_vel_x(other.ang_vel_x), ang_vel_y(other.ang_vel_y), ang_vel_z(other.ang_vel_z),
      force_x(other.force_x), force_y(other.force_y), force_z(other.force_z),
      torque_x(other.torque_x), torque_y(other.torque_y), torque_z(other.torque_z),
      radius(other.radius), inv_mass(other.inv_mass), shear_modulus(other.shear_modulus),
      count(other.count), capacity(other.capacity)
{
    other.pos_x = nullptr;
    other.pos_y = nullptr;
    other.pos_z = nullptr;
    other.vel_x = nullptr;
    other.vel_y = nullptr;
    other.vel_z = nullptr;
    other.ang_vel_x = nullptr;
    other.ang_vel_y = nullptr;
    other.ang_vel_z = nullptr;
    other.force_x = nullptr;
    other.force_y = nullptr;
    other.force_z = nullptr;
    other.torque_x = nullptr;
    other.torque_y = nullptr;
    other.torque_z = nullptr;
    other.radius = nullptr;
    other.inv_mass = nullptr;
    other.shear_modulus = nullptr;
    other.count = 0;
    other.capacity = 0;
}

DEMParticleSoA& DEMParticleSoA::operator=(DEMParticleSoA&& other) noexcept {
    if (this != &other) {
        deallocate_aligned();
        pos_x = other.pos_x;
        pos_y = other.pos_y;
        pos_z = other.pos_z;
        vel_x = other.vel_x;
        vel_y = other.vel_y;
        vel_z = other.vel_z;
        ang_vel_x = other.ang_vel_x;
        ang_vel_y = other.ang_vel_y;
        ang_vel_z = other.ang_vel_z;
        force_x = other.force_x;
        force_y = other.force_y;
        force_z = other.force_z;
        torque_x = other.torque_x;
        torque_y = other.torque_y;
        torque_z = other.torque_z;
        radius = other.radius;
        inv_mass = other.inv_mass;
        shear_modulus = other.shear_modulus;
        count = other.count;
        capacity = other.capacity;

        other.pos_x = nullptr;
        other.pos_y = nullptr;
        other.pos_z = nullptr;
        other.vel_x = nullptr;
        other.vel_y = nullptr;
        other.vel_z = nullptr;
        other.ang_vel_x = nullptr;
        other.ang_vel_y = nullptr;
        other.ang_vel_z = nullptr;
        other.force_x = nullptr;
        other.force_y = nullptr;
        other.force_z = nullptr;
        other.torque_x = nullptr;
        other.torque_y = nullptr;
        other.torque_z = nullptr;
        other.radius = nullptr;
        other.inv_mass = nullptr;
        other.shear_modulus = nullptr;
        other.count = 0;
        other.capacity = 0;
    }
    return *this;
}

void DEMParticleSoA::allocate_aligned(size_t new_capacity) {
    deallocate_aligned();
    capacity = new_capacity;
    
    // Allocate 18 float arrays (3 components × 6 vector fields)
    pos_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    pos_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    pos_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    
    vel_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    vel_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    vel_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    
    ang_vel_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    ang_vel_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    ang_vel_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    
    force_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    force_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    force_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    
    torque_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    torque_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    torque_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    
    radius = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    inv_mass = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    shear_modulus = static_cast<float*>(std::aligned_alloc(ALIGNMENT, capacity * sizeof(float)));
    size_t size = capacity * sizeof(float);
    size_t aligned_size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    
    // Allocate 18 float arrays (3 components × 6 vector fields)
    pos_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    pos_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    pos_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    
    vel_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    vel_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    vel_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    
    ang_vel_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    ang_vel_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    ang_vel_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    
    force_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    force_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    force_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    
    torque_x = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    torque_y = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    torque_z = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    
    radius = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    inv_mass = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
    shear_modulus = static_cast<float*>(std::aligned_alloc(ALIGNMENT, aligned_size));
}

void DEMParticleSoA::deallocate_aligned() {
    std::free(pos_x);
    std::free(pos_y);
    std::free(pos_z);
    std::free(vel_x);
    std::free(vel_y);
    std::free(vel_z);
    std::free(ang_vel_x);
    std::free(ang_vel_y);
    std::free(ang_vel_z);
    std::free(force_x);
    std::free(force_y);
    std::free(force_z);
    std::free(torque_x);
    std::free(torque_y);
    std::free(torque_z);
    std::free(radius);
    std::free(inv_mass);
    std::free(shear_modulus);
    
    pos_x = pos_y = pos_z = nullptr;
    vel_x = vel_y = vel_z = nullptr;
    ang_vel_x = ang_vel_y = ang_vel_z = nullptr;
    force_x = force_y = force_z = nullptr;
    torque_x = torque_y = torque_z = nullptr;
    radius = inv_mass = shear_modulus = nullptr;
    capacity = 0;
}

void DEMParticleSoA::resize(size_t new_count) {
    if (new_count > capacity) {
        size_t new_capacity = (new_count + BATCH_SIZE - 1) & ~(BATCH_SIZE - 1);
        allocate_aligned(new_capacity);
    }
    count = new_count;
}

void DEMParticleSoA::clear() {
    count = 0;
}

void DEMParticleSoA::add_particle(const DEMParticle& particle) {
    if (count >= capacity) {
        size_t new_capacity = capacity > 0 ? capacity * 2 : BATCH_SIZE;
        allocate_aligned(new_capacity);
    }
    
    pos_x[count] = particle.position.x;
    pos_y[count] = particle.position.y;
    pos_z[count] = particle.position.z;
    
    vel_x[count] = particle.velocity.x;
    vel_y[count] = particle.velocity.y;
    vel_z[count] = particle.velocity.z;
    
    ang_vel_x[count] = particle.angular_velocity.x;
    ang_vel_y[count] = particle.angular_velocity.y;
    ang_vel_z[count] = particle.angular_velocity.z;
    
    force_x[count] = 0.0f;
    force_y[count] = 0.0f;
    force_z[count] = 0.0f;
    
    torque_x[count] = 0.0f;
    torque_y[count] = 0.0f;
    torque_z[count] = 0.0f;
    
    radius[count] = particle.radius;
    inv_mass[count] = particle.inv_mass;
    shear_modulus[count] = particle.shear_modulus;
    
    ++count;
}

// ============================================================================
// DEMSystem Implementation
// ============================================================================

DEMSystem::DEMSystem()
    : youngs_modulus_(1e7f),
      poisson_ratio_(0.3f),
      friction_coeff_(0.5f),
      cell_size_(0.0f) {
}

DEMSystem::~DEMSystem() = default;

void DEMSystem::add_particle(const DEMParticle& particle) {
    particles_soa_.add_particle(particle);
    invalidate_aos_cache();
}

const std::vector<DEMParticle>& DEMSystem::get_particles() const {
    if (particles_aos_cache_.empty() && particles_soa_.count > 0) {
        particles_aos_cache_.resize(particles_soa_.count);
        for (size_t i = 0; i < particles_soa_.count; ++i) {
            particles_aos_cache_[i].position = Vec3(
                particles_soa_.pos_x[i],
                particles_soa_.pos_y[i],
                particles_soa_.pos_z[i]
            );
            particles_aos_cache_[i].velocity = Vec3(
                particles_soa_.vel_x[i],
                particles_soa_.vel_y[i],
                particles_soa_.vel_z[i]
            );
            particles_aos_cache_[i].angular_velocity = Vec3(
                particles_soa_.ang_vel_x[i],
                particles_soa_.ang_vel_y[i],
                particles_soa_.ang_vel_z[i]
            );
            particles_aos_cache_[i].accumulated_force = Vec3(
                particles_soa_.force_x[i],
                particles_soa_.force_y[i],
                particles_soa_.force_z[i]
            );
            particles_aos_cache_[i].accumulated_torque = Vec3(
                particles_soa_.torque_x[i],
                particles_soa_.torque_y[i],
                particles_soa_.torque_z[i]
            );
            particles_aos_cache_[i].radius = particles_soa_.radius[i];
            particles_aos_cache_[i].inv_mass = particles_soa_.inv_mass[i];
            particles_aos_cache_[i].shear_modulus = particles_soa_.shear_modulus[i];
        }
    }
    return particles_aos_cache_;
}

std::vector<DEMParticle>& DEMSystem::get_particles() {
    if (particles_aos_cache_.empty() && particles_soa_.count > 0) {
        const_cast<DEMSystem*>(this)->get_particles();
    }
    return particles_aos_cache_;
}

void DEMSystem::update(float dt) {
    if (particles_soa_.count == 0) return;

    // Clear forces
    for (size_t i = 0; i < particles_soa_.count; ++i) {
        particles_soa_.force_x[i] = 0.0f;
        particles_soa_.force_y[i] = 0.0f;
        particles_soa_.force_z[i] = 0.0f;
        particles_soa_.torque_x[i] = 0.0f;
        particles_soa_.torque_y[i] = 0.0f;
        particles_soa_.torque_z[i] = 0.0f;
    }

    build_spatial_hash();
    compute_contact_forces_simd();
    integrate_particles_simd(dt);
    invalidate_aos_cache();
}

void DEMSystem::build_spatial_hash() {
    spatial_grid_.build(
        particles_soa_.pos_x,
        particles_soa_.pos_y,
        particles_soa_.pos_z,
        particles_soa_.count
    );
}

float DEMSystem::hertz_normal_force(float overlap, float radius1, float radius2) const {
    float E_star = youngs_modulus_;
    float r_star = (radius1 * radius2) / (radius1 + radius2 + 1e-6f);
    return (4.0f / 3.0f) * E_star * std::sqrt(r_star) * std::pow(overlap, 1.5f);
}

void DEMSystem::compute_contact_forces_simd() {
    for (size_t i = 0; i < particles_soa_.count; ++i) {
        Vec3 pos_i(particles_soa_.pos_x[i], particles_soa_.pos_y[i], particles_soa_.pos_z[i]);
        float rad_i = particles_soa_.radius[i];

        // Query neighbors using hierarchical grid
        spatial_grid_.query_radius(pos_i.x, pos_i.y, pos_i.z, rad_i * 2.0f, 
            [&](uint32_t j) {
                if (j <= i) return;  // Skip self and avoid duplicates

                Vec3 pos_j(particles_soa_.pos_x[j], particles_soa_.pos_y[j], particles_soa_.pos_z[j]);
                Vec3 delta = pos_j - pos_i;
                float dist_sq = delta.dot(delta);
                float rad_j = particles_soa_.radius[j];
                float rad_sum = rad_i + rad_j;

                if (dist_sq < rad_sum * rad_sum && dist_sq > 1e-6f) {
                    float dist = std::sqrt(dist_sq);
                    float overlap = rad_sum - dist;

                    float fn_mag = hertz_normal_force(overlap, rad_i, rad_j);
                    Vec3 fn = (delta / dist) * fn_mag;

                    Vec3 vel_i(particles_soa_.vel_x[i], particles_soa_.vel_y[i], particles_soa_.vel_z[i]);
                    Vec3 vel_j(particles_soa_.vel_x[j], particles_soa_.vel_y[j], particles_soa_.vel_z[j]);
                    Vec3 rel_vel = vel_j - vel_i;
                    float rel_speed = rel_vel.length();
                    float rel_speed = rel_vel.magnitude();
                    Vec3 ft = Vec3(0.0f, 0.0f, 0.0f);
                    if (rel_speed > 1e-6f) {
                        float ft_max = friction_coeff_ * fn_mag;
                        float ft_mag = std::min(rel_speed * 50.0f, ft_max);
                        ft = (rel_vel / rel_speed) * ft_mag;
                    }

                    particles_soa_.force_x[i] += fn.x + ft.x;
                    particles_soa_.force_y[i] += fn.y + ft.y;
                    particles_soa_.force_z[i] += fn.z + ft.z;

                    particles_soa_.force_x[j] -= fn.x + ft.x;
                    particles_soa_.force_y[j] -= fn.y + ft.y;
                    particles_soa_.force_z[j] -= fn.z + ft.z;

                    Vec3 torque_arm = delta * 0.5f;
                    Vec3 torque_contrib = torque_arm.cross(ft);
                    particles_soa_.torque_x[i] += torque_contrib.x;
                    particles_soa_.torque_y[i] += torque_contrib.y;
                    particles_soa_.torque_z[i] += torque_contrib.z;

                    particles_soa_.torque_x[j] -= torque_contrib.x;
                    particles_soa_.torque_y[j] -= torque_contrib.y;
                    particles_soa_.torque_z[j] -= torque_contrib.z;
                }
            });
    }
}

void DEMSystem::integrate_particles_simd(float dt) {
    const float gravity = 9.81f;

    for (size_t i = 0; i < particles_soa_.count; ++i) {
        float inv_mass = particles_soa_.inv_mass[i];
        if (inv_mass <= 0.0f) continue;

        particles_soa_.force_y[i] -= gravity / inv_mass;
        particles_soa_.force_y[i] -= gravity;

        particles_soa_.vel_x[i] += particles_soa_.force_x[i] * inv_mass * dt;
        particles_soa_.vel_y[i] += particles_soa_.force_y[i] * inv_mass * dt;
        particles_soa_.vel_z[i] += particles_soa_.force_z[i] * inv_mass * dt;

        particles_soa_.pos_x[i] += particles_soa_.vel_x[i] * dt;
        particles_soa_.pos_y[i] += particles_soa_.vel_y[i] * dt;
        particles_soa_.pos_z[i] += particles_soa_.vel_z[i] * dt;

        particles_soa_.ang_vel_x[i] += particles_soa_.torque_x[i] * inv_mass * dt;
        particles_soa_.ang_vel_y[i] += particles_soa_.torque_y[i] * inv_mass * dt;
        particles_soa_.ang_vel_z[i] += particles_soa_.torque_z[i] * inv_mass * dt;
    }
}

} // namespace physics_core