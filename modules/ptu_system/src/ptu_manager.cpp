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
#include "ptu_manager.hpp"
#include <iostream>

namespace ptu_system {

PTUManager::PTUManager() = default;

PTUManager::~PTUManager() = default;

bool PTUManager::initialize(const std::string& db_path, const std::string& repo_path) {
    if (!content_database_.initialize(db_path)) {
        std::cerr << "Failed to initialize content database" << std::endl;
        return false;
    }

    if (!version_control_.create_repository(repo_path)) {
        std::cerr << "Failed to create version control repository" << std::endl;
        return false;
    }

    return true;
}

SubmissionResponse PTUManager::submit_content_package(const ContentPackage& submitted_package) {
    SubmissionResponse response;
    if (!validate_submission(submitted_package, response)) {
        return response;
    }

    // Generate package ID
    static uint64_t package_id_counter = 1;
    ContentPackage processed_package = submitted_package;
    processed_package.package_id = package_id_counter++;

    // Validate security
    if (!security_validator_.validate_no_malicious_code(processed_package) ||
        !security_validator_.verify_signature_integrity(processed_package)) {
        response.success = false;
        response.message = "Security validation failed";
        return response;
    }

    // Integrate package
    processed_package = integration_pipeline_.preprocess_package(processed_package);
    if (!integration_pipeline_.integrate_package_into_game(processed_package)) {
        response.success = false;
        response.message = "Integration failed";
        return response;
    }

    // Store in database
    if (!content_database_.insert_package(processed_package)) {
        response.success = false;
        response.message = "Database storage failed";
        return response;
    }

    // Create initial version
    version_control_.create_version(processed_package, "Initial submission");

    response.success = true;
    response.message = "Package submitted successfully";
    response.package_id = processed_package.package_id;
    response.validation_result = processed_package.validation_result;

    return response;
}

ModerationDecision PTUManager::moderate_content_package(uint64_t package_id) {
    ModerationDecision decision;
    return perform_moderation(package_id, decision) ? decision : ModerationDecision{};
}

bool PTUManager::distribute_content_to_users(const std::vector<uint64_t>& package_ids,
                                           const std::vector<uint32_t>& user_ids) {
    for (uint64_t package_id : package_ids) {
        for (uint32_t user_id : user_ids) {
            if (!distribute_package(package_id, user_id)) {
                return false;
            }
        }
    }
    return true;
}

bool PTUManager::export_from_editor_to_package(const std::string& editor_content,
                                             ContentPackage& output_package) {
    // Placeholder: Convert editor content to package
    output_package.package_name = "Exported Package";
    output_package.compressed_data.assign(editor_content.begin(), editor_content.end());
    output_package.uncompressed_size = editor_content.size();
    output_package.compressed_size = editor_content.size(); // No compression
    return true;
}

bool PTUManager::synchronize_content_with_server(uint64_t package_id) {
    // Placeholder: Sync with server
    return true;
}

AccessPermission PTUManager::validate_user_content_access(uint32_t user_id, uint64_t package_id) {
    AccessPermission permission;
    permission.can_access = version_control_.validate_permission(package_id, user_id, PermissionLevel::READ);
    permission.level = version_control_.get_permissions(package_id, user_id);
    return permission;
}

bool PTUManager::create_version(uint64_t package_id, const std::string& commit_message) {
    ContentPackage package = content_database_.get_package(package_id);
    if (package.package_id == 0) return false;

    return version_control_.create_version(package, commit_message).version_id != 0;
}

bool PTUManager::checkout_version(uint64_t version_id, ContentPackage& output_package) {
    return version_control_.checkout_version(version_id, output_package);
}

bool PTUManager::validate_submission(const ContentPackage& package, SubmissionResponse& response) {
    if (package.package_name.empty()) {
        response.message = "Package name is required";
        return false;
    }

    if (package.creator_id == 0) {
        response.message = "Creator ID is required";
        return false;
    }

    response.success = true;
    return true;
}

bool PTUManager::perform_moderation(uint64_t package_id, ModerationDecision& decision) {
    ContentPackage package = content_database_.get_package(package_id);
    if (package.package_id == 0) return false;

    // Automated checks
    bool security_ok = security_validator_.validate_no_malicious_code(package);
    bool balance_ok = security_validator_.validate_content_balance(package);

    decision.approved = security_ok && balance_ok;
    decision.reason = decision.approved ? "Approved by automated moderation" : "Failed automated checks";
    decision.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (decision.approved) {
        package.is_approved = true;
        content_database_.update_package(package);
    }

    return true;
}

bool PTUManager::distribute_package(uint64_t package_id, uint32_t user_id) {
    AccessPermission access = validate_user_content_access(user_id, package_id);
    if (!access.can_access) return false;

    // Placeholder: Actual distribution logic
    return true;
}

} // namespace ptu_system