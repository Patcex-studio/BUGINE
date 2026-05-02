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
#include "terrain_system.h"
#include <array>
#include <cstdint>
#include <cstddef>

namespace physics_core {

struct WeatherState {
    float rain_intensity;
    float wind_speed;
    float wind_direction_x;
    float wind_direction_y;
    float temperature;
    float humidity;
    float visibility;
    uint32_t weather_type;
    float transition_progress;
};

namespace WeatherType {
    static constexpr uint32_t CLEAR = 0;
    static constexpr uint32_t RAIN = 1;
    static constexpr uint32_t SNOW = 2;
    static constexpr uint32_t FOG = 3;
    static constexpr uint32_t STORM = 4;
}

struct GroundInteractionForces {
    Vec3 normal;
    float friction_coefficient;
    float rolling_resistance;
    float traction_factor;
    float deformation_pressure;
};

struct VehicleState {
    EntityID vehicle_id;
    std::array<Vec3, 4> contact_points;
    size_t contact_count;
    float mass;
    float speed;
};

class EnvironmentSystem {
public:
    EnvironmentSystem();

    void update_weather_effects(
        const WeatherState& weather,
        TerrainSystem& terrain,
        class PhysicsCore* physics = nullptr
    );

    void calculate_ground_forces(
        const VehicleState* vehicles,
        GroundInteractionForces* forces,
        size_t count,
        const TerrainSystem& terrain
    ) const;

    void apply_environment_forces(
        PhysicsBody* bodies,
        const GroundInteractionForces* forces,
        const WeatherState& weather,
        size_t count
    ) const;

    const WeatherState& get_current_weather() const { return current_weather_; }

private:
    WeatherState current_weather_;
};

}  // namespace physics_core
