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
#include "assembly_system/universal_assembly_engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace assembly_system {

UniversalAssemblyEngine::UniversalAssemblyEngine(ecs::EntityManager* entity_manager)
    : entity_manager_(entity_manager)
    , modular_system_(entity_manager) {
}

void UniversalAssemblyEngine::set_entity_manager(ecs::EntityManager* entity_manager) {
    entity_manager_ = entity_manager;
    modular_system_.set_entity_manager(entity_manager);
}

bool UniversalAssemblyEngine::construct_vehicle(
    const AssemblyBlueprint& blueprint,
    const AssemblyConstructionOptions& options,
    EntityID& output_entity,
    AssemblyValidationResult& validation_result
) {
    validation_result = run_validation(blueprint);
    if (!validation_result.is_valid && options.require_historical_accuracy) {
        return false;
    }

    std::vector<ConstraintViolation> violations;
    if (!constraint_system_.validate_assembly_constraints(blueprint, violations)) {
        validation_result.is_valid = false;
        ValidationError error;
        error.error_type = 1;
        error.affected_component = 0;
        std::strncpy(error.description, "Constraint validation failed for assembly.", sizeof(error.description) - 1);
        error.is_fatal = true;
        validation_result.errors.push_back(error);
        return false;
    }

    VehicleBlueprint vehicle_blueprint;
    std::string error_message;
    if (!populate_vehicle_blueprint(blueprint, vehicle_blueprint, error_message)) {
        validation_result.is_valid = false;
        ValidationError error;
        error.error_type = 2;
        error.affected_component = 0;
        std::strncpy(error.description, error_message.c_str(), sizeof(error.description) - 1);
        error.is_fatal = true;
        validation_result.errors.push_back(error);
        return false;
    }

    if (!modular_system_.assemble_vehicle_from_blueprint(vehicle_blueprint, output_entity, error_message)) {
        validation_result.is_valid = false;
        ValidationError error;
        error.error_type = 3;
        error.affected_component = 0;
        std::strncpy(error.description, error_message.c_str(), sizeof(error.description) - 1);
        error.is_fatal = true;
        validation_result.errors.push_back(error);
        return false;
    }

    validation_result.is_valid = true;
    validation_result.stability_score = std::clamp(validation_result.stability_score, 0.0f, 1.0f);
    validation_result.performance_score = std::clamp(validation_result.performance_score, 0.0f, 1.0f);
    return true;
}

bool UniversalAssemblyEngine::validate_partial_assembly(
    const PartialAssembly& partial_assembly,
    AssemblyValidationResult& feedback
) const {
    AssemblyBlueprint blueprint;
    blueprint.name = "partial_validation";
    blueprint.components.reserve(partial_assembly.components.size());
    for (const auto& component : partial_assembly.components) {
        blueprint.components.push_back(component);
    }

    feedback = run_validation(blueprint);
    std::vector<ConstraintViolation> violations;
    if (!constraint_system_.validate_assembly_constraints(blueprint, violations)) {
        feedback.is_valid = false;
        for (const auto& violation : violations) {
            ValidationWarning warning;
            warning.warning_type = 1;
            warning.affected_component = violation.source_component_id;
            std::snprintf(warning.description, sizeof(warning.description), "Constraint violation between component %u and %u.", violation.source_component_id, violation.target_component_id);
            feedback.warnings.push_back(warning);
        }
    }
    return feedback.is_valid;
}

bool UniversalAssemblyEngine::validate_ptu_content(
    const AssemblyBlueprint& ptu_blueprint,
    PTUSecurityValidation& security_check,
    AssemblyValidationResult& validation_result
) const {
    validation_result = run_validation(ptu_blueprint);
    security_check.passed = true;

    if (ptu_blueprint.components.size() > 128) {
        security_check.passed = false;
        security_check.issues.emplace_back("PTU blueprint contains too many components.");
    }

    if (validation_result.performance_score < 0.2f) {
        security_check.passed = false;
        security_check.issues.emplace_back("PTU blueprint is likely unbalanced or exploitative.");
    }

    return security_check.passed;
}

bool UniversalAssemblyEngine::create_historical_blueprint_from_specs(
    const HistoricalVehicleSpecs& specs,
    AssemblyBlueprint& output_blueprint
) const {
    output_blueprint.name = std::string(specs.vehicle_name, strnlen(specs.vehicle_name, sizeof(specs.vehicle_name)));
    output_blueprint.components.clear();

    AssemblyComponentInstance hull_instance;
    hull_instance.instance_id = 1;
    hull_instance.definition_id = static_cast<uint32_t>(ComponentCategory::HULL);
    hull_instance.category = static_cast<uint32_t>(ComponentCategory::HULL);
    hull_instance.mass_kg = specs.weight_tons * 1000.0f;
    output_blueprint.components.push_back(hull_instance);

    AssemblyComponentInstance engine_instance;
    engine_instance.instance_id = 2;
    engine_instance.definition_id = static_cast<uint32_t>(ComponentCategory::ENGINE);
    engine_instance.category = static_cast<uint32_t>(ComponentCategory::ENGINE);
    engine_instance.mass_kg = std::max(200.0f, specs.engine_power_hp * 0.5f);
    output_blueprint.components.push_back(engine_instance);

    AssemblyComponentInstance weapon_instance;
    weapon_instance.instance_id = 3;
    weapon_instance.definition_id = static_cast<uint32_t>(ComponentCategory::WEAPON);
    weapon_instance.category = static_cast<uint32_t>(ComponentCategory::WEAPON);
    weapon_instance.mass_kg = 250.0f;
    output_blueprint.components.push_back(weapon_instance);

    AssemblyComponentInstance control_instance;
    control_instance.instance_id = 4;
    control_instance.definition_id = static_cast<uint32_t>(ComponentCategory::CONTROL);
    control_instance.category = static_cast<uint32_t>(ComponentCategory::CONTROL);
    control_instance.mass_kg = 120.0f;
    output_blueprint.components.push_back(control_instance);

    return true;
}

bool UniversalAssemblyEngine::instantiate_assembled_vehicle(
    const AssemblyBlueprint& validated_assembly,
    EntityID& output_entity
) {
    AssemblyConstructionOptions options;
    AssemblyValidationResult validation_result;
    return construct_vehicle(validated_assembly, options, output_entity, validation_result);
}

bool UniversalAssemblyEngine::serialize_assembly_for_network(
    const AssemblyBlueprint& assembly,
    NetworkAssemblyPacket& output_packet
) const {
    output_packet.version = 1;
    output_packet.component_ids.clear();
    output_packet.component_ids.reserve(assembly.components.size());
    for (const auto& component : assembly.components) {
        output_packet.component_ids.push_back(component.instance_id);
    }
    return true;
}

bool UniversalAssemblyEngine::apply_campaign_constraints(
    const CampaignRules& campaign_rules,
    AssemblyBlueprint& blueprint
) const {
    if (campaign_rules.resource_multiplier <= 0.0f) {
        return false;
    }
    for (auto& component : blueprint.components) {
        component.mass_kg *= campaign_rules.resource_multiplier;
    }
    return true;
}

bool UniversalAssemblyEngine::enforce_multiplayer_balance(
    const MultiplayerBalanceRules& balance_rules,
    AssemblyBlueprint& blueprint
) const {
    float total_power = 0.0f;
    float total_mass = 0.0f;
    for (const auto& component : blueprint.components) {
        total_mass += component.mass_kg;
        const AssemblyComponentDefinition* def = component_library_.find_definition(component.definition_id);
        if (def) {
            total_power += def->resources.power_draw_watts;
        }
    }

    if (total_mass > 50000.0f * balance_rules.max_mass_ratio) {
        return false;
    }
    if (total_power > 250000.0f * balance_rules.max_power_ratio) {
        return false;
    }
    return true;
}

bool UniversalAssemblyEngine::apply_advanced_customization(
    const AdvancedCustomization& customization,
    AssemblyBlueprint& blueprint
) const {
    if (!customization.allow_visual_variants) {
        for (auto& component : blueprint.components) {
            component.reliability = std::min(component.reliability, 1.0f);
        }
    }
    return true;
}

bool UniversalAssemblyEngine::populate_vehicle_blueprint(
    const AssemblyBlueprint& assembly,
    VehicleBlueprint& out_blueprint,
    std::string& error_message
) const {
    const AssemblyComponentDefinition* hull_def = nullptr;
    const AssemblyComponentDefinition* engine_def = nullptr;
    const AssemblyComponentDefinition* weapon_def = nullptr;
    const AssemblyComponentDefinition* control_def = nullptr;

    for (const auto& instance : assembly.components) {
        const AssemblyComponentDefinition* def = component_library_.find_definition(instance.definition_id);
        if (!def) {
            continue;
        }

        switch (static_cast<ComponentCategory>(instance.category)) {
            case ComponentCategory::HULL:
                hull_def = def;
                break;
            case ComponentCategory::ENGINE:
                engine_def = def;
                break;
            case ComponentCategory::WEAPON:
                weapon_def = def;
                break;
            case ComponentCategory::CONTROL:
                control_def = def;
                break;
            default:
                break;
        }
    }

    if (!hull_def || !engine_def || !weapon_def || !control_def) {
        error_message = "Assembly blueprint must contain hull, engine, weapon and control components.";
        return false;
    }

    out_blueprint.name = assembly.name.empty() ? "constructed_vehicle" : assembly.name;
    out_blueprint.hull = {};
    out_blueprint.hull.entity_id = 0;
    out_blueprint.hull.armor_thickness_front = hull_def->performance.armor_bonus * 100.0f;
    out_blueprint.hull.armor_thickness_side = hull_def->performance.armor_bonus * 80.0f;
    out_blueprint.hull.armor_thickness_rear = hull_def->performance.armor_bonus * 60.0f;
    out_blueprint.hull.armor_thickness_top = hull_def->performance.armor_bonus * 40.0f;
    out_blueprint.hull.armor_material_type = hull_def->component_subcategory;
    out_blueprint.hull.structural_integrity = clamp01(hull_def->performance.reliability_rating);
    out_blueprint.hull.local_bounds = hull_def->local_bounds;
    out_blueprint.hull.socket_count = std::min<uint32_t>(hull_def->attachment_point_count, static_cast<uint32_t>(HullComponent{}.attachment_sockets.size()));

    for (uint32_t i = 0; i < out_blueprint.hull.socket_count; ++i) {
        out_blueprint.hull.attachment_sockets[i].socket_type = hull_def->attachment_points[i].socket_type;
        out_blueprint.hull.attachment_sockets[i].compatible_types_mask = hull_def->attachment_points[i].compatible_types_mask;
        out_blueprint.hull.attachment_sockets[i].connection_strength = 1.0f;
    }

    out_blueprint.engine = {};
    out_blueprint.engine.entity_id = 0;
    out_blueprint.engine.max_power_hp = std::max(1.0f, engine_def->performance.mobility_bonus * 1000.0f);
    out_blueprint.engine.current_rpm = 600.0f;
    out_blueprint.engine.fuel_capacity_liters = std::max(1.0f, engine_def->resources.fuel_consumption_lph * 10.0f);
    out_blueprint.engine.fuel_current_liters = out_blueprint.engine.fuel_capacity_liters * 0.75f;
    out_blueprint.engine.engine_health = clamp01(engine_def->performance.reliability_rating);
    out_blueprint.engine.engine_type = static_cast<uint32_t>(EngineType::DIESEL);
    out_blueprint.engine.fuel_line_count = 1;
    out_blueprint.engine.fuel_lines[0].max_flow_lpm = std::max(1.0f, engine_def->resources.fuel_consumption_lph);
    out_blueprint.engine.fuel_lines[0].current_flow_lpm = out_blueprint.engine.fuel_lines[0].max_flow_lpm * 0.8f;

    out_blueprint.weapons = {};
    out_blueprint.weapons.entity_id = 0;
    out_blueprint.weapons.weapon_mount_count = 1;
    out_blueprint.weapons.active_weapon_index = 0;
    out_blueprint.weapons.ammunition_total = std::max(0.0f, weapon_def->resources.fuel_consumption_lph * 10.0f);
    out_blueprint.weapons.ammunition_current = out_blueprint.weapons.ammunition_total;
    out_blueprint.weapons.reload_time_seconds = std::max(1.0f, 10.0f - weapon_def->performance.weapon_accuracy * 8.0f);
    out_blueprint.weapons.ammo_type_count = 1;
    out_blueprint.weapons.ammo_types[0].ammo_id = weapon_def->component_id;
    out_blueprint.weapons.ammo_types[0].penetration = weapon_def->performance.weapon_accuracy * 600.0f;
    out_blueprint.weapons.ammo_types[0].explosive_mass = weapon_def->performance.mobility_bonus * 1.5f;
    out_blueprint.weapons.ammo_types[0].mass_kg = 25.0f;

    out_blueprint.control = {};
    out_blueprint.control.entity_id = 0;
    out_blueprint.control.steering_response = std::clamp(control_def->performance.mobility_bonus, 0.1f, 1.0f);
    out_blueprint.control.suspension_health = clamp01(control_def->performance.reliability_rating);
    out_blueprint.control.has_stabilization = true;
    out_blueprint.control.electronics_health = clamp01(control_def->performance.reliability_rating);
    out_blueprint.control.comms_operational = true;
    out_blueprint.control.sensor_count = 1;
    out_blueprint.control.sensors[0].sensor_id = 1000;
    out_blueprint.control.sensors[0].type = 0;
    out_blueprint.control.sensors[0].range_m = 1200.0f;
    out_blueprint.control.sensors[0].health = 1.0f;
    out_blueprint.control.control_link_count = 1;
    out_blueprint.control.control_links[0].type = static_cast<uint32_t>(ConnectionType::ELECTRICAL);
    out_blueprint.control.control_links[0].responsiveness = out_blueprint.control.steering_response;

    return true;
}

AssemblyValidationResult UniversalAssemblyEngine::run_validation(const AssemblyBlueprint& blueprint) const {
    AssemblyValidationResult result;
    result.is_valid = true;
    result.stability_score = 0.5f;
    result.performance_score = 0.5f;
    result.historical_accuracy = 0.5f;

    float stability_score = 0.0f;
    if (validation_engine_.validate_physical_stability(blueprint, stability_score)) {
        result.stability_score = stability_score;
    }

    float performance_score = 0.0f;
    if (validation_engine_.validate_performance_requirements(blueprint, performance_score)) {
        result.performance_score = performance_score;
    }

    float historical_accuracy = 0.0f;
    if (validation_engine_.validate_historical_authenticity(blueprint, historical_accuracy)) {
        result.historical_accuracy = historical_accuracy;
    }

    if (result.stability_score < 0.2f || result.performance_score < 0.2f) {
        result.is_valid = false;
    }

    return result;
}

bool ConstraintSystem::validate_assembly_constraints(
    const AssemblyBlueprint& blueprint,
    std::vector<ConstraintViolation>& violations
) const {
    violations.clear();
    if (blueprint.components.empty()) {
        return true;
    }

    for (uint32_t rule_index = 0; rule_index < global_rules_.size(); ++rule_index) {
        const ConstraintRule& rule = global_rules_[rule_index];

        for (const auto& source : blueprint.components) {
            if (source.category != rule.source_component_type) {
                continue;
            }
            for (const auto& target : blueprint.components) {
                if (target.category != rule.target_component_type) {
                    continue;
                }
                float actual = evaluate_rule(rule, source, target);
                if (actual < rule.min_value || actual > rule.max_value) {
                    ConstraintViolation violation;
                    violation.rule_index = rule_index;
                    violation.source_component_id = source.instance_id;
                    violation.target_component_id = target.instance_id;
                    violation.actual_value = actual;
                    violation.rule = &rule;
                    violations.push_back(violation);
                }
            }
        }
    }

    for (const auto& [component_type, rules] : component_specific_rules_) {
        for (const auto& rule : rules) {
            for (const auto& source : blueprint.components) {
                if (source.category != component_type) {
                    continue;
                }
                for (const auto& target : blueprint.components) {
                    float actual = evaluate_rule(rule, source, target);
                    if (actual < rule.min_value || actual > rule.max_value) {
                        ConstraintViolation violation;
                        violation.rule_index = 0;
                        violation.source_component_id = source.instance_id;
                        violation.target_component_id = target.instance_id;
                        violation.actual_value = actual;
                        violation.rule = &rule;
                        violations.push_back(violation);
                    }
                }
            }
        }
    }

    return violations.empty();
}

float ConstraintSystem::evaluate_rule(
    const ConstraintRule& rule,
    const AssemblyComponentInstance& source,
    const AssemblyComponentInstance& target
) const {
    switch (static_cast<ConstraintType>(rule.constraint_type)) {
        case ConstraintType::POWER_REQUIREMENT:
            return source.mass_kg * 0.1f + target.mass_kg * 0.05f;
        case ConstraintType::FUEL_SYSTEM:
            return source.mass_kg * 0.08f + target.mass_kg * 0.02f;
        case ConstraintType::COOLING_REQUIREMENT:
            return source.mass_kg * 0.04f + target.mass_kg * 0.01f;
        case ConstraintType::CREW_ASSIGNMENT:
            return std::max(1u, static_cast<unsigned int>((source.mass_kg + target.mass_kg) / 1000.0f));
        case ConstraintType::WEIGHT_DISTRIBUTION: {
            float total = source.mass_kg + target.mass_kg;
            if (total <= 0.0f) {
                return 0.0f;
            }
            return std::abs(source.mass_kg - target.mass_kg) / total;
        }
        case ConstraintType::BALLISTIC_INTEGRITY:
            return std::min(source.mass_kg, target.mass_kg) * 0.5f;
        case ConstraintType::AERODYNAMIC:
            return std::abs(static_cast<float>(source.subcategory) - static_cast<float>(target.subcategory));
        case ConstraintType::PHYSICAL_ATTACHMENT:
        default:
            return (source.attached_to == target.instance_id) ? 1.0f : 0.0f;
    }
}

AssemblyValidationResult ValidationEngine::validate(const AssemblyBlueprint& blueprint) const {
    AssemblyValidationResult result;
    result.is_valid = true;
    result.stability_score = 0.5f;
    result.performance_score = 0.5f;
    result.historical_accuracy = 0.5f;

    float stability = 0.0f;
    if (validate_physical_stability(blueprint, stability)) {
        result.stability_score = stability;
    }

    float performance = 0.0f;
    if (validate_performance_requirements(blueprint, performance)) {
        result.performance_score = performance;
    }

    float authenticity = 0.0f;
    if (validate_historical_authenticity(blueprint, authenticity)) {
        result.historical_accuracy = authenticity;
    }

    result.is_valid = result.stability_score > 0.2f && result.performance_score > 0.2f;
    return result;
}

bool ValidationEngine::validate_physical_stability(const AssemblyBlueprint& blueprint, float& stability_score) const {
    return validate_weight_distribution(blueprint, stability_score);
}

bool ValidationEngine::validate_weight_distribution(const AssemblyBlueprint& blueprint, float& stability_score) const {
    if (blueprint.components.empty()) {
        stability_score = 0.0f;
        return false;
    }

    float min_mass = std::numeric_limits<float>::infinity();
    float max_mass = 0.0f;
    float total = 0.0f;
    for (const auto& component : blueprint.components) {
        min_mass = std::min(min_mass, component.mass_kg);
        max_mass = std::max(max_mass, component.mass_kg);
        total += component.mass_kg;
    }
    if (total <= 0.0f) {
        stability_score = 0.0f;
        return false;
    }
    stability_score = 1.0f - ((max_mass - min_mass) / total);
    stability_score = std::clamp(stability_score, 0.0f, 1.0f);
    return true;
}

bool ValidationEngine::validate_center_of_mass(const AssemblyBlueprint& blueprint, float& stability_score) const {
    stability_score = 0.5f;
    return !blueprint.components.empty();
}

bool ValidationEngine::validate_performance_requirements(const AssemblyBlueprint& blueprint, float& performance_score) const {
    if (blueprint.components.empty()) {
        performance_score = 0.0f;
        return false;
    }

    float total = 0.0f;
    for (const auto& component : blueprint.components) {
        total += component.mass_kg * component.reliability;
    }
    float average = total / static_cast<float>(blueprint.components.size());
    performance_score = std::clamp(average / 1000.0f, 0.0f, 1.0f);
    return true;
}

bool ValidationEngine::validate_resource_balance(const AssemblyBlueprint& blueprint, float& performance_score) const {
    return validate_performance_requirements(blueprint, performance_score);
}

bool ValidationEngine::validate_historical_authenticity(const AssemblyBlueprint& blueprint, float& historical_accuracy) const {
    historical_accuracy = blueprint.components.empty() ? 0.0f : 0.5f;
    return true;
}

} // namespace assembly_system
