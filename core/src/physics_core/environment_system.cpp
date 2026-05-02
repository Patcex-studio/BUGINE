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
#include "physics_core/environment_system.h"
#include "physics_core/physics_core.h"
#include <algorithm>
#include <cmath>

namespace physics_core {

EnvironmentSystem::EnvironmentSystem() {
    current_weather_ = {0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0.5f, 10000.0f, WeatherType::CLEAR, 0.0f};
}

void EnvironmentSystem::update_weather_effects(
    const WeatherState& weather,
    TerrainSystem& terrain,
    PhysicsCore* /*physics*/
) {
    current_weather_ = weather;
    terrain.apply_rain(weather.rain_intensity);
    terrain.apply_moisture_decay(0.016f);
    terrain.generate_mud_from_vehicles(weather.rain_intensity > 0.25f ? 0.65f : 1.0f);
    terrain.update_material_conditions(weather.rain_intensity, weather.temperature);
}

void EnvironmentSystem::calculate_ground_forces(
    const VehicleState* vehicles,
    GroundInteractionForces* forces,
    size_t count,
    const TerrainSystem& terrain
) const {
    for (size_t i = 0; i < count; ++i) {
        const VehicleState& vehicle = vehicles[i];
        float accumulated_height = 0.0f;
        float valid_contacts = 0.0f;
        float average_traction = 0.0f;
        float average_friction = 0.0f;

        for (size_t j = 0; j < vehicle.contact_count && j < vehicle.contact_points.size(); ++j) {
            float sample_height = 0.0f;
            const Vec3& contact_point = vehicle.contact_points[j];
            if (terrain.sample_height(contact_point.x, contact_point.y, sample_height)) {
                accumulated_height += sample_height;
                valid_contacts += 1.0f;
            }
        }

        forces[i].normal = Vec3(0.0, 0.0, 1.0);
        if (valid_contacts > 0.0f) {
            float average_height = accumulated_height / valid_contacts;
            forces[i].deformation_pressure = std::min(1.0f, vehicle.mass * 0.001f / std::max(0.1f, average_height + 0.01f));
        } else {
            forces[i].deformation_pressure = 0.0f;
        }

        uint8_t surface_id = terrain.query_surface_type(
            vehicle.contact_points[0].x,
            vehicle.contact_points[0].y
        );
        const SurfaceMaterial& material = terrain.get_material(surface_id);
        average_friction = material.friction_coefficient;
        average_traction = material.traction_factor;

        float rain_factor = std::clamp(current_weather_.rain_intensity, 0.0f, 1.0f);
        forces[i].friction_coefficient = std::max(0.05f, average_friction * (1.0f - rain_factor * 0.25f));
        forces[i].rolling_resistance = material.rolling_resistance + forces[i].deformation_pressure * 0.12f;
        forces[i].traction_factor = average_traction * (1.0f - rain_factor * 0.15f);
    }
}

void EnvironmentSystem::apply_environment_forces(
    PhysicsBody* bodies,
    const GroundInteractionForces* forces,
    const WeatherState& weather,
    size_t count
) const {
    float drag_coefficient = 0.03f + weather.wind_speed * 0.0015f;
    float rain_factor = std::clamp(weather.rain_intensity, 0.0f, 1.0f);
    for (size_t i = 0; i < count; ++i) {
        PhysicsBody& body = bodies[i];
        if (body.body_type != 1 || !body.is_enabled) {
            continue;
        }

        const GroundInteractionForces& force = forces[i];
        Vec3 friction_force = force.normal * (-force.friction_coefficient * body.mass);
        body.acceleration = body.acceleration + friction_force * body.inv_mass;

        body.velocity = body.velocity * (1.0f - std::min(0.5f, rain_factor * 0.2f + drag_coefficient));
    }
}

}  // namespace physics_core
