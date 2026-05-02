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
#include "physics_core/matter_systems.h"
#include <algorithm>
#include <cmath>

namespace physics_core {

namespace {
    static constexpr double kGasConstant = 287.05; // J/(kg·K), approximate for air
    static constexpr double kNeighborRadius = 2.0; // meters
    static constexpr double kMinDensity = 0.001;

    inline double pressure_kernel(double dist_sq) {
        double radius_sq = kNeighborRadius * kNeighborRadius;
        if (dist_sq >= radius_sq) {
            return 0.0;
        }
        double dist = std::sqrt(dist_sq);
        double factor = (kNeighborRadius - dist) / kNeighborRadius;
        return factor * factor;
    }
}

GasSystem::GasSystem()
    : particles_(1024),
      temperature_(300.0f),
      diffusion_(0.1f),
      compressibility_(1.0f),
      simd_processor_(std::make_unique<SIMDProcessor>()) {
}

GasSystem::~GasSystem() = default;

void GasSystem::update(float dt) {
    const auto& bodies = particles_.get_all_bodies();
    const size_t count = bodies.size();
    if (count == 0 || dt <= 0.0f) {
        return;
    }

    pressure_cache_.assign(count, 0.0);
    temperature_cache_.assign(count, temperature_);
    force_cache_.assign(count, Vec3(0.0, 0.0, 0.0));

    compute_pressures();
    compute_expansion_forces();
    compute_diffusion(dt);
    integrate_particles(dt);
}

EntityID GasSystem::add_body(const PhysicsBody& body) {
    return particles_.register_entity(body);
}

void GasSystem::remove_body(EntityID id) {
    particles_.remove_entity(id);
}

PhysicsBody* GasSystem::get_body(EntityID id) {
    return particles_.get_body(id);
}

void GasSystem::compute_pressures() {
    const auto& bodies = particles_.get_all_bodies();
    const size_t count = bodies.size();

    for (size_t i = 0; i < count; ++i) {
        const PhysicsBody& body = bodies[i];
        double local_density = 0.0;

        for (size_t j = 0; j < count; ++j) {
            const PhysicsBody& neighbor = bodies[j];
            Vec3 delta = neighbor.position - body.position;
            double dist_sq = delta.dot(delta);
            local_density += neighbor.mass * pressure_kernel(dist_sq);
        }

        local_density = std::max(local_density, kMinDensity);
        pressure_cache_[i] = (local_density * kGasConstant * temperature_cache_[i]) / std::max<double>(compressibility_, 1e-3);
    }
}

void GasSystem::compute_expansion_forces() {
    const auto& bodies = particles_.get_all_bodies();
    const size_t count = bodies.size();

    for (size_t i = 0; i < count; ++i) {
        Vec3 force(0.0, 0.0, 0.0);
        const PhysicsBody& body = bodies[i];

        for (size_t j = 0; j < count; ++j) {
            if (i == j) {
                continue;
            }

            const PhysicsBody& neighbor = bodies[j];
            Vec3 delta = neighbor.position - body.position;
            double dist_sq = delta.dot(delta);
            if (dist_sq > kNeighborRadius * kNeighborRadius || dist_sq < 1e-6) {
                continue;
            }

            double dist = std::sqrt(dist_sq);
            Vec3 direction = delta / dist;
            double pressure_diff = pressure_cache_[i] - pressure_cache_[j];
            double magnitude = pressure_diff * 0.5 / (dist + 1e-3);
            force = force + direction * magnitude;
        }

        force_cache_[i] = force;
    }
}

void GasSystem::compute_diffusion(float dt) {
    const auto& bodies = particles_.get_all_bodies();
    const size_t count = bodies.size();
    std::vector<double> temperature_delta(count, 0.0);

    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            Vec3 delta = bodies[j].position - bodies[i].position;
            double dist_sq = delta.dot(delta);
            if (dist_sq > kNeighborRadius * kNeighborRadius) {
                continue;
            }
            double diff = temperature_cache_[j] - temperature_cache_[i];
            double transfer = diff * diffusion_ * dt * 0.1;
            temperature_delta[i] += transfer;
            temperature_delta[j] -= transfer;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        temperature_cache_[i] = std::max(0.0, temperature_cache_[i] + temperature_delta[i]);
    }

    // Keep average temperature aligned with the environment set by the system.
    double average = 0.0;
    for (double t : temperature_cache_) {
        average += t;
    }
    temperature_ = static_cast<float>(average / static_cast<double>(count));
}

void GasSystem::integrate_particles(float dt) {
    auto& bodies = particles_.get_all_bodies();
    const size_t count = bodies.size();

    for (size_t i = 0; i < count; ++i) {
        PhysicsBody& body = bodies[i];
        if (!body.is_enabled || body.body_type != 1) {
            continue;
        }

        Vec3 acceleration = force_cache_[i] / std::max(body.mass, 1.0f);
        body.acceleration = acceleration;
        body.velocity = body.velocity + acceleration * static_cast<double>(dt);
        body.velocity = body.velocity * (1.0 - std::min(0.25, static_cast<double>(dt) * 0.1));
        body.position = body.position + body.velocity * static_cast<double>(dt);
    }
}

}  // namespace physics_core
