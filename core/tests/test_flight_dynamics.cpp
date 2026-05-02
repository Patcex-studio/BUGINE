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
#include <iostream>
#include <cassert>
#include <vector>
#include <immintrin.h>
#include "physics_core/flight_dynamics.h"

using namespace physics_core;

static AircraftState make_test_state(float x, float y, float z, float vx, float vy, float vz) {
    AircraftState state;
    state.position = _mm256_setr_ps(x, y, z, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    state.velocity = _mm256_setr_ps(vx, vy, vz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    state.angular_velocity = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    state.orientation = _mm256_setr_ps(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    state.mass = 12000.0f;
    state.inertia_xx = 1.5e6f;
    state.inertia_yy = 1.5e6f;
    state.inertia_zz = 2.5e6f;
    state.aircraft_type = AIRCRAFT_FIGHTER;
    state.flight_phase = FLIGHT_CRUISE;
    return state;
}

int main() {
    std::cout << "\n=== Flight Dynamics System Tests ===" << std::endl;

    const size_t count = 8;
    std::vector<AircraftState> states(count);
    std::vector<AircraftControlSystem> controls(count);
    std::vector<AerodynamicForces> aerodynamics(count);
    std::vector<EngineThrust> thrust(count);

    for (size_t i = 0; i < count; ++i) {
        states[i] = make_test_state(0.0f, 1000.0f, 500.0f, 220.0f, 0.0f, 5.0f);
        controls[i].ailerons[0].deflection_angle = 0.02f;
        controls[i].ailerons[1].deflection_angle = -0.02f;
        controls[i].elevators[0].deflection_angle = 0.01f;
        controls[i].elevators[1].deflection_angle = 0.01f;
        controls[i].rudder.deflection_angle = 0.005f;
        thrust[i].thrust = 1.2e5f;
        thrust[i].roll_moment = 0.0f;
        thrust[i].pitch_moment = 0.0f;
        thrust[i].yaw_moment = 0.0f;
    }

    FlightDynamicsSystem::calculate_aerodynamics_simd(states.data(), controls.data(), aerodynamics.data(), count);

    for (size_t i = 0; i < count; ++i) {
        assert(aerodynamics[i].force_z > 0.0f);
        assert(std::abs(aerodynamics[i].moment_roll) > 0.0f);
    }

    FlightDynamicsSystem::integrate_6dof_simd(states.data(), aerodynamics.data(), thrust.data(), 1.0f / 60.0f, count);

    for (size_t i = 0; i < count; ++i) {
        alignas(32) float pos[8];
        _mm256_store_ps(pos, states[i].position);
        assert(pos[0] > 0.0f);
    }

    StallConditions stall;
    stall.critical_aoa = 15.0f;
    stall.stall_hysteresis = 2.0f;
    stall.spin_threshold = 0.1f;
    stall.is_stalled = false;
    stall.is_spinning = false;
    stall.recovery_time = 2.0f;

    ControlInputs inputs{0.9f, 0.0f, 0.0f, 0.2f, 0.0f};
    FlightDynamicsSystem::detect_stall_spin(states[0], inputs, stall);
    assert(!stall.is_spinning || stall.is_stalled);

    std::cout << "✓ Flight dynamics tests passed" << std::endl;
    return 0;
}
