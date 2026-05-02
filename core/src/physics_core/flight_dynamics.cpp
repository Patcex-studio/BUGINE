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
#include "physics_core/flight_dynamics.h"
#include "physics_core/physics_core.h"
#include <algorithm>
#include <cstring>

namespace physics_core {

static inline float extract_element(const __m256& value, size_t index) {
    alignas(32) float temp[8];
    _mm256_store_ps(temp, value);
    return temp[index];
}

static inline __m256 load_component(const AircraftState* states, size_t start_index, size_t component_offset) {
    alignas(32) float values[FLIGHT_SIMD_BATCH];
    for (size_t i = 0; i < FLIGHT_SIMD_BATCH; ++i) {
        alignas(32) float temp[8];
        _mm256_store_ps(temp, *reinterpret_cast<const __m256*>(reinterpret_cast<const uint8_t*>(&states[start_index + i]) + component_offset));
        values[i] = temp[0];
    }
    return _mm256_load_ps(values);
}

static inline void store_results(const __m256& src, float* dst) {
    _mm256_store_ps(dst, src);
}

static inline void advance_quaternion(
    const __m256& qw,
    const __m256& qx,
    const __m256& qy,
    const __m256& qz,
    const __m256& wx,
    const __m256& wy,
    const __m256& wz,
    __m256& out_qw,
    __m256& out_qx,
    __m256& out_qy,
    __m256& out_qz,
    const __m256& dt_v
) {
    __m256 half = _mm256_set1_ps(0.5f);

    __m256 dqw = _mm256_mul_ps(half, _mm256_sub_ps(
        _mm256_sub_ps(_mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(0.0f), qx), wx), _mm256_mul_ps(qy, wy)),
        _mm256_mul_ps(qz, wz)
    ));
    __m256 dqx = _mm256_mul_ps(half, _mm256_add_ps(
        _mm256_add_ps(_mm256_mul_ps(qw, wx), _mm256_mul_ps(qy, wz)),
        _mm256_mul_ps(qz, wy)
    ));
    __m256 dqy = _mm256_mul_ps(half, _mm256_add_ps(
        _mm256_add_ps(_mm256_mul_ps(qw, wy), _mm256_mul_ps(qz, wx)),
        _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(0.0f), qx), wz)
    ));
    __m256 dqz = _mm256_mul_ps(half, _mm256_add_ps(
        _mm256_add_ps(_mm256_mul_ps(qw, wz), _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(0.0f), qy), wx)),
        _mm256_mul_ps(qx, wy)
    ));

    out_qw = _mm256_add_ps(qw, _mm256_mul_ps(dqw, dt_v));
    out_qx = _mm256_add_ps(qx, _mm256_mul_ps(dqx, dt_v));
    out_qy = _mm256_add_ps(qy, _mm256_mul_ps(dqy, dt_v));
    out_qz = _mm256_add_ps(qz, _mm256_mul_ps(dqz, dt_v));

    __m256 len2 = _mm256_add_ps(
        _mm256_add_ps(_mm256_mul_ps(out_qw, out_qw), _mm256_mul_ps(out_qx, out_qx)),
        _mm256_add_ps(_mm256_mul_ps(out_qy, out_qy), _mm256_mul_ps(out_qz, out_qz))
    );
    __m256 inv_len = _mm256_div_ps(_mm256_set1_ps(1.0f), _mm256_sqrt_ps(_mm256_add_ps(len2, _mm256_set1_ps(1e-8f))));
    out_qw = _mm256_mul_ps(out_qw, inv_len);
    out_qx = _mm256_mul_ps(out_qx, inv_len);
    out_qy = _mm256_mul_ps(out_qy, inv_len);
    out_qz = _mm256_mul_ps(out_qz, inv_len);
}

static inline float clampf_scalar(float value, float min_val, float max_val) {
    return std::min(std::max(value, min_val), max_val);
}

void FlightDynamicsSystem::calculate_aerodynamics_simd(
    const AircraftState* states,
    const AircraftControlSystem* controls,
    AerodynamicForces* output,
    size_t count
) {
    if (count == 0) {
        return;
    }

    for (size_t idx = 0; idx + FLIGHT_SIMD_BATCH <= count; idx += FLIGHT_SIMD_BATCH) {
        alignas(32) float pos_x[FLIGHT_SIMD_BATCH];
        alignas(32) float pos_y[FLIGHT_SIMD_BATCH];
        alignas(32) float pos_z[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_x[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_y[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_z[FLIGHT_SIMD_BATCH];
        alignas(32) float mass[FLIGHT_SIMD_BATCH];
        alignas(32) float aoa[FLIGHT_SIMD_BATCH];
        alignas(32) float sideslip[FLIGHT_SIMD_BATCH];
        alignas(32) float airspeed[FLIGHT_SIMD_BATCH];

        for (size_t lane = 0; lane < FLIGHT_SIMD_BATCH; ++lane) {
            alignas(32) float p[8];
            _mm256_store_ps(p, states[idx + lane].world_position);
            pos_x[lane] = p[0];
            pos_y[lane] = p[1];
            pos_z[lane] = p[2];

            _mm256_store_ps(p, states[idx + lane].linear_velocity);
            vel_x[lane] = p[0];
            vel_y[lane] = p[1];
            vel_z[lane] = p[2];

            mass[lane] = states[idx + lane].mass_kg;
            aoa[lane] = states[idx + lane].angle_of_attack_deg * 3.141592653589793f / 180.0f; // Convert to radians
            sideslip[lane] = states[idx + lane].sideslip_angle_deg * 3.141592653589793f / 180.0f;
            airspeed[lane] = states[idx + lane].airspeed_ms;
        }

        __m256 px = _mm256_load_ps(pos_x);
        __m256 py = _mm256_load_ps(pos_y);
        __m256 pz = _mm256_load_ps(pos_z);
        __m256 vx = _mm256_load_ps(vel_x);
        __m256 vy = _mm256_load_ps(vel_y);
        __m256 vz = _mm256_load_ps(vel_z);
        __m256 mass_v = _mm256_load_ps(mass);
        __m256 aoa_v = _mm256_load_ps(aoa);
        __m256 sideslip_v = _mm256_load_ps(sideslip);
        __m256 airspeed_v = _mm256_load_ps(airspeed);

        // Calculate dynamic pressure: q = 0.5 * ρ * V²
        __m256 altitude = pz;
        __m256 density = _mm256_max_ps(_mm256_set1_ps(0.4f), _mm256_sub_ps(_mm256_set1_ps(1.225f), _mm256_mul_ps(_mm256_set1_ps(0.0001f), _mm256_max_ps(_mm256_set1_ps(0.0f), altitude))));
        __m256 speed2 = _mm256_mul_ps(airspeed_v, airspeed_v);
        __m256 q = _mm256_mul_ps(_mm256_set1_ps(0.5f), _mm256_mul_ps(density, speed2));

        // Compute lift coefficient: CL = CL₀ + CL_α * α + CL_δ * δ_control
        __m256 cl0 = _mm256_set1_ps(0.2f);
        __m256 cl_alpha = _mm256_set1_ps(5.7f);
        __m256 cl = _mm256_add_ps(cl0, _mm256_mul_ps(cl_alpha, aoa_v));

        // Compute drag coefficient: CD = CD₀ + k * CL² + CD_δ * δ_control
        __m256 cd0 = _mm256_set1_ps(0.02f);
        __m256 k = _mm256_set1_ps(0.04f);
        __m256 cd = _mm256_add_ps(cd0, _mm256_mul_ps(k, _mm256_mul_ps(cl, cl)));

        // Calculate side force coefficient: CY = CY_β * β + CY_δ * δ_rudder
        __m256 cy_beta = _mm256_set1_ps(-0.3f);
        __m256 cy = _mm256_mul_ps(cy_beta, sideslip_v);

        // Wing area (assuming 20m² for now, should be from aircraft properties)
        __m256 wing_area = _mm256_set1_ps(20.0f);

        // Calculate forces
        __m256 lift = _mm256_mul_ps(_mm256_mul_ps(q, wing_area), cl);
        __m256 drag = _mm256_mul_ps(_mm256_mul_ps(q, wing_area), cd);
        __m256 side_force = _mm256_mul_ps(_mm256_mul_ps(q, wing_area), cy);

        // Control surface deflections
        alignas(32) float roll_deflection[FLIGHT_SIMD_BATCH];
        alignas(32) float pitch_deflection[FLIGHT_SIMD_BATCH];
        alignas(32) float yaw_deflection[FLIGHT_SIMD_BATCH];

        for (size_t lane = 0; lane < FLIGHT_SIMD_BATCH; ++lane) {
            const AircraftControlSystem& control = controls[idx + lane];
            // Simplified control calculation - should use proper surface effectiveness
            float aileron_sum = (control.ailerons[0].current_deflection_rad - control.ailerons[1].current_deflection_rad) * 0.5f;
            float elevator_sum = (control.elevators[0].current_deflection_rad + control.elevators[1].current_deflection_rad) * 0.5f;
            float rudder_value = control.rudder.current_deflection_rad;
            roll_deflection[lane] = aileron_sum * control.ailerons[0].effectiveness_coeff;
            pitch_deflection[lane] = elevator_sum * control.elevators[0].effectiveness_coeff;
            yaw_deflection[lane] = rudder_value * control.rudder.effectiveness_coeff;
        }

        __m256 moment_roll = _mm256_mul_ps(_mm256_load_ps(roll_deflection), _mm256_set1_ps(0.22f));
        __m256 moment_pitch = _mm256_mul_ps(_mm256_load_ps(pitch_deflection), _mm256_set1_ps(0.31f));
        __m256 moment_yaw = _mm256_mul_ps(_mm256_load_ps(yaw_deflection), _mm256_set1_ps(0.18f));

        // Store results
        alignas(32) float force_x[FLIGHT_SIMD_BATCH];
        alignas(32) float force_y[FLIGHT_SIMD_BATCH];
        alignas(32) float force_z[FLIGHT_SIMD_BATCH];
        alignas(32) float moment_roll_out[FLIGHT_SIMD_BATCH];
        alignas(32) float moment_pitch_out[FLIGHT_SIMD_BATCH];
        alignas(32) float moment_yaw_out[FLIGHT_SIMD_BATCH];

        _mm256_store_ps(force_x, _mm256_sub_ps(_mm256_set1_ps(0.0f), drag)); // Drag is negative X
        _mm256_store_ps(force_y, side_force);
        _mm256_store_ps(force_z, lift); // Lift is positive Z
        _mm256_store_ps(moment_roll_out, moment_roll);
        _mm256_store_ps(moment_pitch_out, moment_pitch);
        _mm256_store_ps(moment_yaw_out, moment_yaw);

        for (size_t lane = 0; lane < FLIGHT_SIMD_BATCH; ++lane) {
            output[idx + lane].force_x = force_x[lane];
            output[idx + lane].force_y = force_y[lane];
            output[idx + lane].force_z = force_z[lane];
            output[idx + lane].moment_roll = moment_roll_out[lane];
            output[idx + lane].moment_pitch = moment_pitch_out[lane];
            output[idx + lane].moment_yaw = moment_yaw_out[lane];
        }
    }

    // Handle remaining aircraft (non-SIMD)
    for (size_t idx = (count / FLIGHT_SIMD_BATCH) * FLIGHT_SIMD_BATCH; idx < count; ++idx) {
        const AircraftState& state = states[idx];
        const AircraftControlSystem& control = controls[idx];

        float altitude = state.altitude_m;
        float rho = std::max(0.4f, 1.225f - 0.0001f * std::max(altitude, 0.0f));
        float speed = state.airspeed_ms;
        float q = 0.5f * rho * speed * speed;

        // Coefficients
        float alpha_rad = state.angle_of_attack_deg * 3.141592653589793f / 180.0f;
        float beta_rad = state.sideslip_angle_deg * 3.141592653589793f / 180.0f;

        float cl = 0.2f + 5.7f * alpha_rad;
        float cd = 0.02f + 0.04f * cl * cl;
        float cy = -0.3f * beta_rad;

        float wing_area = 20.0f; // Should be from aircraft properties
        float lift = q * wing_area * cl;
        float drag = q * wing_area * cd;
        float side_force = q * wing_area * cy;

        // Control moments
        float aileron_sum = (control.ailerons[0].current_deflection_rad - control.ailerons[1].current_deflection_rad) * 0.5f;
        float elevator_sum = (control.elevators[0].current_deflection_rad + control.elevators[1].current_deflection_rad) * 0.5f;
        float rudder_value = control.rudder.current_deflection_rad;

        output[idx].force_x = -drag;
        output[idx].force_y = side_force;
        output[idx].force_z = lift;
        output[idx].moment_roll = aileron_sum * control.ailerons[0].effectiveness_coeff * 0.22f;
        output[idx].moment_pitch = elevator_sum * control.elevators[0].effectiveness_coeff * 0.31f;
        output[idx].moment_yaw = rudder_value * control.rudder.effectiveness_coeff * 0.18f;
    }
}

void FlightDynamicsSystem::integrate_6dof_simd(
    AircraftState* states,
    const AerodynamicForces* forces,
    const PropulsionForces* propulsion,
    float dt,
    size_t count
) {
    if (count == 0) {
        return;
    }

    __m256 dt_v = _mm256_set1_ps(dt);
    __m256 half_dt2 = _mm256_set1_ps(0.5f * dt * dt);
    __m256 gravity = _mm256_set1_ps(-GRAVITY_MS2); // Gravity in Z direction

    for (size_t idx = 0; idx + FLIGHT_SIMD_BATCH <= count; idx += FLIGHT_SIMD_BATCH) {
        alignas(32) float pos_x[FLIGHT_SIMD_BATCH];
        alignas(32) float pos_y[FLIGHT_SIMD_BATCH];
        alignas(32) float pos_z[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_x[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_y[FLIGHT_SIMD_BATCH];
        alignas(32) float vel_z[FLIGHT_SIMD_BATCH];
        alignas(32) float ang_x[FLIGHT_SIMD_BATCH];
        alignas(32) float ang_y[FLIGHT_SIMD_BATCH];
        alignas(32) float ang_z[FLIGHT_SIMD_BATCH];
        alignas(32) float ori_w[FLIGHT_SIMD_BATCH];
        alignas(32) float ori_x[FLIGHT_SIMD_BATCH];
        alignas(32) float ori_y[FLIGHT_SIMD_BATCH];
        alignas(32) float ori_z[FLIGHT_SIMD_BATCH];
        alignas(32) float mass[FLIGHT_SIMD_BATCH];
        alignas(32) float inertia_xx[FLIGHT_SIMD_BATCH];
        alignas(32) float inertia_yy[FLIGHT_SIMD_BATCH];
        alignas(32) float inertia_zz[FLIGHT_SIMD_BATCH];
        alignas(32) float thrust[FLIGHT_SIMD_BATCH];
        alignas(32) float fx[FLIGHT_SIMD_BATCH];
        alignas(32) float fy[FLIGHT_SIMD_BATCH];
        alignas(32) float fz[FLIGHT_SIMD_BATCH];
        alignas(32) float roll_m[FLIGHT_SIMD_BATCH];
        alignas(32) float pitch_m[FLIGHT_SIMD_BATCH];
        alignas(32) float yaw_m[FLIGHT_SIMD_BATCH];

        for (size_t lane = 0; lane < FLIGHT_SIMD_BATCH; ++lane) {
            alignas(32) float p[8];
            _mm256_store_ps(p, states[idx + lane].world_position);
            pos_x[lane] = p[0];
            pos_y[lane] = p[1];
            pos_z[lane] = p[2];
            _mm256_store_ps(p, states[idx + lane].linear_velocity);
            vel_x[lane] = p[0];
            vel_y[lane] = p[1];
            vel_z[lane] = p[2];
            _mm256_store_ps(p, states[idx + lane].angular_velocity);
            ang_x[lane] = p[0];
            ang_y[lane] = p[1];
            ang_z[lane] = p[2];
            _mm256_store_ps(p, states[idx + lane].orientation_quaternion);
            ori_w[lane] = p[0];
            ori_x[lane] = p[1];
            ori_y[lane] = p[2];
            ori_z[lane] = p[3];
            mass[lane] = states[idx + lane].mass_kg;
            inertia_xx[lane] = states[idx + lane].inertia_xx;
            inertia_yy[lane] = states[idx + lane].inertia_yy;
            inertia_zz[lane] = states[idx + lane].inertia_zz;
            thrust[lane] = propulsion[idx + lane].thrust;
            fx[lane] = forces[idx + lane].force_x;
            fy[lane] = forces[idx + lane].force_y;
            fz[lane] = forces[idx + lane].force_z + (-GRAVITY_MS2) * mass[lane]; // Add gravity
            roll_m[lane] = forces[idx + lane].moment_roll + propulsion[idx + lane].roll_moment;
            pitch_m[lane] = forces[idx + lane].moment_pitch + propulsion[idx + lane].pitch_moment;
            yaw_m[lane] = forces[idx + lane].moment_yaw + propulsion[idx + lane].yaw_moment;
        }

        __m256 px = _mm256_load_ps(pos_x);
        __m256 py = _mm256_load_ps(pos_y);
        __m256 pz = _mm256_load_ps(pos_z);
        __m256 vx = _mm256_load_ps(vel_x);
        __m256 vy = _mm256_load_ps(vel_y);
        __m256 vz = _mm256_load_ps(vel_z);
        __m256 ax = _mm256_div_ps(_mm256_add_ps(_mm256_load_ps(fx), _mm256_load_ps(thrust)), _mm256_load_ps(mass));
        __m256 ay = _mm256_div_ps(_mm256_load_ps(fy), _mm256_load_ps(mass));
        __m256 az = _mm256_div_ps(_mm256_load_ps(fz), _mm256_load_ps(mass));

        // Integrate linear motion
        vx = _mm256_add_ps(vx, _mm256_mul_ps(ax, dt_v));
        vy = _mm256_add_ps(vy, _mm256_mul_ps(ay, dt_v));
        vz = _mm256_add_ps(vz, _mm256_mul_ps(az, dt_v));

        px = _mm256_add_ps(px, _mm256_add_ps(_mm256_mul_ps(vx, dt_v), _mm256_mul_ps(ax, half_dt2)));
        py = _mm256_add_ps(py, _mm256_add_ps(_mm256_mul_ps(vy, dt_v), _mm256_mul_ps(ay, half_dt2)));
        pz = _mm256_add_ps(pz, _mm256_add_ps(_mm256_mul_ps(vz, dt_v), _mm256_mul_ps(az, half_dt2)));

        // Integrate angular motion
        __m256 wx = _mm256_load_ps(ang_x);
        __m256 wy = _mm256_load_ps(ang_y);
        __m256 wz = _mm256_load_ps(ang_z);
        __m256 qx = _mm256_load_ps(ori_x);
        __m256 qy = _mm256_load_ps(ori_y);
        __m256 qz = _mm256_load_ps(ori_z);
        __m256 qw = _mm256_load_ps(ori_w);

        // Angular acceleration
        __m256 alpha_x = _mm256_div_ps(_mm256_load_ps(roll_m), _mm256_load_ps(inertia_xx));
        __m256 alpha_y = _mm256_div_ps(_mm256_load_ps(pitch_m), _mm256_load_ps(inertia_yy));
        __m256 alpha_z = _mm256_div_ps(_mm256_load_ps(yaw_m), _mm256_load_ps(inertia_zz));

        wx = _mm256_add_ps(wx, _mm256_mul_ps(alpha_x, dt_v));
        wy = _mm256_add_ps(wy, _mm256_mul_ps(alpha_y, dt_v));
        wz = _mm256_add_ps(wz, _mm256_mul_ps(alpha_z, dt_v));

        __m256 new_qw, new_qx, new_qy, new_qz;
        advance_quaternion(qw, qx, qy, qz, wx, wy, wz, new_qw, new_qx, new_qy, new_qz, dt_v);

        // Update flight parameters
        __m256 speed2 = _mm256_add_ps(_mm256_mul_ps(vx, vx), _mm256_add_ps(_mm256_mul_ps(vy, vy), _mm256_mul_ps(vz, vz)));
        __m256 speed = _mm256_sqrt_ps(_mm256_add_ps(speed2, _mm256_set1_ps(1e-6f)));

        // Store results
        alignas(32) float out_px[FLIGHT_SIMD_BATCH];
        alignas(32) float out_py[FLIGHT_SIMD_BATCH];
        alignas(32) float out_pz[FLIGHT_SIMD_BATCH];
        alignas(32) float out_vx[FLIGHT_SIMD_BATCH];
        alignas(32) float out_vy[FLIGHT_SIMD_BATCH];
        alignas(32) float out_vz[FLIGHT_SIMD_BATCH];
        alignas(32) float out_wx[FLIGHT_SIMD_BATCH];
        alignas(32) float out_wy[FLIGHT_SIMD_BATCH];
        alignas(32) float out_wz[FLIGHT_SIMD_BATCH];
        alignas(32) float out_qw[FLIGHT_SIMD_BATCH];
        alignas(32) float out_qx[FLIGHT_SIMD_BATCH];
        alignas(32) float out_qy[FLIGHT_SIMD_BATCH];
        alignas(32) float out_qz[FLIGHT_SIMD_BATCH];
        alignas(32) float out_speed[FLIGHT_SIMD_BATCH];

        _mm256_store_ps(out_px, px);
        _mm256_store_ps(out_py, py);
        _mm256_store_ps(out_pz, pz);
        _mm256_store_ps(out_vx, vx);
        _mm256_store_ps(out_vy, vy);
        _mm256_store_ps(out_vz, vz);
        _mm256_store_ps(out_wx, wx);
        _mm256_store_ps(out_wy, wy);
        _mm256_store_ps(out_wz, wz);
        _mm256_store_ps(out_qw, new_qw);
        _mm256_store_ps(out_qx, new_qx);
        _mm256_store_ps(out_qy, new_qy);
        _mm256_store_ps(out_qz, new_qz);
        _mm256_store_ps(out_speed, speed);

        for (size_t lane = 0; lane < FLIGHT_SIMD_BATCH; ++lane) {
            alignas(32) float new_position[8] = { out_px[lane], out_py[lane], out_pz[lane], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            states[idx + lane].world_position = _mm256_load_ps(new_position);
            alignas(32) float new_velocity[8] = { out_vx[lane], out_vy[lane], out_vz[lane], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            states[idx + lane].linear_velocity = _mm256_load_ps(new_velocity);
            alignas(32) float new_angular_velocity[8] = { out_wx[lane], out_wy[lane], out_wz[lane], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            states[idx + lane].angular_velocity = _mm256_load_ps(new_angular_velocity);
            alignas(32) float new_orientation[8] = { out_qw[lane], out_qx[lane], out_qy[lane], out_qz[lane], 0.0f, 0.0f, 0.0f, 0.0f };
            states[idx + lane].orientation_quaternion = _mm256_load_ps(new_orientation);

            // Update flight parameters
            states[idx + lane].airspeed_ms = out_speed[lane];
            states[idx + lane].altitude_m = out_pz[lane];
            // AoA and sideslip would need proper calculation from velocity and orientation
        }
    }

    // Handle remaining aircraft (non-SIMD)
    for (size_t idx = (count / FLIGHT_SIMD_BATCH) * FLIGHT_SIMD_BATCH; idx < count; ++idx) {
        AircraftState& state = states[idx];
        const AerodynamicForces& force = forces[idx];
        const PropulsionForces& prop = propulsion[idx];

        alignas(32) float v[8];
        _mm256_store_ps(v, state.linear_velocity);
        float ax = (force.force_x + prop.thrust) / state.mass_kg;
        float ay = force.force_y / state.mass_kg;
        float az = (force.force_z - GRAVITY_MS2) / state.mass_kg; // Gravity

        v[0] += ax * dt;
        v[1] += ay * dt;
        v[2] += az * dt;
        state.linear_velocity = _mm256_setr_ps(v[0], v[1], v[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

        alignas(32) float p[8];
        _mm256_store_ps(p, state.world_position);
        p[0] += v[0] * dt + 0.5f * ax * dt * dt;
        p[1] += v[1] * dt + 0.5f * ay * dt * dt;
        p[2] += v[2] * dt + 0.5f * az * dt * dt;
        state.world_position = _mm256_setr_ps(p[0], p[1], p[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

        // Angular integration
        float alpha_x = (force.moment_roll + prop.roll_moment) / state.inertia_xx;
        float alpha_y = (force.moment_pitch + prop.pitch_moment) / state.inertia_yy;
        float alpha_z = (force.moment_yaw + prop.yaw_moment) / state.inertia_zz;

        alignas(32) float w[8];
        _mm256_store_ps(w, state.angular_velocity);
        w[0] += alpha_x * dt;
        w[1] += alpha_y * dt;
        w[2] += alpha_z * dt;
        state.angular_velocity = _mm256_setr_ps(w[0], w[1], w[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

        // Quaternion update
        alignas(32) float q[8];
        _mm256_store_ps(q, state.orientation_quaternion);
        float qw = q[0], qx = q[1], qy = q[2], qz = q[3];
        float dqw = 0.5f * (-qx * w[0] - qy * w[1] - qz * w[2]);
        float dqx = 0.5f * (qw * w[0] + qy * w[2] - qz * w[1]);
        float dqy = 0.5f * (qw * w[1] - qx * w[2] + qz * w[0]);
        float dqz = 0.5f * (qw * w[2] + qx * w[1] - qy * w[0]);
        qw += dqw * dt;
        qx += dqx * dt;
        qy += dqy * dt;
        qz += dqz * dt;
        float qlen = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
        if (qlen > 1e-6f) {
            qw /= qlen;
            qx /= qlen;
            qy /= qlen;
            qz /= qlen;
        }
        state.orientation_quaternion = _mm256_setr_ps(qw, qx, qy, qz, 0.0f, 0.0f, 0.0f, 0.0f);

        // Update flight parameters
        state.airspeed_ms = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        state.altitude_m = p[2];
        // AoA and sideslip calculation would go here
    }
}

void FlightDynamicsSystem::calculate_aerodynamics(
    const AircraftState& state,
    const AircraftControlSystem& controls,
    AerodynamicForces& output_forces
) {
    // Calculate dynamic pressure: q = 0.5 * ρ * V²
    float altitude = state.altitude_m;
    float rho = std::max(0.4f, 1.225f - 0.0001f * std::max(altitude, 0.0f));
    float speed = state.airspeed_ms;
    float q = 0.5f * rho * speed * speed;

    // Convert angles to radians
    float alpha_rad = state.angle_of_attack_deg * 3.141592653589793f / 180.0f;
    float beta_rad = state.sideslip_angle_deg * 3.141592653589793f / 180.0f;

    // Compute lift coefficient: CL = CL₀ + CL_α * α + CL_δ * δ_control
    float cl0 = 0.2f;
    float cl_alpha = 5.7f;
    float cl = cl0 + cl_alpha * alpha_rad;

    // Add control surface effects
    float elevator_sum = (controls.elevators[0].current_deflection_rad + controls.elevators[1].current_deflection_rad) * 0.5f;
    cl += elevator_sum * controls.elevators[0].effectiveness_coeff * 0.5f;

    // Compute drag coefficient: CD = CD₀ + k * CL² + CD_δ * δ_control
    float cd0 = 0.02f;
    float k = 0.04f;
    float cd = cd0 + k * cl * cl;

    // Add control surface drag
    cd += std::abs(elevator_sum) * 0.01f;

    // Calculate side force coefficient: CY = CY_β * β + CY_δ * δ_rudder
    float cy_beta = -0.3f;
    float cy = cy_beta * beta_rad;
    cy += controls.rudder.current_deflection_rad * controls.rudder.effectiveness_coeff * 0.2f;

    // Wing area (should be from aircraft properties)
    float wing_area = 20.0f;

    // Calculate forces
    float lift = q * wing_area * cl;
    float drag = q * wing_area * cd;
    float side_force = q * wing_area * cy;

    // Calculate control surface moments
    float aileron_sum = (controls.ailerons[0].current_deflection_rad - controls.ailerons[1].current_deflection_rad) * 0.5f;
    float rudder_value = controls.rudder.current_deflection_rad;

    float moment_roll = aileron_sum * controls.ailerons[0].effectiveness_coeff * 0.22f;
    float moment_pitch = elevator_sum * controls.elevators[0].effectiveness_coeff * 0.31f;
    float moment_yaw = rudder_value * controls.rudder.effectiveness_coeff * 0.18f;

    // Apply Mach effects (transonic and compressibility)
    float mach = speed / SPEED_OF_SOUND_MPS;
    if (mach > 0.8f) {
        // Transonic drag rise and Mach tuck
        calculate_transonic_effects(mach, cd, moment_pitch);
        // Compressibility drag
        model_compressibility_drag(mach, cl, cd);
    }

    // Ground effect
    apply_ground_effect(altitude, cl);

    // Aircraft-specific dynamics (fixed-wing or rotary-wing)
    if (state.aircraft_type == AIRCRAFT_HELICOPTER) {
        // Helicopter-specific calculations
        simulate_rotor_blade_flapping(controls.collective_pitch, controls.cyclic_pitch_x);
        calculate_ground_effect(altitude);
        float descent_rate = extract_element(state.linear_velocity, 2);
        model_vortex_ring_state(descent_rate);
        simulate_retreating_blade_stall(state.airspeed_ms);
    } else {
        // Fixed-wing specific calculations
        simulate_fixed_wing_dynamics(state);
    }

    // FBW and stability augmentation
    ControlInputs fbw_inputs;
    fbw_inputs.throttle = controls.throttle_input;
    fbw_inputs.stick_pitch = controls.stick_pitch_input;
    fbw_inputs.stick_roll = controls.stick_roll_input;
    fbw_inputs.rudder = controls.rudder_input;
    fbw_inputs.flaps = 0.0f;
    simulate_fly_by_wire_systems(fbw_inputs);
    model_stability_augmentation(state);

    output_forces.force_x = -drag; // Drag is negative X
    output_forces.force_y = side_force;
    output_forces.force_z = lift;  // Lift is positive Z
    output_forces.moment_roll = moment_roll;
    output_forces.moment_pitch = moment_pitch;
    output_forces.moment_yaw = moment_yaw;
}

void FlightDynamicsSystem::integrate_6dof_aircraft(
    AircraftState& state,
    const AerodynamicForces& aerodynamics,
    const PropulsionForces& propulsion,
    float delta_time
) {
    // Calculate total forces: aerodynamic + propulsion + gravity
    float total_force_x = aerodynamics.force_x + propulsion.thrust;
    float total_force_y = aerodynamics.force_y + propulsion.roll_moment; // Simplified
    float total_force_z = aerodynamics.force_z - state.mass_kg * GRAVITY_MS2;

    // Calculate total moments
    float total_moment_x = aerodynamics.moment_roll + propulsion.roll_moment;
    float total_moment_y = aerodynamics.moment_pitch + propulsion.pitch_moment;
    float total_moment_z = aerodynamics.moment_yaw + propulsion.yaw_moment;

    // Integrate linear motion
    alignas(32) float v[8];
    _mm256_store_ps(v, state.linear_velocity);
    float ax = total_force_x / state.mass_kg;
    float ay = total_force_y / state.mass_kg;
    float az = total_force_z / state.mass_kg;

    v[0] += ax * delta_time;
    v[1] += ay * delta_time;
    v[2] += az * delta_time;
    state.linear_velocity = _mm256_setr_ps(v[0], v[1], v[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    alignas(32) float p[8];
    _mm256_store_ps(p, state.world_position);
    p[0] += v[0] * delta_time + 0.5f * ax * delta_time * delta_time;
    p[1] += v[1] * delta_time + 0.5f * ay * delta_time * delta_time;
    p[2] += v[2] * delta_time + 0.5f * az * delta_time * delta_time;
    state.world_position = _mm256_setr_ps(p[0], p[1], p[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Integrate angular motion
    float alpha_x = total_moment_x / state.inertia_xx;
    float alpha_y = total_moment_y / state.inertia_yy;
    float alpha_z = total_moment_z / state.inertia_zz;

    alignas(32) float w[8];
    _mm256_store_ps(w, state.angular_velocity);
    w[0] += alpha_x * delta_time;
    w[1] += alpha_y * delta_time;
    w[2] += alpha_z * delta_time;
    state.angular_velocity = _mm256_setr_ps(w[0], w[1], w[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Quaternion update
    alignas(32) float q[8];
    _mm256_store_ps(q, state.orientation_quaternion);
    float qw = q[0], qx = q[1], qy = q[2], qz = q[3];
    float dqw = 0.5f * (-qx * w[0] - qy * w[1] - qz * w[2]);
    float dqx = 0.5f * (qw * w[0] + qy * w[2] - qz * w[1]);
    float dqy = 0.5f * (qw * w[1] - qx * w[2] + qz * w[0]);
    float dqz = 0.5f * (qw * w[2] + qx * w[1] - qy * w[0]);
    qw += dqw * delta_time;
    qx += dqx * delta_time;
    qy += dqy * delta_time;
    qz += dqz * delta_time;
    float qlen = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
    if (qlen > 1e-6f) {
        qw /= qlen;
        qx /= qlen;
        qy /= qlen;
        qz /= qlen;
    }
    state.orientation_quaternion = _mm256_setr_ps(qw, qx, qy, qz, 0.0f, 0.0f, 0.0f, 0.0f);

    // Update flight parameters
    state.airspeed_ms = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    state.altitude_m = p[2];

    // Calculate angle of attack and sideslip (improved using quaternion transformation)
    if (state.airspeed_ms > 1e-3f) {
        // Transform velocity vector from world to body frame using inverse quaternion
        // Body frame: X=forward, Y=right, Z=down
        // q_inverse = [qw, -qx, -qy, -qz] for unit quaternion
        float qinv_w = qw;
        float qinv_x = -qx;
        float qinv_y = -qy;
        float qinv_z = -qz;
        
        // Rotate velocity vector: v_body = qinv * v_world * q
        // First rotation: temp = v_world * q
        float temp_x = v[0] * qinv_w + v[1] * qinv_z - v[2] * qinv_y;
        float temp_y = -v[0] * qinv_z + v[1] * qinv_w + v[2] * qinv_x;
        float temp_z = v[0] * qinv_y - v[1] * qinv_x + v[2] * qinv_w;
        
        // Second rotation: v_body = temp * qinv
        float vx_body = temp_x * qinv_w - temp_y * qinv_z + temp_z * qinv_y;
        float vy_body = temp_x * qinv_z + temp_y * qinv_w - temp_z * qinv_x;
        float vz_body = -temp_x * qinv_y + temp_y * qinv_x + temp_z * qinv_w;
        
        // Calculate angles from body-frame velocity components
        state.angle_of_attack_deg = std::atan2(vz_body, vx_body) * 180.0f / 3.141592653589793f;
        state.sideslip_angle_deg = std::atan2(vy_body, vx_body) * 180.0f / 3.141592653589793f;
    }
}

void PropulsionSystem::update_engine_performance(
    EngineState& engine_state,
    const EngineProperties& engine_props,
    const AircraftState& aircraft_state,
    float throttle_input,
    float delta_time
) {
    // Calculate ambient conditions
    float altitude = aircraft_state.altitude_m;
    float pressure_ratio = std::max(0.1f, 1.0f - altitude / 44300.0f); // Simplified pressure
    float temperature_ratio = std::max(0.5f, 1.0f - altitude / 10000.0f); // Simplified temperature

    // Determine thrust based on altitude and throttle
    int altitude_index = std::min(10, static_cast<int>(altitude / 1000.0f));
    float base_thrust = engine_props.thrust_curve[altitude_index] * engine_props.max_thrust_sea_level_N;

    // Apply throttle with spool dynamics
    float target_rpm = throttle_input * engine_props.max_rpm;
    float rpm_rate = throttle_input > engine_state.engine_rpm ? engine_props.spool_up_time_seconds : engine_props.spool_down_time_seconds;
    rpm_rate = 1.0f / rpm_rate; // Convert to rate per second

    engine_state.engine_rpm += (target_rpm - engine_state.engine_rpm) * rpm_rate * delta_time;
    engine_state.engine_rpm = std::clamp(engine_state.engine_rpm, 0.0f, engine_props.max_rpm);

    // Calculate thrust
    float thrust_factor = engine_state.engine_rpm / engine_props.max_rpm;
    engine_state.current_thrust_N = base_thrust * thrust_factor * pressure_ratio;

    // Apply afterburner if available
    if (engine_props.has_afterburner && throttle_input > 0.9f) {
        engine_state.afterburner_active = true;
        engine_state.current_thrust_N *= engine_props.afterburner_thrust_mult;
    } else {
        engine_state.afterburner_active = false;
    }

    // Calculate fuel consumption
    float fuel_rate = engine_props.fuel_consumption_rate * engine_state.current_thrust_N * delta_time;
    engine_state.fuel_remaining_kg -= fuel_rate;
    engine_state.fuel_remaining_kg = std::max(0.0f, engine_state.fuel_remaining_kg);

    // Update temperature (simplified)
    float target_temp = 500.0f + 500.0f * thrust_factor; // Base + thrust dependent
    engine_state.exhaust_temperature_C += (target_temp - engine_state.exhaust_temperature_C) * 0.1f * delta_time;

    // Apply damage effects
    if (engine_state.is_damaged) {
        engine_state.current_thrust_N *= engine_state.damage_thrust_mult;
    }

    // Calculate power for propeller engines
    if (engine_props.engine_type == PISTON_RADIAL || engine_props.engine_type == TURBOPROP) {
        engine_state.current_power_hp = engine_props.max_power_sea_level_hp * thrust_factor * pressure_ratio;
    }
}

void FlightDynamicsSystem::apply_flight_forces_to_physics(
    EntityID aircraft_entity,
    const AircraftState& state,
    const AerodynamicForces& aerodynamics,
    const PropulsionForces& propulsion,
    PhysicsCore& physics
) {
    // Apply aerodynamic and propulsion forces to the physics body.
    // PhysicsCore currently exposes apply_force and apply_impulse.

    Vec3 aero_force(aerodynamics.force_x, aerodynamics.force_y, aerodynamics.force_z);
    physics.apply_force(aircraft_entity, aero_force);

    // Propulsion thrust acts along the forward X-axis of the aircraft in body space.
    // Here we simplify and apply it directly in world space along the X-axis.
    Vec3 prop_force(propulsion.thrust, 0.0f, 0.0f);
    physics.apply_force(aircraft_entity, prop_force);

    // Apply weight separately so PhysicsCore can integrate gravity consistently.
    Vec3 weight_force(0.0f, 0.0f, -state.mass_kg * GRAVITY_MS2);
    physics.apply_force(aircraft_entity, weight_force);

    // Aerodynamic torques are not directly supported in PhysicsCore yet.
    // We can approximate them via impulses at offset points if needed.
    // For now only forces are applied to ensure the aircraft interacts with the physics system.
}

void FlightDynamicsSystem::initialize_from_modular_aircraft(
    EntityID aircraft_entity,
    const ModularAircraftBlueprint& blueprint,
    PhysicsCore& physics
) {
    // Calculate mass and inertia from blueprint values.
    float total_mass = blueprint.total_mass_kg;
    float inertia_xx = blueprint.inertia_xx;
    float inertia_yy = blueprint.inertia_yy;
    float inertia_zz = blueprint.inertia_zz;

    PhysicsBody* body = physics.get_body(aircraft_entity);
    if (!body) {
        return;
    }

    body->mass = std::max(total_mass, 0.1f);
    body->inv_mass = 1.0 / body->mass;

    // Update inertia tensor diagonals directly.
    Mat3x3 inertia;
    inertia(0, 0) = inertia_xx;
    inertia(1, 1) = inertia_yy;
    inertia(2, 2) = inertia_zz;
    body->inertia_tensor = inertia;

    Mat3x3 inertia_inv;
    inertia_inv(0, 0) = inertia_xx > 0.0f ? 1.0 / inertia_xx : 0.0;
    inertia_inv(1, 1) = inertia_yy > 0.0f ? 1.0 / inertia_yy : 0.0;
    inertia_inv(2, 2) = inertia_zz > 0.0f ? 1.0 / inertia_zz : 0.0;
    body->inertia_tensor_inv = inertia_inv;
}

void FlightDynamicsSystem::simulate_fixed_wing_dynamics(
    const AircraftState& state
) {
    float alpha_rad = state.angle_of_attack_deg * 3.141592653589793f / 180.0f;
    float lift_slope_factor = std::max(0.0f, 1.0f - std::abs(alpha_rad) / (25.0f * 3.141592653589793f / 180.0f));
    float drag_buildup = 1.0f + std::min(1.5f, std::abs(alpha_rad) / (10.0f * 3.141592653589793f / 180.0f));
    (void)lift_slope_factor;
    (void)drag_buildup;
}

void FlightDynamicsSystem::calculate_transonic_effects(
    float mach_number, float& drag_coefficient, float& pitch_moment
) {
    const float M_crit = 0.8f;
    const float M_supersonic = 1.2f;
    
    if (mach_number < M_crit) {
        // Subsonic, no additional effects
        return;
    }
    
    if (mach_number <= M_supersonic) {
        // Transonic rise: cubic growth for drag
        float delta_M = mach_number - M_crit;
        float drag_rise = 2.0f * std::pow(delta_M, 3.0f); // k * (M - 0.8)^3
        drag_coefficient += drag_rise;
        
        // Mach tuck: nose-down moment
        float mach_tuck = delta_M * 0.1f;
        pitch_moment += mach_tuck;
    } else {
        // Supersonic: wave drag decay
        float epsilon = 1e-6f;
        float denominator = std::max(epsilon, mach_number * mach_number - 1.0f);
        float wave_drag = 0.5f / std::sqrt(denominator); // Simplified 1/sqrt(M^2 - 1)
        drag_coefficient += wave_drag;
        
        // Reduced pitch moment in supersonic
        pitch_moment *= 0.8f;
    }
}

void FlightDynamicsSystem::model_compressibility_drag(
    float mach,
    float& cl,
    float& cd
) {
    // Model compressibility effects on drag
    
    // Compressibility drag starts around M=0.7 and increases to M=1.0
    // Formula: CD_comp = k * (M^2 - 1)^2 * CL^2 or similar empirical model
    if (mach > 0.7f) {
        float mach_factor = (mach * mach - 1.0f);
        if (mach_factor > 0.0f) {
            float compressibility_drag = 0.1f * mach_factor * mach_factor * cl * cl;
            cd += compressibility_drag;
        }
    }
}

void FlightDynamicsSystem::simulate_stall_recovery_procedures() {
    float recovery_gain = 0.15f;
    float pitch_trim = -0.05f;
    float roll_damping = 0.02f;
    (void)recovery_gain;
    (void)pitch_trim;
    (void)roll_damping;
}

void FlightDynamicsSystem::simulate_rotor_blade_flapping(
    float collective,
    float cyclic
) {
    float flapping_angle = (collective + cyclic * 0.5f) * 0.08f;
    float coning_effect = std::min(0.25f, std::abs(flapping_angle));
    (void)coning_effect;
}

void FlightDynamicsSystem::apply_ground_effect(float altitude_m, float& lift_coefficient) {
    // Ground effect calculations for fixed-wing aircraft
    
    // Ground effect increases lift near the ground
    // CL_multiplier = 1 + k * (wingspan / altitude) for altitude < wingspan
    // Typically 10-20% increase at altitude = wingspan/4
    
    // Simplified piecewise linear model
    if (altitude_m < 10.0f) { // Assume wingspan ~10m for small aircraft
        float wingspan = 10.0f; // Should come from aircraft properties
        float height_ratio = altitude_m / wingspan;
        float lift_increase = 0.0f;
        
        if (height_ratio < 0.25f) {
            lift_increase = 0.2f; // 20% increase very close to ground
        } else if (height_ratio < 1.0f) {
            lift_increase = 0.2f * (1.0f - height_ratio / 0.25f * 0.75f); // Linear decrease
        }
        
        lift_coefficient *= (1.0f + lift_increase);
    }
}

void FlightDynamicsSystem::model_vortex_ring_state(
    float descent_rate
) {
    if (descent_rate < -5.0f) {
        float vortex_loss = std::min(0.7f, (-descent_rate - 5.0f) * 0.05f);
        (void)vortex_loss;
    }
}

void FlightDynamicsSystem::simulate_retreating_blade_stall(
    float airspeed
) {
    if (airspeed > 50.0f) {
        float stall_margin = airspeed - 50.0f;
        float roll_instability = std::min(0.25f, stall_margin * 0.001f);
        (void)roll_instability;
    }
}

void FlightDynamicsSystem::simulate_fly_by_wire_systems(
    const ControlInputs& inputs
) {
    float roll_limiter = std::clamp(inputs.stick_roll, -0.8f, 0.8f);
    float pitch_limiter = std::clamp(inputs.stick_pitch, -0.7f, 0.7f);
    (void)roll_limiter;
    (void)pitch_limiter;
}

void FlightDynamicsSystem::model_stability_augmentation(
    const AircraftState& state
) {
    alignas(32) float angular_velocity[8];
    _mm256_store_ps(angular_velocity, state.angular_velocity);
    float angular_yaw = angular_velocity[2];
    float angular_pitch = angular_velocity[1];
    float yaw_damping = -0.1f * angular_yaw;
    float pitch_damping = -0.05f * angular_pitch;
    (void)yaw_damping;
    (void)pitch_damping;
}

// Helicopter advanced methods implementation
float FlightDynamicsSystem::compute_induced_velocity(float thrust, float disk_area, float vertical_speed) {
    const float rho = 1.225f; // Air density
    float term = vertical_speed / 2.0f;
    float discriminant = term * term + thrust / (2.0f * rho * disk_area);
    
    if (discriminant < 0.0f) return 0.0f; // Invalid physics
    return -term + std::sqrt(discriminant);
}

void FlightDynamicsSystem::model_vortex_ring_state(HelicopterState& heli, float descent_rate, float horiz_speed) {
    // Vortex Ring State conditions: descent > 5 m/s, horizontal speed < 10 m/s
    if (descent_rate > 5.0f && horiz_speed < 10.0f) {
        heli.vortex_ring_state = true;
        // Reduce rotor effectiveness by 70%
        heli.induced_velocity *= 0.3f;
        // Increase vibrations
        heli.vibration_level = std::min(1.0f, heli.vibration_level + 0.1f);
    } else {
        heli.vortex_ring_state = false;
        // Dampen vibrations
        heli.vibration_level *= 0.95f;
    }
}

void FlightDynamicsSystem::simulate_autorotation(HelicopterState& heli, float engine_torque, float airflow_up) {
    if (engine_torque <= 0.0f) {
        heli.autorotation_active = true;
        // Rotor accelerates due to upward airflow
        float target_rpm = airflow_up * 0.1f; // Simplified constant
        heli.autorotation_rpm += (target_rpm - heli.autorotation_rpm) * 0.1f; // Inertia
        heli.rotor_rpm = heli.autorotation_rpm;
    } else {
        heli.autorotation_active = false;
        heli.autorotation_rpm = 0.0f;
    }
}

void FlightDynamicsSystem::simulate_blade_damage(HelicopterState& heli, float damage_amount) {
    // Damage reduces lift coefficient, causing imbalance
    // Assuming blade_profile_ is accessible, but since it's static, we'll simulate
    if (damage_amount > 0.0f) {
        // Reduce collective effectiveness
        heli.collective_pitch *= (1.0f - damage_amount * 0.5f);
        // Increase vibrations
        heli.vibration_level = std::min(1.0f, heli.vibration_level + damage_amount);
    }
}

}  // namespace physics_core
