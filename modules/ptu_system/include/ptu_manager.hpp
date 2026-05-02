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

#include "content_package.hpp"
#include "content_security_validator.hpp"
#include "content_integration_pipeline.hpp"
#include "content_database.hpp"
#include "content_version_control.hpp"
#include <string>

namespace ptu_system {

// Submission response
struct SubmissionResponse {
    bool success = false;
    std::string message;
    uint64_t package_id = 0;
    ContentValidationResult validation_result;
};

// Moderation decision
struct ModerationDecision {
    bool approved = false;
    std::string reason;
    std::vector<std::string> issues;
    uint32_t moderator_id = 0;
    uint32_t timestamp = 0;
};

// Access permission
struct AccessPermission {
    bool can_access = false;
    PermissionLevel level = PermissionLevel::NONE;
    std::string reason;
};

// PTU Manager - main interface
class PTUManager {
public:
    PTUManager();
    ~PTUManager();

    // Initialization
    bool initialize(const std::string& db_path, const std::string& repo_path);

    // Content submission
    SubmissionResponse submit_content_package(const ContentPackage& submitted_package);

    // Content moderation
    ModerationDecision moderate_content_package(uint64_t package_id);

    // Content distribution
    bool distribute_content_to_users(const std::vector<uint64_t>& package_ids,
                                   const std::vector<uint32_t>& user_ids);

    // Editor integration
    bool export_from_editor_to_package(const std::string& editor_content,
                                    ContentPackage& output_package);

    // Network synchronization
    bool synchronize_content_with_server(uint64_t package_id);

    // User access validation
    AccessPermission validate_user_content_access(uint32_t user_id, uint64_t package_id);

    // Version control operations
    bool create_version(uint64_t package_id, const std::string& commit_message);
    bool checkout_version(uint64_t version_id, ContentPackage& output_package);

private:
    ContentSecurityValidator security_validator_;
    ContentIntegrationPipeline integration_pipeline_;
    ContentDatabase content_database_;
    ContentVersionControl version_control_;

    // Helper functions
    bool validate_submission(const ContentPackage& package, SubmissionResponse& response);
    bool perform_moderation(uint64_t package_id, ModerationDecision& decision);
    bool distribute_package(uint64_t package_id, uint32_t user_id);
};

} // namespace ptu_system