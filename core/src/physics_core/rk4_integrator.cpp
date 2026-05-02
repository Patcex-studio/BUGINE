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
#include "physics_core/integrators.h"
#include <cassert>
#include <cmath>
#include <functional>

namespace physics_core {

// ============================================================================
// Particle State and Derivative Function Types
// ============================================================================

/**
 * @struct ParticleState
 * @brief Represents the state of a particle for RK4 integration
 */
struct ParticleState {
    Vec3 position;      ///< Current position
    Vec3 velocity;      ///< Current velocity
    float mass;         ///< Particle mass
    // Additional parameters can be added here as needed
};

/**
 * @typedef DerivativeFunc
 * @brief Function type for computing derivatives of particle state
 * Returns the derivative d(state)/dt = [velocity, acceleration]
 */
using DerivativeFunc = std::function<std::pair<Vec3, Vec3>(const ParticleState&)>;

// ============================================================================
// RK4 Integration Functions
// ============================================================================

/**
 * @brief Integrate particle state using 4th-order Runge-Kutta method
 * @tparam T Floating point type (float or double)
 * @param state Particle state to integrate (modified in-place)
 * @param dt Time step
 * @param deriv_func Function to compute state derivatives
 */
template<typename T>
void integrate_rk4(ParticleState& state, T dt, DerivativeFunc deriv_func) noexcept {
    static_assert(std::is_floating_point_v<T>, "T must be a floating point type");

    // Handle edge cases
    if (dt == 0) {
        return; // No-op for zero time step
    }
    assert(dt > 0 && "Negative time step not supported");

    // Convert to template type
    T dt_half = dt * static_cast<T>(0.5);
    T dt_sixth = dt / static_cast<T>(6.0);

    // Stage 1: k1 = f(t, y)
    auto [k1_pos, k1_vel] = deriv_func(state);

    // Stage 2: k2 = f(t + dt/2, y + dt/2 * k1)
    ParticleState state2 = state;
    state2.position = state.position + k1_pos * dt_half;
    state2.velocity = state.velocity + k1_vel * dt_half;
    auto [k2_pos, k2_vel] = deriv_func(state2);

    // Stage 3: k3 = f(t + dt/2, y + dt/2 * k2)
    ParticleState state3 = state;
    state3.position = state.position + k2_pos * dt_half;
    state3.velocity = state.velocity + k2_vel * dt_half;
    auto [k3_pos, k3_vel] = deriv_func(state3);

    // Stage 4: k4 = f(t + dt, y + dt * k3)
    ParticleState state4 = state;
    state4.position = state.position + k3_pos * dt;
    state4.velocity = state.velocity + k3_vel * dt;
    auto [k4_pos, k4_vel] = deriv_func(state4);

    // Final update: y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    Vec3 delta_pos = (k1_pos + k2_pos * static_cast<T>(2.0) + k3_pos * static_cast<T>(2.0) + k4_pos) * dt_sixth;
    Vec3 delta_vel = (k1_vel + k2_vel * static_cast<T>(2.0) + k3_vel * static_cast<T>(2.0) + k4_vel) * dt_sixth;

    state.position = state.position + delta_pos;
    state.velocity = state.velocity + delta_vel;
}

// Explicit instantiations for float and double
template void integrate_rk4<float>(ParticleState& state, float dt, DerivativeFunc deriv_func) noexcept;
template void integrate_rk4<double>(ParticleState& state, double dt, DerivativeFunc deriv_func) noexcept;

// ============================================================================
// Standalone RK4 Class Implementation (if needed)
// ============================================================================

// The RK4Integrator class implementation remains in verlet_integrator.cpp
// This file contains only the standalone integrate_rk4 function

}  // namespace physics_core
