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
#include "historical_vehicle_system/historical_vehicle_system.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

#include <rendering_engine/resource_manager/resource_manager.h>
#include <rendering_engine/model_system.h>
#include <physics_core/damage_system.h>

// For procedural generation
#include <procedural_armor_factory/armor_generator.h>
#include <procedural_armor_factory/parametric_template.h>

namespace historical_vehicle_system {

static float clampf(float value, float min_value, float max_value) {
    return std::max(min_value, std::min(max_value, value));
}

static bool is_digit_or_sign(char c) {
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E';
}

std::string HistoricalDatabase::normalize_name(std::string_view name) {
    std::string output;
    output.reserve(std::min<size_t>(name.size(), 63));
    for (char c : name) {
        if (c == '"' || c == '\r' || c == '\n') {
            continue;
        }
        output.push_back(c);
        if (output.size() >= 63) {
            break;
        }
    }
    while (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }
    return output;
}

std::vector<std::string> HistoricalDatabase::split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;

    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c == ',' && !in_quotes) {
            tokens.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    tokens.push_back(current);
    return tokens;
}

bool HistoricalDatabase::load_vehicle_database(const std::string& database_path, DataValidationLevel validation_level) {
    std::ifstream file(database_path, std::ios::binary);
    if (!file.is_open()) {
        last_error_message_ = "Unable to open database file: " + database_path;
        return false;
    }

    std::string extension;
    auto dot = database_path.find_last_of('.');
    if (dot != std::string::npos) {
        extension = database_path.substr(dot + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }

    bool result = false;
    if (extension == "csv") {
        result = parse_csv(file, validation_level);
    } else if (extension == "json") {
        result = parse_json(file, validation_level);
    } else {
        last_error_message_ = "Unsupported database extension: " + extension;
        return false;
    }

    if (!result && last_error_message_.empty()) {
        last_error_message_ = "Failed to parse database: " + database_path;
    }
    return result;
}

const HistoricalVehicleSpecs* HistoricalDatabase::get_vehicle_by_id(uint32_t vehicle_id) const {
    auto it = vehicle_index_.find(vehicle_id);
    if (it == vehicle_index_.end()) {
        return nullptr;
    }
    return &vehicles_[it->second];
}

bool HistoricalDatabase::filter_vehicles_by_era(uint32_t era_filter, std::vector<uint32_t>& output_vehicle_ids) const {
    output_vehicle_ids.clear();
    for (const auto& entry : vehicles_) {
        if (entry.era == era_filter) {
            output_vehicle_ids.push_back(entry.vehicle_id);
        }
    }
    return !output_vehicle_ids.empty();
}

bool HistoricalDatabase::get_nation_variants(uint32_t base_vehicle_id, uint32_t nation, std::vector<HistoricalVehicleSpecs>& variants) const {
    variants.clear();
    const HistoricalVehicleSpecs* base = get_vehicle_by_id(base_vehicle_id);
    if (!base) {
        return false;
    }

    std::string prefix = base->vehicle_name;
    auto delim = prefix.find_first_of("/ ");
    if (delim != std::string::npos) {
        prefix.resize(delim);
    }

    for (const auto& entry : vehicles_) {
        if (entry.vehicle_id == base_vehicle_id) {
            continue;
        }
        if (entry.nation != nation) {
            continue;
        }
        if (entry.era != base->era || entry.vehicle_class != base->vehicle_class) {
            continue;
        }
        std::string name(entry.vehicle_name);
        if (!prefix.empty() && name.rfind(prefix, 0) == 0) {
            variants.push_back(entry);
        }
    }
    return !variants.empty();
}

bool HistoricalDatabase::get_campaign_vehicles(const CampaignSpecs& campaign, std::vector<uint32_t>& available_vehicles) const {
    available_vehicles.clear();
    for (const auto& entry : vehicles_) {
        if (campaign.era != UINT32_MAX && entry.era != campaign.era) {
            continue;
        }
        if (campaign.nation != UINT32_MAX && entry.nation != campaign.nation) {
            continue;
        }
        if (campaign.vehicle_class != UINT32_MAX && entry.vehicle_class != campaign.vehicle_class) {
            continue;
        }
        available_vehicles.push_back(entry.vehicle_id);
    }
    return !available_vehicles.empty();
}

bool HistoricalDatabase::parse_csv(std::istream& stream, DataValidationLevel validation_level) {
    std::string line;
    if (!std::getline(stream, line)) {
        last_error_message_ = "CSV file is empty.";
        return false;
    }

    size_t line_number = 1;
    while (std::getline(stream, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        auto tokens = split_csv_line(line);
        if (tokens.size() != 36) {
            last_error_message_ = "CSV line " + std::to_string(line_number) + " has incorrect field count.";
            return false;
        }

        HistoricalVehicleSpecs entry;
        entry.vehicle_id = static_cast<uint32_t>(std::stoul(tokens[0]));
        std::string normalized = normalize_name(tokens[1]);
        std::memcpy(entry.vehicle_name, normalized.c_str(), std::min<size_t>(normalized.size(), sizeof(entry.vehicle_name) - 1));
        entry.nation = static_cast<uint32_t>(std::stoul(tokens[2]));
        entry.era = static_cast<uint32_t>(std::stoul(tokens[3]));
        entry.vehicle_class = static_cast<uint32_t>(std::stoul(tokens[4]));
        entry.length_m = std::stof(tokens[5]);
        entry.width_m = std::stof(tokens[6]);
        entry.height_m = std::stof(tokens[7]);
        entry.weight_tons = std::stof(tokens[8]);
        entry.ground_pressure_kpa = std::stof(tokens[9]);
        entry.armor.hull_front = std::stof(tokens[10]);
        entry.armor.hull_side = std::stof(tokens[11]);
        entry.armor.hull_rear = std::stof(tokens[12]);
        entry.armor.hull_top = std::stof(tokens[13]);
        entry.armor.turret_front = std::stof(tokens[14]);
        entry.armor.turret_side = std::stof(tokens[15]);
        entry.armor.turret_rear = std::stof(tokens[16]);
        entry.armor.turret_top = std::stof(tokens[17]);
        entry.engine.engine_power_hp = std::stof(tokens[18]);
        entry.engine.max_speed_kmh = std::stof(tokens[19]);
        entry.engine.reverse_speed_kmh = std::stof(tokens[20]);
        entry.engine.fuel_capacity_liters = std::stof(tokens[21]);
        entry.engine.operational_range_km = std::stof(tokens[22]);
        entry.engine.engine_type = static_cast<uint32_t>(std::stoul(tokens[23]));
        entry.weapons.main_gun_caliber_mm = static_cast<uint32_t>(std::stoul(tokens[24]));
        entry.weapons.secondary_weapons_count = static_cast<uint32_t>(std::stoul(tokens[25]));
        entry.weapons.rate_of_fire_rpm = std::stof(tokens[26]);
        entry.weapons.ammunition_capacity = static_cast<uint32_t>(std::stoul(tokens[27]));
        entry.weapons.max_elevation_deg = std::stof(tokens[28]);
        entry.weapons.max_depression_deg = std::stof(tokens[29]);
        entry.crew_size = static_cast<uint32_t>(std::stoul(tokens[30]));
        entry.reload_time_seconds = std::stof(tokens[31]);
        entry.has_radio = static_cast<bool>(std::stoul(tokens[32]));
        entry.has_night_vision = static_cast<bool>(std::stoul(tokens[33]));
        entry.data_confidence = static_cast<uint32_t>(std::stoul(tokens[34]));
        entry.source_references = static_cast<uint32_t>(std::stoul(tokens[35]));

        std::string validation_message;
        if (!validate_vehicle_entry(entry, validation_level, validation_message)) {
            last_error_message_ = "CSV validation failed at line " + std::to_string(line_number) + ": " + validation_message;
            return false;
        }

        if (vehicle_index_.count(entry.vehicle_id) != 0) {
            last_error_message_ = "Duplicate vehicle_id detected: " + std::to_string(entry.vehicle_id);
            return false;
        }

        build_soa(entry);
    }

    return !vehicles_.empty();
}

bool HistoricalDatabase::parse_json(std::istream& stream, DataValidationLevel validation_level) {
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    size_t index = 0;
    const size_t length = text.size();

    auto skip_whitespace = [&]() {
        while (index < length && std::isspace(static_cast<unsigned char>(text[index]))) {
            ++index;
        }
    };

    auto parse_token = [&](const std::string& token) {
        skip_whitespace();
        if (text.compare(index, token.size(), token) == 0) {
            index += token.size();
            return true;
        }
        return false;
    };

    skip_whitespace();
    if (index >= length || text[index] != '[') {
        last_error_message_ = "JSON root must be an array.";
        return false;
    }
    ++index;

    while (index < length) {
        skip_whitespace();
        if (index < length && text[index] == ']') {
            ++index;
            break;
        }
        if (index >= length || text[index] != '{') {
            last_error_message_ = "JSON object expected.";
            return false;
        }
        ++index;

        HistoricalVehicleSpecs entry = {};
        bool first_field = true;
        while (index < length) {
            skip_whitespace();
            if (index < length && text[index] == '}') {
                ++index;
                break;
            }
            if (!first_field) {
                if (!parse_token(",")) {
                    last_error_message_ = "JSON object missing comma separator.";
                    return false;
                }
                skip_whitespace();
            }
            first_field = false;

            if (index >= length || text[index] != '"') {
                last_error_message_ = "JSON field name expected.";
                return false;
            }
            ++index;
            size_t name_start = index;
            while (index < length && text[index] != '"') {
                ++index;
            }
            if (index >= length) {
                last_error_message_ = "Unterminated JSON field name.";
                return false;
            }
            std::string field_name = text.substr(name_start, index - name_start);
            ++index;
            skip_whitespace();
            if (!parse_token(":")) {
                last_error_message_ = "JSON field delimiter ':' expected.";
                return false;
            }
            skip_whitespace();

            bool is_string = index < length && text[index] == '"';
            std::string value;
            if (is_string) {
                ++index;
                size_t value_start = index;
                while (index < length && text[index] != '"') {
                    if (text[index] == '\\' && index + 1 < length) {
                        index += 2;
                    } else {
                        ++index;
                    }
                }
                if (index >= length) {
                    last_error_message_ = "Unterminated JSON string value.";
                    return false;
                }
                value = text.substr(value_start, index - value_start);
                ++index;
            } else {
                size_t value_start = index;
                while (index < length && (is_digit_or_sign(text[index]) || text[index] == 'e' || text[index] == 'E')) {
                    ++index;
                }
                value = text.substr(value_start, index - value_start);
            }

            if (field_name == "vehicle_id") {
                entry.vehicle_id = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "vehicle_name") {
                std::string normalized = normalize_name(value);
                std::memcpy(entry.vehicle_name, normalized.c_str(), std::min<size_t>(normalized.size(), sizeof(entry.vehicle_name) - 1));
            } else if (field_name == "nation") {
                entry.nation = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "era") {
                entry.era = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "vehicle_class") {
                entry.vehicle_class = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "length_m") {
                entry.length_m = std::stof(value);
            } else if (field_name == "width_m") {
                entry.width_m = std::stof(value);
            } else if (field_name == "height_m") {
                entry.height_m = std::stof(value);
            } else if (field_name == "weight_tons") {
                entry.weight_tons = std::stof(value);
            } else if (field_name == "ground_pressure_kpa") {
                entry.ground_pressure_kpa = std::stof(value);
            } else if (field_name == "hull_front") {
                entry.armor.hull_front = std::stof(value);
            } else if (field_name == "hull_side") {
                entry.armor.hull_side = std::stof(value);
            } else if (field_name == "hull_rear") {
                entry.armor.hull_rear = std::stof(value);
            } else if (field_name == "hull_top") {
                entry.armor.hull_top = std::stof(value);
            } else if (field_name == "turret_front") {
                entry.armor.turret_front = std::stof(value);
            } else if (field_name == "turret_side") {
                entry.armor.turret_side = std::stof(value);
            } else if (field_name == "turret_rear") {
                entry.armor.turret_rear = std::stof(value);
            } else if (field_name == "turret_top") {
                entry.armor.turret_top = std::stof(value);
            } else if (field_name == "engine_power_hp") {
                entry.engine.engine_power_hp = std::stof(value);
            } else if (field_name == "max_speed_kmh") {
                entry.engine.max_speed_kmh = std::stof(value);
            } else if (field_name == "reverse_speed_kmh") {
                entry.engine.reverse_speed_kmh = std::stof(value);
            } else if (field_name == "fuel_capacity_liters") {
                entry.engine.fuel_capacity_liters = std::stof(value);
            } else if (field_name == "operational_range_km") {
                entry.engine.operational_range_km = std::stof(value);
            } else if (field_name == "engine_type") {
                entry.engine.engine_type = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "main_gun_caliber_mm") {
                entry.weapons.main_gun_caliber_mm = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "secondary_weapons_count") {
                entry.weapons.secondary_weapons_count = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "rate_of_fire_rpm") {
                entry.weapons.rate_of_fire_rpm = std::stof(value);
            } else if (field_name == "ammunition_capacity") {
                entry.weapons.ammunition_capacity = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "max_elevation_deg") {
                entry.weapons.max_elevation_deg = std::stof(value);
            } else if (field_name == "max_depression_deg") {
                entry.weapons.max_depression_deg = std::stof(value);
            } else if (field_name == "crew_size") {
                entry.crew_size = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "reload_time_seconds") {
                entry.reload_time_seconds = std::stof(value);
            } else if (field_name == "has_radio") {
                entry.has_radio = static_cast<bool>(std::stoul(value));
            } else if (field_name == "has_night_vision") {
                entry.has_night_vision = static_cast<bool>(std::stoul(value));
            } else if (field_name == "data_confidence") {
                entry.data_confidence = static_cast<uint32_t>(std::stoul(value));
            } else if (field_name == "source_references") {
                entry.source_references = static_cast<uint32_t>(std::stoul(value));
            }
        }

        std::string validation_message;
        if (!validate_vehicle_entry(entry, validation_level, validation_message)) {
            last_error_message_ = "JSON validation failed for vehicle id " + std::to_string(entry.vehicle_id) + ": " + validation_message;
            return false;
        }

        if (vehicle_index_.count(entry.vehicle_id) != 0) {
            last_error_message_ = "Duplicate vehicle_id detected: " + std::to_string(entry.vehicle_id);
            return false;
        }

        build_soa(entry);
        skip_whitespace();
        if (index < length && text[index] == ',') {
            ++index;
        }
    }

    return !vehicles_.empty();
}

bool HistoricalDatabase::validate_vehicle_entry(const HistoricalVehicleSpecs& specs, DataValidationLevel validation_level, std::string& out_message) const {
    if (specs.vehicle_id == 0) {
        out_message = "vehicle_id must be non-zero.";
        return false;
    }
    if (specs.vehicle_name[0] == '\0') {
        out_message = "vehicle_name must not be empty.";
        return false;
    }
    if (specs.length_m <= 0.0f || specs.width_m <= 0.0f || specs.height_m <= 0.0f) {
        out_message = "Vehicle dimensions must be positive.";
        return false;
    }
    if (specs.weight_tons <= 0.0f) {
        out_message = "Vehicle weight must be positive.";
        return false;
    }
    if (specs.engine.engine_power_hp <= 0.0f) {
        out_message = "Engine power must be positive.";
        return false;
    }
    if (specs.engine.max_speed_kmh < 0.0f) {
        out_message = "Maximum speed cannot be negative.";
        return false;
    }
    if (specs.engine.fuel_capacity_liters < 0.0f) {
        out_message = "Fuel capacity cannot be negative.";
        return false;
    }
    if (specs.crew_size == 0) {
        out_message = "Crew size must be at least 1.";
        return false;
    }
    if (specs.reload_time_seconds < 0.0f) {
        out_message = "Reload time cannot be negative.";
        return false;
    }
    if (validation_level == DataValidationLevel::FULL) {
        if (specs.data_confidence == 0 || specs.source_references == 0) {
            out_message = "Full validation requires confidence and source references.";
            return false;
        }
    }
    return true;
}

void HistoricalDatabase::build_soa(const HistoricalVehicleSpecs& specs) {
    size_t index = vehicles_.size();
    vehicles_.push_back(specs);
    vehicle_index_[specs.vehicle_id] = index;
    vehicle_ids_.push_back(specs.vehicle_id);
    vehicle_lengths_.push_back(specs.length_m);
    vehicle_widths_.push_back(specs.width_m);
    vehicle_heights_.push_back(specs.height_m);
    vehicle_weights_.push_back(specs.weight_tons);
    ground_pressures_.push_back(specs.ground_pressure_kpa);
    era_index_[specs.era].push_back(specs.vehicle_id);
}

uint32_t PhysicsMappingSystem::pack_float(float value) noexcept {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

uint64_t PhysicsMappingSystem::compute_cache_key(uint32_t vehicle_id, const BalanceProfile& profile) noexcept {
    uint64_t key = static_cast<uint64_t>(vehicle_id) << 32;
    uint32_t a = pack_float(profile.realism_level);
    uint32_t b = pack_float(profile.team_balance_factor);
    uint32_t c = pack_float(profile.era_balance_factor);
    uint32_t d = pack_float(profile.map_size_factor);
    key ^= static_cast<uint64_t>(a);
    key ^= static_cast<uint64_t>(b) << 8;
    key ^= static_cast<uint64_t>(c) << 16;
    key ^= static_cast<uint64_t>(d) << 24;
    return key;
}

bool PhysicsMappingSystem::generate_physics_parameters(
    const HistoricalVehicleSpecs& historical_specs,
    const BalanceProfile& balance_profile,
    PhysicsMappingParameters& output_params
) {
    uint64_t key = compute_cache_key(historical_specs.vehicle_id, balance_profile);
    auto cache_it = cache_.find(key);
    if (cache_it != cache_.end()) {
        output_params = cache_it->second;
        return true;
    }

    float length = std::max(1.0f, historical_specs.length_m);
    float width = std::max(1.0f, historical_specs.width_m);
    float height = std::max(1.0f, historical_specs.height_m);
    float mass = std::max(1.0f, historical_specs.weight_tons * 1000.0f);

    output_params.mass_distribution_x = clampf((width - length) / std::max(length, 1.0f), -1.0f, 1.0f);
    output_params.mass_distribution_y = 0.0f;
    output_params.mass_distribution_z = clampf((height - 2.0f) / std::max(height, 1.0f), -1.0f, 1.0f);
    output_params.inertia_scale_xx = clampf(0.3f + mass * 0.0005f, 0.6f, 3.0f);
    output_params.inertia_scale_yy = clampf(0.4f + mass * 0.00045f, 0.6f, 2.8f);
    output_params.inertia_scale_zz = clampf(0.5f + mass * 0.00035f, 0.6f, 2.5f);
    output_params.suspension_stiffness = clampf(historical_specs.weight_tons * 12000.0f, 15000.0f, 350000.0f);
    output_params.suspension_damping = clampf(output_params.suspension_stiffness * 0.035f, 1200.0f, 12000.0f);
    output_params.track_friction_coeff = clampf(0.8f - historical_specs.ground_pressure_kpa * 0.0005f, 0.3f, 1.0f);

    output_params.drag_coefficient = clampf(0.65f + (length * width * height) * 0.002f, 0.2f, 1.2f);
    output_params.lift_coefficient = (historical_specs.vehicle_class == 3u) ? 0.35f : 0.05f;
    output_params.recoil_impulse_scale = clampf(historical_specs.weapons.rate_of_fire_rpm * 0.018f, 0.1f, 5.0f);
    output_params.barrel_elevation_speed = clampf(15.0f + historical_specs.weight_tons * 0.15f, 8.0f, 55.0f);
    output_params.mobility_multiplier = clampf(1.0f + (balance_profile.team_balance_factor - 0.5f) * 0.15f, 0.75f, 1.25f);
    output_params.armor_multiplier = clampf(1.0f + (balance_profile.realism_level - 0.5f) * 0.2f, 0.8f, 1.3f);
    output_params.weapon_accuracy_multiplier = clampf(1.0f - (1.0f - balance_profile.realism_level) * 0.25f, 0.75f, 1.0f);

    cache_[key] = output_params;
    return true;
}

void VehicleCalibrationSystem::set_active_profiles(const std::vector<BalanceProfile>& profiles) {
    active_profiles_ = profiles;
}

void VehicleCalibrationSystem::auto_calibrate_for_balance(const HistoricalVehicleSpecs& specs, PhysicsMappingParameters& output) {
    auto it = calibration_db_.find(specs.vehicle_id);
    if (it != calibration_db_.end()) {
        output = it->second;
    } else {
        mapping_system_.generate_physics_parameters(specs, active_profiles_.empty() ? BalanceProfile{} : active_profiles_.front(), output);
    }

    if (!active_profiles_.empty()) {
        const BalanceProfile& profile = active_profiles_.front();
        output.mobility_multiplier = clampf(output.mobility_multiplier * (1.0f + (profile.realism_level - 0.5f) * 0.1f), 0.75f, 1.35f);
        output.armor_multiplier = clampf(output.armor_multiplier * (1.0f + (profile.era_balance_factor - 0.5f) * 0.1f), 0.75f, 1.4f);
        output.weapon_accuracy_multiplier = clampf(output.weapon_accuracy_multiplier * (1.0f + (profile.map_size_factor - 0.5f) * 0.05f), 0.75f, 1.15f);
    }
}

void VehicleCalibrationSystem::apply_manual_calibration(uint32_t vehicle_id, const PhysicsMappingParameters& overrides) {
    calibration_db_[vehicle_id] = overrides;
}

HistoricalVehicleSystem::HistoricalVehicleSystem(ecs::EntityManager* entity_manager)
    : entity_manager_(entity_manager), modular_vehicle_system_(entity_manager) {
}

void HistoricalVehicleSystem::set_entity_manager(ecs::EntityManager* entity_manager) {
    entity_manager_ = entity_manager;
    modular_vehicle_system_.set_entity_manager(entity_manager);
}

void HistoricalVehicleSystem::set_model_system(rendering_engine::ModelSystem* model_system) {
    model_system_ = model_system;
}

void HistoricalVehicleSystem::set_physics_core(physics_core::PhysicsCore* physics_core) {
    physics_core_ = physics_core;
}

void HistoricalVehicleSystem::set_database(const HistoricalDatabase& database) {
    database_ = database;
}

bool HistoricalVehicleSystem::load_vehicle_database(const std::string& database_path, DataValidationLevel validation_level) {
    return database_.load_vehicle_database(database_path, validation_level);
}

bool HistoricalVehicleSystem::create_vehicle_from_historical_id(
    uint32_t vehicle_id,
    const BalanceProfile& balance_profile,
    physics_core::EntityID& output_entity
) {
    const HistoricalVehicleSpecs* specs = database_.get_vehicle_by_id(vehicle_id);
    if (!specs || !entity_manager_) {
        return false;
    }

    PhysicsMappingParameters parameters;
    physics_mapper_.generate_physics_parameters(*specs, balance_profile, parameters);
    calibration_system_.auto_calibrate_for_balance(*specs, parameters);

    assembly_system::VehicleBlueprint blueprint;
    if (!convert_to_modular_blueprint(*specs, blueprint)) {
        return false;
    }

    std::string error_message;
    if (!modular_vehicle_system_.assemble_vehicle_from_blueprint(blueprint, output_entity, error_message)) {
        return false;
    }
    return true;
}

bool HistoricalVehicleSystem::convert_to_modular_blueprint(
    const HistoricalVehicleSpecs& specs,
    assembly_system::VehicleBlueprint& output_blueprint
) const {
    output_blueprint = assembly_system::VehicleBlueprint::create_default();
    std::string vehicle_name(specs.vehicle_name);
    if (!vehicle_name.empty()) {
        output_blueprint.name = vehicle_name;
    }
    output_blueprint.hull.armor_thickness_front = specs.armor.hull_front;
    output_blueprint.hull.armor_thickness_side = specs.armor.hull_side;
    output_blueprint.hull.armor_thickness_rear = specs.armor.hull_rear;
    output_blueprint.hull.armor_thickness_top = specs.armor.hull_top;
    output_blueprint.hull.local_bounds.min = physics_core::Vec3(-specs.width_m * 0.5, -specs.length_m * 0.5, 0.0);
    output_blueprint.hull.local_bounds.max = physics_core::Vec3(specs.width_m * 0.5, specs.length_m * 0.5, specs.height_m);
    output_blueprint.hull.structural_integrity = clampf(0.5f + specs.weight_tons * 0.01f, 0.5f, 1.5f);

    output_blueprint.engine.max_power_hp = specs.engine.engine_power_hp;
    output_blueprint.engine.fuel_capacity_liters = specs.engine.fuel_capacity_liters;
    output_blueprint.engine.current_rpm = 600.0f;
    output_blueprint.engine.engine_type = specs.engine.engine_type;
    output_blueprint.engine.fuel_current_liters = specs.engine.fuel_capacity_liters * 0.75f;
    output_blueprint.engine.fuel_line_count = 1;
    output_blueprint.engine.fuel_lines[0].max_flow_lpm = specs.engine.engine_power_hp * 0.22f;
    output_blueprint.engine.fuel_lines[0].current_flow_lpm = output_blueprint.engine.fuel_lines[0].max_flow_lpm * 0.6f;

    output_blueprint.weapons.weapon_mount_count = 1;
    output_blueprint.weapons.ammunition_total = static_cast<float>(specs.weapons.ammunition_capacity);
    output_blueprint.weapons.ammunition_current = static_cast<float>(specs.weapons.ammunition_capacity);
    output_blueprint.weapons.reload_time_seconds = specs.reload_time_seconds;
    output_blueprint.weapons.ammo_type_count = 1;
    output_blueprint.weapons.ammo_types[0].ammo_id = specs.weapons.main_gun_caliber_mm;
    output_blueprint.weapons.ammo_types[0].penetration = static_cast<float>(specs.weapons.main_gun_caliber_mm) * 8.0f;
    output_blueprint.weapons.ammo_types[0].explosive_mass = static_cast<float>(specs.weapons.main_gun_caliber_mm) * 0.015f;
    output_blueprint.weapons.ammo_types[0].mass_kg = static_cast<float>(specs.weapons.main_gun_caliber_mm) * 0.18f;
    output_blueprint.weapons.reload_time_seconds = specs.reload_time_seconds;

    output_blueprint.control.steering_response = clampf(0.4f + specs.engine.max_speed_kmh * 0.005f, 0.3f, 1.0f);
    output_blueprint.control.suspension_health = clampf(0.6f + specs.weight_tons * 0.01f, 0.6f, 1.0f);
    output_blueprint.control.has_stabilization = specs.has_night_vision;

    output_blueprint.weapons.weapon_mounts[0].recoil_force = specs.weapons.rate_of_fire_rpm * 0.05f;

    return true;
}

bool HistoricalVehicleSystem::apply_physics_mapping(
    physics_core::EntityID vehicle_entity,
    const PhysicsMappingParameters& params,
    physics_core::PhysicsCore& physics_core
) const {
    physics_core::PhysicsBody* body = physics_core.get_body(vehicle_entity);
    if (!body) {
        return false;
    }

    body->mass = std::max(1.0f, params.inertia_scale_xx * 1000.0f);
    body->inv_mass = 1.0f / body->mass;
    body->friction = clampf(params.track_friction_coeff, 0.0f, 1.0f);
    body->linear_damping = clampf(params.drag_coefficient * 0.15f, 0.0f, 1.0f);
    body->angular_damping = clampf(params.drag_coefficient * 0.1f, 0.0f, 1.0f);

    physics_core::Mat3x3 inertia;
    inertia(0, 0) = body->mass * params.inertia_scale_xx;
    inertia(1, 1) = body->mass * params.inertia_scale_yy;
    inertia(2, 2) = body->mass * params.inertia_scale_zz;
    body->inertia_tensor = inertia;
    body->inertia_tensor_inv = physics_core::Mat3x3::identity();
    body->inertia_tensor_inv(0, 0) = 1.0f / inertia(0, 0);
    body->inertia_tensor_inv(1, 1) = 1.0f / inertia(1, 1);
    body->inertia_tensor_inv(2, 2) = 1.0f / inertia(2, 2);

    return true;
}

bool HistoricalVehicleSystem::assign_historical_appearance(
    physics_core::EntityID /*vehicle_entity*/,
    const HistoricalVehicleSpecs& specs,
    rendering_engine::RenderingEngine& /*renderer*/
) const {
    return specs.vehicle_name[0] != '\0';
}

// ============================================================================
// PROCEDURAL GENERATION INTEGRATION
// ============================================================================

// Forward declaration for procedural template conversion
procedural_armor_factory::ParametricTankTemplate create_template_from_specs(
    const HistoricalVehicleSpecs& specs
) {
    procedural_armor_factory::ParametricTankTemplate tmpl;
    
    tmpl.hull_length = specs.length_m;
    tmpl.hull_width = specs.width_m;
    tmpl.hull_height = specs.height_m;
    
    // Estimate front armor angle from thickness (simplified)
    tmpl.hull_front_angle = specs.armor.hull_front > 0 ? 30.0f : 0.0f;
    
    // Calculate road wheels count from hull length
    tmpl.road_wheels_count = static_cast<uint8_t>(
        std::clamp(static_cast<int>(specs.length_m / 0.8f), 4, 8)
    );
    
    tmpl.wheel_radius = 0.3f;
    tmpl.track_width = 0.35f;
    tmpl.turret_ring_diameter = specs.width_m * 0.6f;
    tmpl.turret_height = specs.height_m * 0.4f;
    tmpl.gun_caliber = static_cast<float>(specs.weapons.main_gun_caliber_mm);
    tmpl.gun_length = 50.0f;  // nominal 50 calibers
    tmpl.seed = specs.vehicle_id;
    tmpl.era_flags = static_cast<uint8_t>(specs.era);
    tmpl.weight_tons = specs.weight_tons;
    
    return tmpl;
}

bool HistoricalVehicleSystem::create_parametric_vehicle(
    uint32_t vehicle_id,
    const BalanceProfile& balance_profile,
    physics_core::EntityID& output_entity
) {
    const HistoricalVehicleSpecs* specs = database_.get_vehicle_by_id(vehicle_id);
    if (!specs) {
        return false;
    }

    procedural_armor_factory::ParametricTankTemplate tmpl =
        create_template_from_specs(*specs);

    if (!physics_core_) {
        return false;
    }

    PhysicsMappingParameters physics_params;
    if (!physics_mapper_.generate_physics_parameters(*specs, balance_profile, physics_params)) {
        return false;
    }
    calibration_system_.auto_calibrate_for_balance(*specs, physics_params);

    if (!create_parametric_vehicle_from_template(tmpl, balance_profile, *physics_core_, output_entity)) {
        return false;
    }

    return apply_physics_mapping(output_entity, physics_params, *physics_core_);
}

bool HistoricalVehicleSystem::create_parametric_vehicle_from_template(
    const procedural_armor_factory::ParametricTankTemplate& tmpl,
    const BalanceProfile& balance_profile,
    physics_core::PhysicsCore& physics_core,
    physics_core::EntityID& output_entity
) {
    if (!model_system_) {
        return false;
    }

    // Create procedural model
    procedural_armor_factory::ProceduralArmorGenerator generator;
    auto model = generator.generate(tmpl);

    // Register visual mesh asset
    rendering_engine::resource_manager::AssetID mesh_id = rendering_engine::resource_manager::ResourceManager::get_instance()
        .get_asset_manager()
        .register_mesh(model.visual_vertices, model.visual_indices);
    if (mesh_id == 0) {
        return false;
    }
    model.base_model_id = mesh_id;

    // Compute physics parameters
    auto physics_map = procedural_armor_factory::ProceduralArmorGenerator::compute_physics_mapping(tmpl);

    // Create physics body
    output_entity = physics_core.create_rigid_body(
        physics_core::Vec3(
            static_cast<double>(physics_map.center_of_mass.x),
            static_cast<double>(physics_map.center_of_mass.y),
            static_cast<double>(physics_map.center_of_mass.z)
        ),
        physics_map.mass,
        physics_core::Mat3x3(std::array<double, 9>{
            physics_map.inertia_tensor[0][0],
            physics_map.inertia_tensor[0][1],
            physics_map.inertia_tensor[0][2],
            physics_map.inertia_tensor[1][0],
            physics_map.inertia_tensor[1][1],
            physics_map.inertia_tensor[1][2],
            physics_map.inertia_tensor[2][0],
            physics_map.inertia_tensor[2][1],
            physics_map.inertia_tensor[2][2]
        })
    );

    if (output_entity == 0) {
        return false;
    }

    // Attach model to entity and set world transform origin
    model.entity_id = output_entity;
    std::fill(std::begin(model.world_transform.f32), std::end(model.world_transform.f32), 0.0f);
    model.world_transform.f32[0] = 1.0f;
    model.world_transform.f32[5] = 1.0f;
    model.world_transform.f32[10] = 1.0f;
    model.world_transform.f32[15] = 1.0f;
    model.world_transform.f32[12] = physics_map.center_of_mass.x;
    model.world_transform.f32[13] = physics_map.center_of_mass.y;
    model.world_transform.f32[14] = physics_map.center_of_mass.z;

    // Create damage components from mapping data
    auto damage_mappings = generator.create_damage_mapping(model, tmpl);
    physics_core::VehicleDamageState vehicle_damage;
    vehicle_damage.vehicle_entity = output_entity;
    vehicle_damage.overall_mobility = 1.0f;
    vehicle_damage.overall_firepower = 1.0f;
    vehicle_damage.overall_survivability = 1.0f;
    vehicle_damage.cumulative_damage = 0.0f;
    vehicle_damage.cumulative_fire_damage = 0.0f;
    vehicle_damage.status_flags = 0;
    vehicle_damage.is_vehicularly_destroyed = false;
    vehicle_damage.is_combat_efficient = true;
    vehicle_damage.is_immobilized = false;
    vehicle_damage.is_gun_disabled = false;
    vehicle_damage.center_of_damage = _mm256_setzero_ps();
    vehicle_damage.asymmetric_damage_ratio = 0.0f;
    vehicle_damage.is_on_fire = false;
    vehicle_damage.fire_intensity = 0.0f;
    vehicle_damage.fire_spread_rate = 0.0f;

    for (const auto& mapping : damage_mappings) {
        physics_core::ComponentDamageState component_state{};
        component_state.component_entity = output_entity;
        component_state.current_health = 1.0f;
        component_state.max_health = 1.0f;
        component_state.damage_type_flags = 0;
        component_state.structural_integrity = 1.0f;
        component_state.armor_thickness = mapping.armor_thickness;
        component_state.armor_hardness = 1.0f;
        component_state.damage_level = 0;
        component_state.cumulative_stress = 0.0f;
        component_state.status_flags = 0;
        component_state.last_damage_time = 0.0f;
        component_state.fire_start_time = 0.0f;
        component_state.total_damage_received = 0.0f;
        component_state.hit_count = 0;
        component_state.reactive_tile_coverage = 0.0f;
        component_state.reactive_tiles_triggered = false;
        component_state.damage_position = _mm256_setzero_ps();
        component_state.damage_normal = _mm256_setzero_ps();

        vehicle_damage.component_damage_states.push_back(component_state);

        rendering_engine::ComponentState visual_state{};
        visual_state.component_index = static_cast<uint32_t>(mapping.component);
        visual_state.health_current = 1.0f;
        visual_state.is_destroyed = false;
        model.component_states.push_back(std::move(visual_state));
    }

    vehicle_damage_registry_[output_entity] = std::move(vehicle_damage);

    // Create an instance in the model system and transfer generated instance data
    uint64_t instance_id = model_system_->create_instance(output_entity, mesh_id);
    rendering_engine::ModelInstance* registered_instance = model_system_->get_instance(instance_id);
    if (!registered_instance) {
        return false;
    }

    *registered_instance = std::move(model);
    model_system_->sync_model_to_physics(*registered_instance, physics_core);

    return true;
}

} // namespace historical_vehicle_system
