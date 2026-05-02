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
#include "physics_core/sph_boundary_system.h"
#include <cmath>
#include <algorithm>

namespace physics_core {

// ============================================================================
// SPHBoundarySystem Implementation
// ============================================================================

SPHBoundarySystem::SPHBoundarySystem(float particle_spacing)
    : particle_spacing_(particle_spacing)
{
}

SPHBoundarySystem::~SPHBoundarySystem() = default;

void SPHBoundarySystem::generate_from_mesh(
    uint32_t body_id,
    const std::vector<Vec3>& vertices,
    float particle_spacing
)
{
    // If body already has particles, skip (regeneration not yet supported)
    if (body_particle_indices_.find(body_id) != body_particle_indices_.end()) {
        return;
    }
    
    // Generate boundary particles from mesh surface
    // Using simple point sampling on mesh vertices (simplified)
    // Full implementation would sample from triangle surfaces
    
    std::vector<uint32_t> new_particle_indices;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        uint32_t particle_idx = particles_.count;
        
        // Resize arrays to accommodate new particle
        particles_.count++;
        particles_.pos_x.push_back(vertices[i].x);
        particles_.pos_y.push_back(vertices[i].y);
        particles_.pos_z.push_back(vertices[i].z);
        
        // Normal is simplified (placeholder - should compute from mesh)
        particles_.norm_x.push_back(1.0f);
        particles_.norm_y.push_back(0.0f);
        particles_.norm_z.push_back(0.0f);
        
        particles_.body_id.push_back(body_id);
        particles_.local_index.push_back(static_cast<uint32_t>(i));
        particles_.mass.push_back(1000.0f);  // Large mass for stability
        particles_.flags.push_back(0);
        
        new_particle_indices.push_back(particle_idx);
    }
    
    // Store mapping from body to its particles
    body_particle_indices_[body_id] = new_particle_indices;
    
    // Initialize force accumulators
    body_force_accum_[body_id] = Vec3{0.0f, 0.0f, 0.0f};
    body_impulse_accum_[body_id] = Vec3{0.0f, 0.0f, 0.0f};
}

void SPHBoundarySystem::update_positions(
    uint32_t body_id,
    const Vec3& position,
    const Mat3x3& orientation,
    const std::vector<Vec3>& local_vertices
)
{
    auto it = body_particle_indices_.find(body_id);
    if (it == body_particle_indices_.end()) {
        return; // Body has no boundary particles
    }
    
    const auto& particle_indices = it->second;
    
    for (size_t i = 0; i < particle_indices.size(); ++i) {
        uint32_t idx = particle_indices[i];
        uint32_t local_idx = particles_.local_index[idx];
        
        if (local_idx < local_vertices.size()) {
            // Transform local vertex to world space
            Vec3 local_pos = local_vertices[local_idx];
            Vec3 world_pos = position + orientation * local_pos;
            
            particles_.pos_x[idx] = world_pos.x;
            particles_.pos_y[idx] = world_pos.y;
            particles_.pos_z[idx] = world_pos.z;
            
            // Transform normal as well
            Vec3 local_normal{particles_.norm_x[idx], particles_.norm_y[idx], particles_.norm_z[idx]};
            Vec3 world_normal = orientation * local_normal;
            
            particles_.norm_x[idx] = world_normal.x;
            particles_.norm_y[idx] = world_normal.y;
            particles_.norm_z[idx] = world_normal.z;
        }
    }
}

std::vector<uint32_t> SPHBoundarySystem::get_particles_for_body(uint32_t body_id) const
{
    auto it = body_particle_indices_.find(body_id);
    if (it != body_particle_indices_.end()) {
        return it->second;
    }
    return {};
}

void SPHBoundarySystem::add_particle_force(uint32_t particle_idx, const Vec3& force)
{
    if (particle_idx >= particles_.count) {
        return;
    }
    
    uint32_t body_id = particles_.body_id[particle_idx];
    body_force_accum_[body_id] += force;
}

Vec3 SPHBoundarySystem::get_and_clear_body_force(uint32_t body_id)
{
    auto it = body_force_accum_.find(body_id);
    if (it != body_force_accum_.end()) {
        Vec3 force = it->second;
        it->second = Vec3{0.0f, 0.0f, 0.0f};
        return force;
    }
    return Vec3{0.0f, 0.0f, 0.0f};
}

Vec3 SPHBoundarySystem::get_and_clear_body_impulse(uint32_t body_id)
{
    auto it = body_impulse_accum_.find(body_id);
    if (it != body_impulse_accum_.end()) {
        Vec3 impulse = it->second;
        it->second = Vec3{0.0f, 0.0f, 0.0f};
        return impulse;
    }
    return Vec3{0.0f, 0.0f, 0.0f};
}

bool SPHBoundarySystem::has_particles(uint32_t body_id) const
{
    return body_particle_indices_.find(body_id) != body_particle_indices_.end();
}

void SPHBoundarySystem::clear()
{
    particles_.count = 0;
    particles_.pos_x.clear();
    particles_.pos_y.clear();
    particles_.pos_z.clear();
    particles_.norm_x.clear();
    particles_.norm_y.clear();
    particles_.norm_z.clear();
    particles_.body_id.clear();
    particles_.local_index.clear();
    particles_.mass.clear();
    particles_.flags.clear();
    
    body_force_accum_.clear();
    body_impulse_accum_.clear();
    body_particle_indices_.clear();
}

} // namespace physics_core
