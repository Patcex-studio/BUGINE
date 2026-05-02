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

#include "physics_body.h"
#include <vector>

namespace physics_core {

// ============================================================================
// Physics Integrators
// ============================================================================

/**
 * @class IntegratorBase
 * @brief Abstract base class for physics integrators
 */
class IntegratorBase {
public:
    virtual ~IntegratorBase() = default;

    /**
     * Integrate physics bodies forward in time
     * @param bodies Vector of physics bodies
     * @param dt Time step in seconds
     */
    virtual void integrate(std::vector<PhysicsBody>& bodies, float dt) = 0;

    /**
     * Get integrator name
     */
    virtual const char* name() const = 0;
};

/**
 * @class VerletIntegrator
 * @brief Verlet integration method (semi-implicit)
 * 
 * Criteria:
 * - Stable at dt = 1/60 second
 * - Energy conservative
 * - Memory efficient (only stores position and old_position)
 * - Good for constraint-based physics
 * 
 * Algorithm:
 * x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
 */
class VerletIntegrator : public IntegratorBase {
public:
    VerletIntegrator();
    void integrate(std::vector<PhysicsBody>& bodies, float dt) override;
    const char* name() const override { return "Verlet"; }

private:
    bool initialized_;
    std::vector<Vec3> previous_positions_;
    void ensure_storage(size_t body_count);
};

/**
 * @class RK4Integrator
 * @brief 4th-order Runge-Kutta integration method
 * 
 * Criteria:
 * - Accuracy 3 orders of magnitude better than Euler
 * - Stable for higher velocities
 * - More expensive computationally (4x evaluations per step)
 * - Best for accurate trajectory prediction
 * 
 * Algorithm:
 * k1 = f(t, y)
 * k2 = f(t + dt/2, y + dt/2 * k1)
 * k3 = f(t + dt/2, y + dt/2 * k2)
 * k4 = f(t + dt, y + dt * k3)
 * y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
 */
class RK4Integrator : public IntegratorBase {
public:
    RK4Integrator();
    void integrate(std::vector<PhysicsBody>& bodies, float dt) override;
    const char* name() const override { return "RK4"; }

private:
    // Temporary storage for RK4 stages
    std::vector<Vec3> k1_vel_, k2_vel_, k3_vel_, k4_vel_;
    std::vector<Vec3> k1_pos_, k2_pos_, k3_pos_, k4_pos_;

    void ensure_storage(size_t body_count);

    // Compute acceleration from forces
    Vec3 compute_acceleration(const PhysicsBody& body, const Vec3& external_force);
};

/**
 * @class EulerIntegrator
 * @brief Simple Euler forward integration (baseline)
 * 
 * v(t+dt) = v(t) + a(t)*dt
 * x(t+dt) = x(t) + v(t)*dt
 * 
 * Note: Less stable, provided for comparison
 */
class EulerIntegrator : public IntegratorBase {
public:
    void integrate(std::vector<PhysicsBody>& bodies, float dt) override;
    const char* name() const override { return "Euler (Baseline)"; }
};

}  // namespace physics_core
