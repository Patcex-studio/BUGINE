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
#include "physics_core/soft_body_system.h"
#include "physics_core/matter_systems.h"
#include <cmath>
#include <unordered_map>

namespace physics_core {

namespace {

inline uint64_t spatial_hash(int x, int y, int z) {
    const uint64_t p1 = 73856093u;
    const uint64_t p2 = 19349663u;
    const uint64_t p3 = 83492791u;
    return static_cast<uint64_t>(x) * p1 ^ static_cast<uint64_t>(y) * p2 ^ static_cast<uint64_t>(z) * p3;
}

} // namespace

SoftBodySystem::SoftBodySystem()
    : gravity_(0.0, -9.81, 0.0) {
}

SoftBodySystem::~SoftBodySystem() = default;

size_t SoftBodySystem::add_soft_body(SoftBody&& body) {
    if (body.positions.size() != body.prev_positions.size()) {
        body.prev_positions = body.positions;
    }
    if (body.inv_masses.size() != body.positions.size()) {
        body.inv_masses.assign(body.positions.size(), 1.0f);
    }
    if (body.pinned.size() != body.positions.size()) {
        body.pinned.assign(body.positions.size(), 0);
    }
    soft_bodies_.push_back(std::move(body));
    return soft_bodies_.size() - 1;
}

size_t SoftBodySystem::create_rope(
    uint32_t particle_count,
    float length,
    float particle_radius,
    const Vec3& start,
    const Vec3& direction,
    bool pin_start,
    physics_core::EntityID owner_entity
) {
    SoftBody body;
    body.owner_entity = owner_entity;
    body.positions.resize(particle_count);
    body.prev_positions.resize(particle_count);
    body.inv_masses.assign(particle_count, 1.0f);
    body.pinned.assign(particle_count, 0);
    body.particle_radius = particle_radius;

    Vec3 step = direction.normalized() * (length / static_cast<float>(particle_count - 1));
    for (uint32_t i = 0; i < particle_count; ++i) {
        body.positions[i] = start + step * static_cast<double>(i);
        body.prev_positions[i] = body.positions[i];
    }

    if (pin_start && particle_count > 0) {
        body.inv_masses[0] = 0.0f;
        body.pinned[0] = 1;
    }

    for (uint32_t i = 0; i + 1 < particle_count; ++i) {
        Vec3 delta = body.positions[i + 1] - body.positions[i];
        body.constraints.push_back({i, i + 1, static_cast<float>(delta.magnitude()), 1.0f});
    }

    for (uint32_t i = 0; i + 2 < particle_count; ++i) {
        Vec3 delta = body.positions[i + 2] - body.positions[i];
        body.constraints.push_back({i, i + 2, static_cast<float>(delta.magnitude()), 0.15f});
    }

    return add_soft_body(std::move(body));
}

size_t SoftBodySystem::create_cloth(
    uint32_t width,
    uint32_t height,
    float spacing,
    const Vec3& origin,
    bool pin_corners,
    physics_core::EntityID owner_entity
) {
    SoftBody body;
    body.owner_entity = owner_entity;
    uint32_t total = width * height;
    body.positions.resize(total);
    body.prev_positions.resize(total);
    body.inv_masses.assign(total, 1.0f);
    body.pinned.assign(total, 0);
    body.particle_radius = spacing * 0.5f;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t index = y * width + x;
            body.positions[index] = origin + Vec3(x * spacing, 0.0, y * spacing);
            body.prev_positions[index] = body.positions[index];
        }
    }

    if (pin_corners && total > 0) {
        body.pinned[0] = 1;
        body.inv_masses[0] = 0.0f;
        body.pinned[width - 1] = 1;
        body.inv_masses[width - 1] = 0.0f;
        body.pinned[total - width] = 1;
        body.inv_masses[total - width] = 0.0f;
        body.pinned[total - 1] = 1;
        body.inv_masses[total - 1] = 0.0f;
    }

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t index = y * width + x;
            if (x + 1 < width) {
                uint32_t right = index + 1;
                body.constraints.push_back({index, right, spacing, 1.0f});
            }
            if (y + 1 < height) {
                uint32_t down = index + width;
                body.constraints.push_back({index, down, spacing, 1.0f});
            }
            if (x + 1 < width && y + 1 < height) {
                uint32_t diag = index + width + 1;
                body.constraints.push_back({index, diag, spacing * std::sqrt(2.0f), 0.75f});
            }
            if (x > 0 && y + 1 < height) {
                uint32_t diag = index + width - 1;
                body.constraints.push_back({index, diag, spacing * std::sqrt(2.0f), 0.75f});
            }
        }
    }

    return add_soft_body(std::move(body));
}

void SoftBodySystem::update(float dt, CollisionSystem& collision_system, RigidBodySystem& rigid_body_system) {
    for (SoftBody& body : soft_bodies_) {
        if (body.positions.empty()) {
            continue;
        }

        for (size_t i = 0; i < body.positions.size(); ++i) {
            if (body.pinned[i]) {
                body.prev_positions[i] = body.positions[i];
                continue;
            }

            Vec3 velocity = body.positions[i] - body.prev_positions[i];
            body.prev_positions[i] = body.positions[i];
            body.positions[i] = body.positions[i] + velocity + gravity_ * (dt * dt);
        }

        solve_constraints(body);
        enforce_self_collisions(body);
        resolve_collisions(body, collision_system, rigid_body_system);
    }
}

void SoftBodySystem::solve_constraints(SoftBody& body) {
    constexpr int kIterations = 8;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        for (const SoftConstraint& constraint : body.constraints) {
            Vec3& p0 = body.positions[constraint.vertex_a];
            Vec3& p1 = body.positions[constraint.vertex_b];
            float inv0 = body.inv_masses[constraint.vertex_a];
            float inv1 = body.inv_masses[constraint.vertex_b];
            float total_inv = inv0 + inv1;
            if (total_inv <= 0.0f) {
                continue;
            }

            Vec3 delta = p1 - p0;
            float dist = static_cast<float>(delta.magnitude());
            if (dist < 1e-6f) {
                continue;
            }

            float difference = (dist - constraint.rest_length) / dist;
            Vec3 correction = delta * (difference * constraint.stiffness);
            if (inv0 > 0.0f) {
                p0 = p0 + correction * (inv0 / total_inv);
            }
            if (inv1 > 0.0f) {
                p1 = p1 - correction * (inv1 / total_inv);
            }
        }

        for (size_t i = 0; i < body.positions.size(); ++i) {
            if (body.pinned[i]) {
                body.positions[i] = body.prev_positions[i];
            }
        }
    }
}

void SoftBodySystem::enforce_self_collisions(SoftBody& body) {
    if (body.positions.size() < 8) {
        return;
    }

    float cell_size = body.particle_radius * 2.0f;
    std::unordered_map<uint64_t, std::vector<size_t>> grid;
    grid.reserve(body.positions.size() * 2);

    for (size_t i = 0; i < body.positions.size(); ++i) {
        const Vec3& position = body.positions[i];
        int gx = static_cast<int>(std::floor(position.x / cell_size));
        int gy = static_cast<int>(std::floor(position.y / cell_size));
        int gz = static_cast<int>(std::floor(position.z / cell_size));
        grid[spatial_hash(gx, gy, gz)].push_back(i);
    }

    auto query_neighbors = [&](const Vec3& position, float radius, std::vector<size_t>& result) {
        result.clear();
        int xmin = static_cast<int>(std::floor((position.x - radius) / cell_size));
        int xmax = static_cast<int>(std::floor((position.x + radius) / cell_size));
        int ymin = static_cast<int>(std::floor((position.y - radius) / cell_size));
        int ymax = static_cast<int>(std::floor((position.y + radius) / cell_size));
        int zmin = static_cast<int>(std::floor((position.z - radius) / cell_size));
        int zmax = static_cast<int>(std::floor((position.z + radius) / cell_size));

        for (int x = xmin; x <= xmax; ++x) {
            for (int y = ymin; y <= ymax; ++y) {
                for (int z = zmin; z <= zmax; ++z) {
                    auto it = grid.find(spatial_hash(x, y, z));
                    if (it == grid.end()) {
                        continue;
                    }
                    for (size_t index : it->second) {
                        result.push_back(index);
                    }
                }
            }
        }
    };

    std::vector<size_t> neighbors;
    neighbors.reserve(16);
    float min_dist = body.particle_radius * 0.9f;

    for (size_t i = 0; i < body.positions.size(); ++i) {
        const Vec3& p0 = body.positions[i];
        if (body.pinned[i]) {
            continue;
        }

        query_neighbors(p0, min_dist * 1.5f, neighbors);
        for (size_t j : neighbors) {
            if (j <= i) {
                continue;
            }
            if (body.pinned[j]) {
                continue;
            }

            Vec3 delta = body.positions[j] - p0;
            float distance = static_cast<float>(delta.magnitude());
            if (distance < 1e-6f || distance >= min_dist) {
                continue;
            }

            float penetration = min_dist - distance;
            Vec3 normal = delta / distance;
            float inv0 = body.inv_masses[i];
            float inv1 = body.inv_masses[j];
            float total_inv = inv0 + inv1;
            if (total_inv <= 0.0f) {
                continue;
            }

            Vec3 correction = normal * (penetration * 0.5f);
            if (inv0 > 0.0f) {
                body.positions[i] = body.positions[i] - correction * (inv0 / total_inv);
            }
            if (inv1 > 0.0f) {
                body.positions[j] = body.positions[j] + correction * (inv1 / total_inv);
            }
        }
    }
}

void SoftBodySystem::resolve_collisions(SoftBody& body, CollisionSystem& collision_system, RigidBodySystem& rigid_body_system) {
    std::vector<EntityID> candidates;
    candidates.reserve(16);

    for (size_t vertex_index = 0; vertex_index < body.positions.size(); ++vertex_index) {
        const Vec3& position = body.positions[vertex_index];
        collision_system.sphere_query_simd(position, body.particle_radius, candidates);
        for (EntityID entity_id : candidates) {
            const CollisionShape* shape = collision_system.find_shape(entity_id);
            if (shape) {
                enforce_collision_with_shape(body, vertex_index, *shape);
            }
        }
    }
}

void SoftBodySystem::enforce_collision_with_shape(SoftBody& body, size_t vertex_index, const CollisionShape& shape) {
    Vec3& position = body.positions[vertex_index];
    if (shape.type == CollisionShapeType::Sphere) {
        Vec3 delta = position - shape.center;
        float distance = static_cast<float>(delta.magnitude());
        float radius_sum = shape.radius + body.particle_radius;
        if (distance < radius_sum && distance > 1e-6f) {
            Vec3 normal = delta / distance;
            position = shape.center + normal * radius_sum;
        }
    } else if (shape.type == CollisionShapeType::Box) {
        Vec3 relative = position - shape.center;
        Vec3 clamped = relative;
        clamped.x = std::max(-shape.half_extents.x, std::min(shape.half_extents.x, clamped.x));
        clamped.y = std::max(-shape.half_extents.y, std::min(shape.half_extents.y, clamped.y));
        clamped.z = std::max(-shape.half_extents.z, std::min(shape.half_extents.z, clamped.z));

        if (clamped.x == relative.x && clamped.y == relative.y && clamped.z == relative.z) {
            Vec3 delta = position - shape.center;
            Vec3 abs_delta = Vec3(std::abs(delta.x), std::abs(delta.y), std::abs(delta.z));
            if (abs_delta.x <= shape.half_extents.x && abs_delta.y <= shape.half_extents.y && abs_delta.z <= shape.half_extents.z) {
                if (abs_delta.x <= abs_delta.y && abs_delta.x <= abs_delta.z) {
                    position.x = shape.center.x + (delta.x > 0.0 ? shape.half_extents.x + body.particle_radius : -shape.half_extents.x - body.particle_radius);
                } else if (abs_delta.y <= abs_delta.z) {
                    position.y = shape.center.y + (delta.y > 0.0 ? shape.half_extents.y + body.particle_radius : -shape.half_extents.y - body.particle_radius);
                } else {
                    position.z = shape.center.z + (delta.z > 0.0 ? shape.half_extents.z + body.particle_radius : -shape.half_extents.z - body.particle_radius);
                }
            }
        }
    }
}

} // namespace physics_core
