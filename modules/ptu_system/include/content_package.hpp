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

#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace ptu_system {

// Security levels
enum class SecurityLevel {
    LOW,
    MEDIUM,
    HIGH
};

// Content types
enum class ContentType {
    VEHICLE_BLUEPRINT,
    MISSION,
    MAP,
    MOD,
    OTHER
};

// Security tags
struct SecurityTag {
    std::string tag_name;
    std::string description;
};

// Validation result
struct ContentValidationResult {
    bool is_valid = false;
    float score = 0.0f;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Validation error
struct ValidationError {
    std::string error_code;
    std::string description;
    std::string severity; // "low", "medium", "high", "critical"
};

// Content package structure
struct ContentPackage {
    uint64_t package_id = 0;            // Unique package identifier
    std::string package_name;           // Package name (max 128 chars)
    std::string package_description;    // Package description (max 512 chars)
    uint32_t package_version = 0;       // Version number
    ContentType content_type = ContentType::OTHER; // Content type
    uint32_t creator_id = 0;            // Creator identifier
    uint32_t creation_timestamp = 0;    // Creation time
    uint32_t last_modified_timestamp = 0; // Last modification time

    // Security information
    std::array<char, 64> signature_hash; // SHA-256 signature hash
    std::array<char, 32> encryption_key; // Encryption key (if encrypted)
    SecurityLevel security_level = SecurityLevel::MEDIUM;
    std::vector<SecurityTag> security_tags; // Security classification tags

    // Content data
    std::vector<uint8_t> compressed_data; // Compressed content data
    uint32_t uncompressed_size = 0;     // Size when uncompressed
    uint32_t compressed_size = 0;       // Size when compressed
    uint32_t file_count = 0;            // Number of files in package

    // Validation information
    ContentValidationResult validation_result; // Validation status
    std::vector<ValidationError> validation_errors; // Validation errors
    float validation_score = 0.0f;      // Overall validation score (0.0-1.0)
    bool is_approved = false;           // Approved for distribution
    bool is_banned = false;             // Banned content
};

} // namespace ptu_system