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

#include <array>
#include <cstdint>
#include <immintrin.h>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "assembly_system/vehicle_assembly.h"

namespace assembly_system {

using AssetID = uint64_t;

enum class ComponentCategory : uint32_t {
    UNKNOWN = 0,
    HULL = 1,
    ENGINE = 2,
    WEAPON = 3,
    CONTROL = 4,
    SENSOR = 5,
    CHASSIS = 6,
    SYSTEM = 7,
};

struct CompatibleComponent {
    uint32_t component_id = 0;
    float compatibility_score = 1.0f;
};

struct MaterialOverride {
    AssetID material_id = 0;
    uint32_t material_slot = 0;
};

struct ResourceRequirements {
    float power_draw_watts = 0.0f;
    float fuel_consumption_lph = 0.0f;
    float cooling_requirement = 0.0f;
    uint32_t crew_requirement = 0;
};

struct PerformanceStats {
    float mobility_bonus = 0.0f;
    float armor_bonus = 0.0f;
    float weapon_accuracy = 0.0f;
    float reliability_rating = 1.0f;
};

struct AttachmentPointDefinition {
    __m256 local_position{};
    __m256 local_rotation{};
    uint32_t socket_type = 0;
    uint32_t compatible_types_mask = 0;
};

struct AssemblyComponentDefinition {
    static constexpr size_t kMaxAttachmentPoints = 4;
    static constexpr size_t kMaxCompatibleEntries = 4;
    static constexpr size_t kMaxMaterialOverrides = 4;

    uint32_t component_id = 0;
    char component_name[64] = {};
    uint32_t component_category = static_cast<uint32_t>(ComponentCategory::UNKNOWN);
    uint32_t component_subcategory = 0;

    float mass_kg = 0.0f;
    BoundingBox local_bounds{};
    __m256 center_of_mass{};

    std::array<AttachmentPointDefinition, kMaxAttachmentPoints> attachment_points{};
    uint32_t attachment_point_count = 0;

    std::array<CompatibleComponent, kMaxCompatibleEntries> compatible_components{};
    uint32_t compatible_component_count = 0;

    ResourceRequirements resources{};
    PerformanceStats performance{};

    AssetID visual_mesh_id = 0;
    AssetID collision_mesh_id = 0;

    std::array<MaterialOverride, kMaxMaterialOverrides> material_overrides{};
    uint32_t material_override_count = 0;
};

enum class ConstraintType : uint32_t {
    PHYSICAL_ATTACHMENT = 0,
    POWER_REQUIREMENT = 1,
    FUEL_SYSTEM = 2,
    COOLING_REQUIREMENT = 3,
    CREW_ASSIGNMENT = 4,
    WEIGHT_DISTRIBUTION = 5,
    AERODYNAMIC = 6,
    BALLISTIC_INTEGRITY = 7
};

struct ConstraintRule {
    uint32_t constraint_type = static_cast<uint32_t>(ConstraintType::PHYSICAL_ATTACHMENT);
    uint32_t source_component_type = static_cast<uint32_t>(ComponentCategory::UNKNOWN);
    uint32_t target_component_type = static_cast<uint32_t>(ComponentCategory::UNKNOWN);
    float min_value = 0.0f;
    float max_value = std::numeric_limits<float>::infinity();
    bool is_hard_constraint = true;
    char error_message[128] = {};
};

struct ConstraintViolation {
    uint32_t rule_index = 0;
    uint32_t source_component_id = 0;
    uint32_t target_component_id = 0;
    float actual_value = 0.0f;
    const ConstraintRule* rule = nullptr;
};

struct AssemblyComponentInstance {
    uint32_t instance_id = 0;
    uint32_t definition_id = 0;
    uint32_t category = static_cast<uint32_t>(ComponentCategory::UNKNOWN);
    uint32_t subcategory = 0;
    float mass_kg = 0.0f;
    float reliability = 1.0f;
    uint32_t attached_to = 0;
};

struct AssemblyBlueprint {
    std::string name;
    std::vector<AssemblyComponentInstance> components;
};

struct AssemblyConstructionOptions {
    bool allow_unfinished_components = false;
    bool require_historical_accuracy = false;
    float max_budget = 0.0f;
};

struct ValidationError {
    uint32_t error_type = 0;
    EntityID affected_component = 0;
    char description[256] = {};
    bool is_fatal = true;
};

struct ValidationWarning {
    uint32_t warning_type = 0;
    EntityID affected_component = 0;
    char description[256] = {};
};

struct PerformanceSuggestion {
    uint32_t suggestion_type = 0;
    char description[256] = {};
    float expected_gain = 0.0f;
};

struct AssemblyValidationResult {
    bool is_valid = false;
    float stability_score = 0.0f;
    float performance_score = 0.0f;
    float historical_accuracy = 0.0f;
    std::vector<ValidationError> errors;
    std::vector<ValidationWarning> warnings;
    std::vector<PerformanceSuggestion> suggestions;
};

struct PartialAssembly {
    std::vector<AssemblyComponentInstance> components;
};

struct PTUSecurityValidation {
    bool passed = false;
    std::vector<std::string> issues;
};

struct NetworkAssemblyPacket {
    uint64_t version = 1;
    std::vector<uint32_t> component_ids;
};

struct HistoricalVehicleSpecs {
    uint32_t vehicle_id = 0;
    char vehicle_name[64] = {};
    uint32_t nation = 0;
    uint32_t era = 0;
    uint32_t vehicle_class = 0;
    float weight_tons = 0.0f;
    float engine_power_hp = 0.0f;
    float fuel_capacity_liters = 0.0f;
    float armor_front = 0.0f;
    float armor_side = 0.0f;
    float armor_rear = 0.0f;
    float armor_top = 0.0f;
    uint32_t crew_size = 0;
};

struct CampaignRules {
    float era_balance_factor = 1.0f;
    float resource_multiplier = 1.0f;
    uint32_t restrict_nations_mask = UINT32_MAX;
};

struct MultiplayerBalanceRules {
    float max_power_ratio = 1.5f;
    float max_mass_ratio = 1.5f;
    bool enforce_role_limits = true;
};

struct AdvancedCustomization {
    bool allow_visual_variants = true;
    bool allow_tuning_packages = true;
};

class ComponentLibrary {
public:
    bool register_definition(AssemblyComponentDefinition definition) {
        if (definition.component_id == 0) {
            return false;
        }
        definitions_[definition.component_id] = std::move(definition);
        return true;
    }

    const AssemblyComponentDefinition* find_definition(uint32_t component_id) const {
        auto it = definitions_.find(component_id);
        if (it == definitions_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    std::vector<const AssemblyComponentDefinition*> find_compatible(uint32_t component_id) const {
        std::vector<const AssemblyComponentDefinition*> result;
        const AssemblyComponentDefinition* source = find_definition(component_id);
        if (!source) {
            return result;
        }

        for (auto& [id, entry] : definitions_) {
            if (id == component_id) {
                continue;
            }
            for (uint32_t index = 0; index < source->compatible_component_count; ++index) {
                if (source->compatible_components[index].component_id == id) {
                    result.push_back(&entry);
                    break;
                }
            }
        }
        return result;
    }

private:
    std::unordered_map<uint32_t, AssemblyComponentDefinition> definitions_;
};

class ConstraintSystem {
public:
    void add_global_rule(ConstraintRule rule) {
        global_rules_.push_back(std::move(rule));
    }

    void add_component_specific_rule(uint32_t component_type, ConstraintRule rule) {
        component_specific_rules_[component_type].push_back(std::move(rule));
    }

    bool validate_assembly_constraints(
        const AssemblyBlueprint& blueprint,
        std::vector<ConstraintViolation>& violations
    ) const;

private:
    float evaluate_rule(const ConstraintRule& rule, const AssemblyComponentInstance& source, const AssemblyComponentInstance& target) const;
    std::vector<ConstraintRule> global_rules_;
    std::unordered_map<uint32_t, std::vector<ConstraintRule>> component_specific_rules_;
};

class ValidationEngine {
public:
    AssemblyValidationResult validate(const AssemblyBlueprint& blueprint) const;
    bool validate_physical_stability(const AssemblyBlueprint& blueprint, float& stability_score) const;
    bool validate_weight_distribution(const AssemblyBlueprint& blueprint, float& stability_score) const;
    bool validate_center_of_mass(const AssemblyBlueprint& blueprint, float& stability_score) const;
    bool validate_performance_requirements(const AssemblyBlueprint& blueprint, float& performance_score) const;
    bool validate_resource_balance(const AssemblyBlueprint& blueprint, float& performance_score) const;
    bool validate_historical_authenticity(const AssemblyBlueprint& blueprint, float& historical_accuracy) const;
};

class UniversalAssemblyEngine {
public:
    explicit UniversalAssemblyEngine(ecs::EntityManager* entity_manager = nullptr);

    void set_entity_manager(ecs::EntityManager* entity_manager);
    ComponentLibrary& component_library() { return component_library_; }
    const ComponentLibrary& component_library() const { return component_library_; }

    bool construct_vehicle(
        const AssemblyBlueprint& blueprint,
        const AssemblyConstructionOptions& options,
        EntityID& output_entity,
        AssemblyValidationResult& validation_result
    );

    bool validate_partial_assembly(
        const PartialAssembly& partial_assembly,
        AssemblyValidationResult& feedback
    ) const;

    bool validate_ptu_content(
        const AssemblyBlueprint& ptu_blueprint,
        PTUSecurityValidation& security_check,
        AssemblyValidationResult& validation_result
    ) const;

    bool create_historical_blueprint_from_specs(
        const HistoricalVehicleSpecs& specs,
        AssemblyBlueprint& output_blueprint
    ) const;

    bool instantiate_assembled_vehicle(
        const AssemblyBlueprint& validated_assembly,
        EntityID& output_entity
    );

    bool serialize_assembly_for_network(
        const AssemblyBlueprint& assembly,
        NetworkAssemblyPacket& output_packet
    ) const;

    bool apply_campaign_constraints(
        const CampaignRules& campaign_rules,
        AssemblyBlueprint& blueprint
    ) const;

    bool enforce_multiplayer_balance(
        const MultiplayerBalanceRules& balance_rules,
        AssemblyBlueprint& blueprint
    ) const;

    bool apply_advanced_customization(
        const AdvancedCustomization& customization,
        AssemblyBlueprint& blueprint
    ) const;

private:
    bool populate_vehicle_blueprint(const AssemblyBlueprint& assembly, VehicleBlueprint& out_blueprint, std::string& error_message) const;
    AssemblyValidationResult run_validation(const AssemblyBlueprint& blueprint) const;
    static float clamp01(float value) {
        return std::max(0.0f, std::min(1.0f, value));
    }

    ecs::EntityManager* entity_manager_ = nullptr;
    ComponentLibrary component_library_;
    ConstraintSystem constraint_system_;
    ValidationEngine validation_engine_;
    ModularVehicleSystem modular_system_;
};

} // namespace assembly_system
