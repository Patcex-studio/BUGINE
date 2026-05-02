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

#include <vector>
#include <cstdint>
#include <immintrin.h>
#include <array>
#include "vehicle_component.h"
#include "ballistics_system.h"
#include "hybrid_precision.h"
#include "types.h"

namespace physics_core {

// ============================================================================
// DAMAGE STATE STRUCTURES
// ============================================================================

// Component damage state (target: < 256 bytes per component)
struct ComponentDamageState {
    // Entity identification
    EntityID component_entity;                  // Associated component entity
    
    // Health tracking
    float current_health;                       // Current health (0.0-1.0)
    float max_health;                          // Maximum health
    
    // Damage type flags
    uint32_t damage_type_flags;                 // PENETRATION, FIRE, EXPLOSION, etc.
    
    // Structural integrity
    float structural_integrity;                 // Structural damage (0.0-1.0)
    float armor_thickness;                     // Current effective armor thickness (mm)
    float armor_hardness;                      // Material hardness factor (1.0 = RHA)
    
    // Fatigue and weld damage
    float fatigue_damage;                      // Accumulated fatigue (0.0-100.0+)
    float weld_integrity;                      // Weld integrity (1.0 = intact, 0.0 = broken)
    
    // Damage status
    uint32_t damage_level;                     // 0-5 damage severity levels
    float cumulative_stress;                   // Cumulative stress from damage
    
    // Functional status
    uint32_t status_flags;                     // DESTROYED, DISABLED, ON_FIRE, etc.
    
    // Timing
    float last_damage_time;                    // Time of last damage event (seconds)
    float fire_start_time;                     // When fire started (for duration)
    
    // Impact data
    __m256 damage_position;                    // Local position of last hit (x,y,z,0)
    __m256 damage_normal;                      // Surface normal at impact (x,y,z,0)
    
    // Statistics
    float total_damage_received;                // Total cumulative damage
    uint32_t hit_count;                        // Number of hits sustained
    
    // Additional armor system data
    float reactive_tile_coverage;              // ERA tile coverage percentage
    bool reactive_tiles_triggered;             // Have ERA tiles detonated
    
    // Padding to reach exactly 256 bytes
    uint32_t _padding[2];
};

// Vehicle damage composition
struct VehicleDamageState {
    // Vehicle identification
    EntityID vehicle_entity;                    // Root vehicle entity
    
    // Component damage states (SIMD-friendly: 8 components per batch)
    std::vector<ComponentDamageState> component_damage_states;
    
    // Overall vehicle status
    float overall_mobility;                    // Overall vehicle mobility (0.0-1.0)
    float overall_firepower;                   // Overall vehicle firepower (0.0-1.0)
    float overall_survivability;               // Overall vehicle survivability (0.0-1.0)
    
    // Critical system tracking
    uint32_t critical_systems_lost;            // Count of lost critical systems
    uint32_t mobility_contributing_damage;     // Components damaged affecting mobility
    uint32_t firepower_contributing_damage;    // Components damaged affecting firepower
    
    // Cumulative damage
    float cumulative_damage;                   // Total damage taken (0.0-1.0)
    float cumulative_fire_damage;              // Total fire damage accumulated
    
    // Vehicle status flags
    uint32_t status_flags;                     // DESTROYED, COMBAT_EFFICIENT, etc.
    
    // Combat effectiveness
    bool is_vehicularly_destroyed;             // Vehicle completely destroyed
    bool is_combat_efficient;                  // Still combat effective
    bool is_immobilized;                       // Cannot move
    bool is_gun_disabled;                      // Cannot fire
    
    // Damage distribution
    __m256 center_of_damage;                   // Center of damage distribution (x,y,z,0)
    float asymmetric_damage_ratio;             // Ratio of damage on one side
    
    // Fire system state
    bool is_on_fire;                           // Vehicle is burning
    float fire_intensity;                      // Current fire intensity (0.0-1.0)
    float fire_spread_rate;                    // Rate of fire spread (m/s)
    
    // Ammo system state
    bool ammo_stowage_compromised;             // Can potentially cook off
    uint32_t ammo_remaining;                   // Remaining ammunition count
    
    // Statistics
    float time_to_destruction_estimate;        // ETA to automatic destruction (seconds)
};

// Status flags for components
enum class ComponentStatusFlag : uint32_t {
    OPERATIONAL = 0,
    PARTIALLY_DAMAGED = 1 << 0,
    HEAVILY_DAMAGED = 1 << 1,
    DISABLED = 1 << 2,
    DESTROYED = 1 << 3,
    ON_FIRE = 1 << 4,
    AMMO_EXPLOSION_PENDING = 1 << 5,
};

// Status flags for vehicles
enum class VehicleStatusFlag : uint32_t {
    OPERATIONAL = 0,
    BATTLE_DAMAGED = 1 << 0,
    MOBILITY_IMPAIRED = 1 << 1,
    FIREPOWER_IMPAIRED = 1 << 2,
    SURVIVAL_IMPAIRED = 1 << 3,
    CRITICAL_DAMAGE = 1 << 4,
    ON_FIRE = 1 << 5,
    IMMOBILIZED = 1 << 6,
    GUN_DISABLED = 1 << 7,
    DESTROYED = 1 << 8,
};

// Damage type flags
enum class DamageTypeFlag : uint32_t {
    PENETRATION = 1 << 0,
    SPALLING = 1 << 1,
    FIRE = 1 << 2,
    EXPLOSION = 1 << 3,
    FRAGMENTATION = 1 << 4,
    THERMAL = 1 << 5,
    SHOCK = 1 << 6,
};

// Damage event for propagation
struct DamageEvent {
    EntityID vehicle_id;
    EntityID component_id;
    float damage_amount;                       // 0.0-1.0 normalized
    uint32_t damage_type;                      // DamageTypeFlag
    __m256 impact_position;                    // World space impact position
    __m256 impact_velocity;                    // Impact velocity vector
    uint32_t timestamp_ms;                     // Event timestamp
    float impulse_magnitude;                   // Force of impact (Newtons)
};

// ============================================================================
// ADVANCED BALLISTIC STRUCTURES
// ============================================================================

// Projectile characteristics (enhanced from spec)
struct ProjectileCharacteristics {
    // Basic properties
    uint32_t projectile_type;                  // APFSDS, HEAT, HESH, etc.
    float caliber_mm;                          // Projectile caliber
    float mass_kg;                             // Projectile mass
    float velocity_ms;                         // Muzzle velocity
    
    // Aerodynamic properties
    float drag_coefficient;                    // Aerodynamic drag
    float ballistic_coefficient;               // Ballistic coefficient
    
    // Penetrator/warhead specific
    float penetrator_length_mm;                // Long rod penetrator length
    float shaped_charge_diameter_mm;           // HEAT warhead diameter
    float explosive_mass_kg;                   // TNT equivalent
    
    // Fuse and detonation
    float fuse_delay_ms;                       // Delayed fuse timing
    uint32_t fuse_type;                        // IMPACT, TIMED, PROXIMITY
    
    // Guidance system
    bool has_guidance;                         // Laser/GPS guidance
    float guidance_accuracy_m;                 // CEP (Circular Error Probable)
    
    // Material properties
    float armor_piercing_constant;             // De Marre constant
    float penetration_efficiency;              // Penetration efficiency factor
        // Derived geometry and material properties
    float density_kg_m3;                       // Projectile density for hydrodynamic penetration
    float length_mm;                           // Projectile effective length for APFSDS
    // Stability
    float stability_factor;                    // Aerodynamic stability (0.0-1.0)
};

// Armor characteristics (enhanced from spec)
struct ArmorCharacteristics {
    // Material basics
    uint32_t armor_type;                       // RHA, CHA, FHA, COMPOSITE, REACTIVE
    float thickness_mm;                        // Armor thickness
    float hardness_rha;                        // Hardness relative to RHA
    
    // Physical properties
    float density_kg_m3;                       // Material density
    float yield_strength_mpa;                  // Yield strength
    float elastic_modulus_gpa;                 // Elastic modulus
    
    // Performance characteristics
    float spall_resistance;                    // Resistance to spalling (0.0-1.0)
    float ricochet_threshold;                  // Critical angle for ricochet (degrees)
    float ductility_factor;                    // Ductility for deformation (0.0-1.0)
    
    // Composite/ERA specific
    float ceramic_efficiency;                  // Ceramic layer effectiveness
    uint32_t composite_layers;                 // Number of composite layers
    float reactive_tile_thickness;             // ERA tile thickness (mm)
    float reactive_explosive_mass;             // ERA explosive mass (kg TNT)
    float reactive_activation_threshold;       // Threshold for ERA activation
    float reactive_coverage_percent;           // Coverage percentage (0-100%)
    
    // Environmental factors
    float heat_degradation_factor;             // Heat effects on armor
    float moisture_absorption_factor;          // Moisture effects (for ERA)
};

// Ballistic impact results
struct BallisticImpactResult {
    // Penetration assessment
    bool is_penetrated;                        // Did projectile penetrate?
    bool is_ricocheted;                        // Did projectile ricochet?
    bool caused_spalling;                      // Did spalling occur?
    bool activated_era;                        // Did ERA activate?
    
    // Depth measurements
    float penetration_depth_mm;                // Actual penetration depth
    float residual_velocity_ms;                // Velocity after armor
    float spall_velocity_ms;                   // Spall fragment velocity
    
    // Energy calculations
    float damage_energy_joules;                // Energy transferred to target
    float armor_damage_mm;                     // Damage to armor itself
    float heat_generated_joules;               // Heat generated during impact
    
    // Spatial data
    __m256 impact_position;                    // World space impact position
    __m256 impact_normal;                      // Surface normal at impact
    __m256 exit_position;                      // Exit position (if penetrated)
    __m256 exit_normal;                        // Exit surface normal
    
    // Impact parameters
    float impact_angle_deg;                    // Angle of impact
    float normalized_impact_angle;             // Normalized to effective armor angle
    float time_to_impact_ms;                   // Time from firing to impact
    
    // Secondary effects
    uint32_t fragment_count;                   // Number of fragments generated
    float fragment_kinetic_energy;             // Total kinetic energy of fragments
    float blast_radius_m;                      // Blast effect radius
    float fire_probability;                    // Probability of starting fires
};

// ============================================================================
// SECONDARY EFFECTS STRUCTURES
// ============================================================================

// Fire propagation system
struct FirePropagationState {
    EntityID fire_source;                      // Source component
    EntityID current_container;                // Current component burning
    
    float fire_intensity;                      // Fire intensity (0.0-1.0)
    float burn_rate;                           // Rate of spread (m/s)
    float temperature_kelvin;                  // Current temperature
    float burn_duration;                       // Duration of burning (seconds)
    
    bool is_extinguishable;                    // Can be extinguished
    float extinction_time;                     // Natural extinction time (seconds)
    float suppression_effectiveness;           // How well suppression works (0.0-1.0)
    
    uint32_t propagated_to_count;              // Number of components affected
    std::vector<EntityID> adjacent_components; // Connected components
};

// Fire propagation node
struct FirePropagationNode {
    EntityID target_component;                 // Component being ignited
    float ignition_probability;                // Chance of ignition (0.0-1.0)
    float heat_transfer_rate;                  // Rate of heat transfer (joules/s)
    float burn_duration;                       // Duration if ignited (seconds)
    float distance_to_source;                  // Distance from fire source (m)
};

// Explosion effect structure
struct ExplosionEffect {
    // Source information
    EntityID explosion_source;                 // Component that exploded
    uint32_t explosion_type;                   // AMMO, FUEL, ARMOR_PIERCING
    
    // Spatial data
    __m256 explosion_center;                   // World space explosion center
    float explosion_radius_m;                  // Blast radius
    
    // Energy calculations
    float explosive_yield_kg_tnt;              // TNT equivalent yield
    float yield_joules;                        // Total energy released (joules)
    float blast_pressure_pa;                   // Peak blast pressure (Pa)
    float blast_impulse;                       // Blast impulse (Pa·s)
    
    // Fragmentation
    uint32_t fragmentation_count;              // Number of fragments
    float fragmentation_velocity_ms;           // Average fragment velocity
    
    // Effects
    float fire_chance;                         // Probability of starting fires (0.0-1.0)
    float structural_damage;                   // Damage to surrounding structures
    
    // Timing
    float explosion_delay_ms;                  // Delay before explosion (for cook-off)
};

// Fragment data structure
struct Fragment {
    __m256 velocity;                           // Fragment velocity vector
    float mass_kg;                             // Fragment mass
    float penetration_power;                   // Ability to penetrate (0.0-1.0)
    EntityID target_component;                 // Component hit by fragment
    float time_to_target;                      // Time to reach target (ms)
    float impact_energy_joules;                // Energy at impact
};

// ============================================================================
// CREW DAMAGE SYSTEM
// ============================================================================

// Injury types enumeration
enum class InjuryType : uint32_t {
    MINOR_LACERATION = 0,
    SEVERE_LACERATION = 1,
    BONE_FRACTURE = 2,
    INTERNAL_BLEEDING = 3,
    BURN = 4,
    CONCUSSION = 5,
    SHRAPNEL_WOUND = 6,
    CRUSHING_INJURY = 7,
    THERMAL_SHOCK = 8,
    BLAST_TRAUMA = 9,
    COUNT
};

// Detailed injury structure
struct Injury {
    InjuryType type;
    float severity;        // Injury severity (0.0-1.0)
    float blood_loss_rate; // Blood loss rate (ml/s)
    float pain_factor;     // Pain factor (0.0-1.0)
};

// Deterministic RNG for reproducible simulations
class DeterministicRNG {
    uint64_t state_;
public:
    explicit DeterministicRNG(uint64_t seed) : state_(seed) {}
    
    // LCG — fast and deterministic
    float next_float() noexcept {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>((state_ >> 32) & 0xFFFFFF) / 16777216.0f;
    }
    
    // Seed = entity_id ^ frame_id for reproducibility
    static uint64_t make_seed(EntityID entity_id, uint64_t frame_id) {
        return static_cast<uint64_t>(entity_id) ^ (frame_id << 16);
    }
};

// Crew member damage state
struct CrewDamageState {
    EntityID crew_member_entity;               // Individual crew member
    uint32_t crew_position;                    // COMMANDER, GUNNER, DRIVER, LOADER
    
    // Injury tracking
    float injury_severity;                     // Injury level (0.0-1.0)
    std::vector<Injury> injuries;              // Specific injuries sustained
    uint32_t injury_count;                     // Total number of injuries
    
    // Status flags
    bool is_killed;                            // Crew member killed
    bool is_wounded;                           // Crew member wounded
    bool is_unconscious;                       // Crew member unconscious
    bool is_incapacitated;                     // Unable to perform duties
    
    // Medical parameters
    float blood_volume;                        // Current blood volume (ml)
    float blood_loss_rate;                     // Blood loss rate (ml/s)
    float consciousness_level;                 // Consciousness (0.0-1.0)
    float pain_level;                          // Pain level (0.0-1.0)
    
    // Survival
    float survival_chance;                     // Probability of survival (0.0-1.0)
    float time_to_death_estimate;              // ETA to death if untreated (seconds)
    
    // Shock damage
    float shock_damage;                        // Shock damage level (0.0-1.0)
    
    // Treatment
    bool has_medical_kit;                      // Has medical kit available
    float treatment_progress;                  // Treatment progress (0.0-1.0)
    float time_until_death;                    // Time until death if untreated
};

// ============================================================================
// DAMAGE SYSTEM CLASS
// ============================================================================

class DamageSystem {
public:
    // ========================================================================
    // MAIN DAMAGE PROCESSING FUNCTIONS
    // ========================================================================
    
    // Calculate ballistic impact and apply damage
    static BallisticImpactResult calculate_ballistic_impact(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        const ImpactParameters& impact_params,
        BallisticImpactResult& result
    );
    
    // Apply damage to component
    static void apply_damage_to_component(
        EntityID component_entity,
        const BallisticImpactResult& impact,
        ComponentDamageState& component_state
    );
    
    // Apply damage to vehicle (coordination function)
    static void apply_damage_to_vehicle(
        EntityID vehicle_entity,
        const BallisticImpactResult& impact,
        VehicleDamageState& vehicle_state
    );
    
    // Process secondary effects cascade
    static void process_secondary_effects_cascade(
        const ExplosionEffect& explosion,
        std::vector<DamageEvent>& cascade_events
    );
    
    // ========================================================================
    // BALLISTIC CALCULATION FUNCTIONS
    // ========================================================================
    
    // Core penetration calculation (< 100 μs target)
    static bool calculate_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );
    
    // APFSDS (Armor-Piercing Fin-Stabilized Discarding Sabot)
    static bool simulate_apfsds_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        BallisticImpactResult& result
    );
    
    // HEAT (High-Explosive Anti-Tank)
    static bool simulate_heat_penetration(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float standoff_distance,
        float velocity_ms,
        BallisticImpactResult& result
    );
    
    // HESH (High-Explosive Squash Head)
    static bool simulate_hesh_spalling(
        const ProjectileCharacteristics& projectile,
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        BallisticImpactResult& result
    );
    
    // Ricochet calculation
    static bool calculate_ricochet(
        const ArmorCharacteristics& armor,
        float impact_angle_deg,
        float velocity_ms,
        __m256& ricochet_direction,
        float& ricochet_velocity_ms
    );
    
    // Spalling calculation
    static void calculate_spalling_damage(
        const ArmorCharacteristics& armor,
        const BallisticImpactResult& impact,
        std::vector<Fragment>& spall_fragments
    );
    
    // ========================================================================
    // SECONDARY EFFECTS FUNCTIONS
    // ========================================================================
    
    // Fire propagation simulation
    static void simulate_fire_propagation(
        const FirePropagationState& fire_state,
        std::vector<FirePropagationNode>& propagation_targets,
        float delta_time
    );
    
    // Explosion effect calculation
    static void calculate_explosion_effects(
        const ExplosionEffect& explosion,
        std::vector<Fragment>& fragments,
        std::vector<DamageEvent>& damage_events
    );
    
    // Ammunition cook-off simulation
    static void simulate_ammo_cook_off(
        EntityID ammo_storage_entity,
        const ComponentDamageState& ammo_component,
        ExplosionEffect& cookoff_explosion
    );
    
    // Fuel system damage simulation
    static void simulate_fuel_system_damage(
        EntityID fuel_system_entity,
        const ComponentDamageState& fuel_component,
        const BallisticImpactResult& impact,
        FirePropagationState& fuel_fire
    );
    
    // ========================================================================
    // COMPONENT AND VEHICLE STATUS FUNCTIONS
    // ========================================================================
    
    // Update component health and status
    static void update_component_health(
        ComponentDamageState& component,
        float damage_amount,
        uint32_t damage_type
    );
    
    // Check for critical failures
    static bool check_critical_failure(
        const ComponentDamageState& component,
        ComponentType type
    );
    
    // Calculate vehicle status from all components
    static void calculate_vehicle_status(
        const std::vector<ComponentDamageState>& components,
        VehicleDamageState& vehicle_state
    );
    
    // ========================================================================
    // CREW DAMAGE FUNCTIONS
    // ========================================================================
    
    // Apply damage to crew members
    static void apply_crew_damage_from_vehicle_damage(
        const VehicleDamageState& vehicle_damage,
        std::vector<CrewDamageState>& crew_states
    );
    
    // Process crew injuries
    static void process_crew_injuries(
        CrewDamageState& crew_member,
        float delta_time
    );
    
    // ========================================================================
    // SIMD BATCH PROCESSING
    // ========================================================================
    
    // Batch process multiple impacts (8 impacts per AVX2 operation)
    static void process_ballistic_impacts_batch(
        const std::vector<ProjectileCharacteristics>& projectiles,
        const std::vector<ArmorCharacteristics>& armors,
        const std::vector<ImpactParameters>& impacts,
        std::vector<BallisticImpactResult>& results
    );
    
    // ========================================================================
    // PHYSICS INTEGRATION
    // ========================================================================
    
    // Apply damage forces to physics
    static void apply_damage_forces_to_physics(
        EntityID damaged_entity,
        const BallisticImpactResult& impact,
        class PhysicsCore& physics
    );
    
    // Apply impulse from impact
    static void apply_ballistic_impulse(
        const BallisticImpactResult& impact,
        class LocalPhysicsBody& target_body
    );
    
private:
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Calculate effective armor thickness based on angle
    static float calculate_effective_armor_thickness(
        float base_thickness,
        float impact_angle_deg
    );
    
    // Calculate penetration capability using De Marre formula
    static float calculate_de_marre_penetration(
        float projectile_mass_kg,
        float projectile_velocity_ms,
        float projectile_diameter_mm,
        float armor_hardness_factor
    );
    
    // Calculate heat penetration (HEAT)
    static float calculate_heat_penetration(
        float shaped_charge_diameter_mm,
        float standoff_distance,
        float armor_thickness_mm
    );
    
    // Calculate damage multiplier for component type
    static float calculate_damage_multiplier(
        uint32_t damage_type,
        ComponentType component_type
    );
    
    // Check if damage causes fire
    static bool check_fire_initiation(
        const BallisticImpactResult& impact,
        ComponentType component_type
    );
    
    // Check if damage causes ammunition cook-off
    static bool check_ammo_cookoff(
        const ComponentDamageState& ammo_component
    );
    
    // Generate spalling damage events
    static void generate_spalling_effects(
        EntityID hit_component_id,
        const BallisticImpactResult& impact,
        std::vector<Fragment>& spall_fragments
    );
    
    // Update component functionality based on damage
    static void update_component_functionality(
        ComponentDamageState& component,
        ComponentType type
    );
};

} // namespace physics_core