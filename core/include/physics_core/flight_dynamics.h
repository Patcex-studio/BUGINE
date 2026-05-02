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

#include <immintrin.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "physics_body.h"

namespace physics_core {

// ============================================================================
// Flight Dynamics Data Structures
// ============================================================================

enum AircraftType : uint32_t {
    AIRCRAFT_FIGHTER = 0,
    AIRCRAFT_BOMBER = 1,
    AIRCRAFT_TRANSPORT = 2,
    AIRCRAFT_HELICOPTER = 3,
    AIRCRAFT_UAV = 4,
};

enum FlightPhase : uint32_t {
    FLIGHT_TAKEOFF = 0,
    FLIGHT_CRUISE = 1,
    FLIGHT_COMBAT = 2,
    FLIGHT_LANDING = 3,
};

enum ControlSurfaceType : uint32_t {
    AILERON = 0,
    ELEVATOR = 1,
    RUDDER = 2,
    FLAP = 3,
    SPOILER = 4,
    SLAT = 5,
};

enum EngineType : uint32_t {
    JET_TURBOFAN = 0,
    PISTON_RADIAL = 1,
    TURBOPROP = 2,
    ROCKET = 3,
};

static constexpr size_t FLIGHT_SIMD_BATCH = 8;
static constexpr float SPEED_OF_SOUND_MPS = 340.29f;
static constexpr float GRAVITY_MS2 = 9.80665f;

struct alignas(32) AircraftState {
    __m256 world_position;          // World position xyz0 (float32)
    __m256 orientation_quaternion;   // Quaternion wxyz
    __m256 linear_velocity;          // Linear velocity in world space
    __m256 linear_acceleration;      // Linear acceleration
    __m256 angular_velocity;         // Roll, pitch, yaw rates
    __m256 angular_acceleration;     // Angular acceleration

    float mass_kg;                  // Total aircraft mass
    float inertia_xx;               // Moment of inertia tensor diagonal
    float inertia_yy;
    float inertia_zz;

    float airspeed_ms;              // True airspeed
    float altitude_m;               // Altitude above sea level
    float angle_of_attack_deg;       // Angle of attack
    float sideslip_angle_deg;        // Sideslip angle

    uint32_t aircraft_type;         // FIGHTER, BOMBER, TRANSPORT, HELICOPTER
    uint32_t flight_phase;          // TAKEOFF, CRUISE, COMBAT, LANDING
    EntityID entity_id;             // ECS entity reference
};

struct ControlSurface {
    char surface_name[32];          // Human-readable name
    uint32_t surface_type;          // AILERON, ELEVATOR, RUDDER, FLAP, SPOILER, SLAT
    float current_deflection_rad;    // Current deflection angle (-π/2 to π/2)
    float max_deflection_rad;       // Maximum mechanical deflection
    float min_deflection_rad;       // Minimum mechanical deflection
    float effectiveness_coeff;      // Aerodynamic effectiveness coefficient
    float surface_area_m2;          // Surface area in square meters
    __m256 local_position;          // Position relative to aircraft CG
    __m256 local_normal;            // Surface normal vector
    bool is_damaged;                // Battle damage status
    float damage_effectiveness;     // Reduced effectiveness due to damage
};

struct AircraftControlSystem {
    ControlSurface ailerons[2];     // Left and right ailerons
    ControlSurface elevators[2];    // Left and right elevators
    ControlSurface rudder;          // Single rudder

    ControlSurface flaps[2];        // Inner and outer flaps
    ControlSurface spoilers[4];      // Roll spoilers (modern fighters)
    ControlSurface slats[2];        // Leading edge slats

    float collective_pitch;         // Collective pitch control
    float cyclic_pitch_x;           // Longitudinal cyclic
    float cyclic_pitch_y;           // Lateral cyclic
    float tail_rotor_pitch;         // Tail rotor pitch

    float stick_roll_input;         // Pilot roll input (-1.0 to 1.0)
    float stick_pitch_input;        // Pilot pitch input (-1.0 to 1.0)
    float rudder_input;             // Rudder pedal input (-1.0 to 1.0)
    float throttle_input;           // Throttle input (0.0 to 1.0)
};

struct EngineProperties {
    uint32_t engine_type;           // JET_TURBOFAN, PISTON_RADIAL, TURBOPROP, ROCKET
    char engine_name[64];           // Engine model name
    float max_thrust_sea_level_N;    // Maximum thrust at sea level (Newtons)
    float max_power_sea_level_hp;    // Maximum power at sea level (horsepower)
    float thrust_curve[11];         // Thrust vs altitude (0-10km in 1km steps)
    float fuel_consumption_rate;     // Fuel consumption kg/s per thrust unit
    float spool_up_time_seconds;     // Time to spool up to full power
    float spool_down_time_seconds;   // Time to spool down to idle
    bool has_afterburner;           // Afterburner capability
    float afterburner_thrust_mult;   // Afterburner thrust multiplier
    bool has_thrust_reverser;       // Thrust reverser capability
    float compressor_stall_limit;     // Critical AoA for compressor stall
    float max_rpm;                  // Maximum engine RPM
};

struct EngineState {
    float current_thrust_N;          // Current thrust output
    float current_power_hp;          // Current power output
    float fuel_remaining_kg;         // Remaining fuel in kg
    float engine_rpm;               // Current engine RPM percentage
    float exhaust_temperature_C;     // Exhaust gas temperature in Celsius
    bool afterburner_active;         // Afterburner status
    bool thrust_reverser_active;     // Thrust reverser status
    bool is_damaged;                // Battle damage status
    float damage_thrust_mult;        // Thrust reduction due to damage
    EntityID engine_entity;          // ECS entity reference
};

struct AerodynamicForces {
    float force_x;
    float force_y;
    float force_z;
    float moment_roll;
    float moment_pitch;
    float moment_yaw;
};

struct PropulsionForces {
    float thrust;
    float roll_moment;
    float pitch_moment;
    float yaw_moment;
};

struct StallConditions {
    float critical_aoa;             // Degrees for stall onset
    float stall_hysteresis;         // Hysteresis for recovery
    float spin_threshold;           // Yaw rate for spin entry
    bool is_stalled;                // Current stall state
    bool is_spinning;               // Current spin state
    float recovery_time;            // Time to recover from stall
};

struct ControlInputs {
    float throttle;
    float stick_pitch;
    float stick_roll;
    float rudder;
    float flaps;
};

struct MissionObjectives {
    float target_distance;
    float target_altitude;
    bool engage_target;
};

struct ModularAircraftBlueprint {
    // Placeholder for modular aircraft blueprint structure
    // Would contain component definitions, mass properties, etc.
    float total_mass_kg;
    float inertia_xx;
    float inertia_yy;
    float inertia_zz;
    // Add more fields as needed
};

struct NetworkAircraftState {
    float position_x, position_y, position_z;
    float velocity_x, velocity_y, velocity_z;
    float orientation_w, orientation_x, orientation_y, orientation_z;
    float airspeed;
    float altitude;
    float angle_of_attack;
    float sideslip_angle;
    uint32_t aircraft_type;
    uint32_t flight_phase;
};

// Helicopter-specific structures
struct alignas(16) BladeElement {
    float radius;           // Relative radius (0..1)
    float chord;            // Chord length
    float pitch_angle;      // Pitch angle (degrees)
    float lift_coeff;       // Lift coefficient
    float drag_coeff;       // Drag coefficient
};

struct HelicopterState {
    // Rotor parameters
    float rotor_rpm;
    float collective_pitch; // Collective pitch (degrees)
    float cyclic_lat;       // Lateral cyclic (degrees)
    float cyclic_lon;       // Longitudinal cyclic (degrees)
    
    // Aerodynamic effects
    float induced_velocity; // Induced velocity (m/s)
    bool vortex_ring_state; // Vortex ring state flag
    float vibration_level;  // Vibration level (0..1)
    
    // Autorotation
    bool autorotation_active;
    float autorotation_rpm;
};

class PropulsionSystem {
public:
    static void update_engine_performance(
        EngineState& engine_state,
        const EngineProperties& engine_props,
        const AircraftState& aircraft_state,
        float throttle_input,
        float delta_time
    );

    static float sample_thrust_curve(const float* thrust_curve, float altitude_meters);
    static float atmospheric_density(float altitude_meters);
    static float thrust_altitude_multiplier(float altitude_meters);
};

class FlightDynamicsSystem {
public:
    static void calculate_aerodynamics_simd(
        const AircraftState* states,
        const AircraftControlSystem* controls,
        AerodynamicForces* output,
        size_t count
    );

    static void integrate_6dof_simd(
        AircraftState* states,
        const AerodynamicForces* forces,
        const PropulsionForces* propulsion,
        float dt,
        size_t count
    );

    static void calculate_aerodynamics(
        const AircraftState& state,
        const AircraftControlSystem& controls,
        AerodynamicForces& output_forces
    );

    static void integrate_6dof_aircraft(
        AircraftState& state,
        const AerodynamicForces& aerodynamics,
        const PropulsionForces& propulsion,
        float delta_time
    );

    static void calculate_transonic_effects(float mach_number, float& drag_coefficient, float& pitch_moment);
    static void model_compressibility_drag(float mach_number, float& cl, float& cd);
    static void apply_ground_effect(float altitude_m, float& lift_multiplier);

    static void detect_stall_spin(
        const AircraftState& state,
        const ControlInputs& inputs,
        StallConditions& conditions
    );

    static void apply_flight_forces_to_physics(
        EntityID aircraft_entity,
        const AircraftState& state,
        const AerodynamicForces& aerodynamics,
        const PropulsionForces& propulsion,
        class PhysicsCore& physics
    );

    static void initialize_from_modular_aircraft(
        EntityID aircraft_entity,
        const ModularAircraftBlueprint& blueprint,
        class PhysicsCore& physics
    );

    // Fixed-wing specific functions
    static void simulate_fixed_wing_dynamics(const AircraftState& state);
    static void simulate_stall_recovery_procedures();

    // Rotary-wing specific functions
    static void simulate_rotor_blade_flapping(float collective, float cyclic);
    static void calculate_ground_effect(float altitude);
    static void model_vortex_ring_state(float descent_rate);
    static void simulate_retreating_blade_stall(float airspeed);

    // Helicopter advanced methods
    static float compute_induced_velocity(float thrust, float disk_area, float vertical_speed);
    static void model_vortex_ring_state(HelicopterState& heli, float descent_rate, float horiz_speed);
    static void simulate_autorotation(HelicopterState& heli, float engine_torque, float airflow_up);
    static void simulate_blade_damage(HelicopterState& heli, float damage_amount);

    // Modern aircraft systems
    static void simulate_fly_by_wire_systems(const ControlInputs& inputs);
    static void model_stability_augmentation(const AircraftState& state);

    static void generate_ai_controls(
        const AircraftState& state,
        const MissionObjectives& objectives,
        ControlInputs& outputs
    );
};

}  // namespace physics_core
