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
#include "physics_core/sph_system.h"
#include <algorithm>
#include <cmath>

namespace physics_core {

FluidSystem::FluidSystem()
    : sph_system_(std::make_unique<SPHSystem>(100000)),
      current_fluid_type_(FluidType::WATER),
      viscosity_(0.001f),
      surface_tension_(0.07f) {
}

FluidSystem::~FluidSystem() = default;

void FluidSystem::update(float dt) {
    sph_system_->update(dt);
}

EntityID FluidSystem::add_body(const PhysicsBody& body) {
    std::vector<std::array<float, 3>> positions;
    positions.reserve(1);
    positions.push_back({
        static_cast<float>(body.position.x),
        static_cast<float>(body.position.y),
        static_cast<float>(body.position.z)
    });
    sph_system_->add_particles(positions, current_fluid_type_);
    return static_cast<EntityID>(sph_system_->get_particles().size() - 1);
}

void FluidSystem::remove_body(EntityID id) {
    if (id != INVALID_ENTITY_ID) {
        sph_system_->remove_particles({static_cast<uint32_t>(id)});
    }
}

PhysicsBody* FluidSystem::get_body(EntityID id) {
    if (id >= dummy_bodies_.size()) {
        dummy_bodies_.resize(id + 1);
    }
    return &dummy_bodies_[id];
}

void FluidSystem::initialize_fluid(FluidType type, const std::vector<std::array<float, 3>>& positions) {
    current_fluid_type_ = type;
    sph_system_->initialize_fluid(type, positions);
}

float FluidSystem::get_average_density() const {
    const auto& particles = sph_system_->get_particles();
    if (particles.empty()) {
        return 1000.0f;
    }

    float total_density = 0.0f;
    for (const auto& p : particles) {
        total_density += p.density;
    }
    return total_density / static_cast<float>(particles.size());
}

const std::vector<SPHParticle>& FluidSystem::get_sph_particles() const {
    return sph_system_->get_particles();
}

}  // namespace physics_core
