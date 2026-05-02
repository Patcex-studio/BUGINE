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
#include <vector>
#include <string>
#include <array>

namespace ptu_system {

struct ContentDiff;

// Content version
struct ContentVersion {
    uint64_t version_id = 0;            // Unique version identifier
    uint64_t package_id = 0;            // Associated package
    uint32_t version_number = 0;        // Incremental version number
    std::string commit_message;         // Commit message (max 256 chars)
    uint32_t author_id = 0;             // Author identifier
    uint32_t timestamp = 0;             // Commit timestamp
    uint64_t parent_version_id = 0;     // Previous version (for branching)
    std::vector<ContentDiff> changes;   // Changes in this version
    std::array<char, 64> checksum;      // SHA-256 checksum of changes
    bool is_published = false;          // Published to users
    bool is_beta = false;               // Beta version
    bool is_stable = false;             // Stable release
};

// Content diff
struct ContentDiff {
    enum class DiffType { ADDITION, DELETION, MODIFICATION };
    DiffType diff_type;
    std::string file_path;          // Affected file path
    std::vector<uint8_t> old_content; // Old content (for modifications)
    std::vector<uint8_t> new_content; // New content
    uint32_t line_numbers_changed = 0;  // Line numbers affected
    std::string description;        // Description of changes
};

// Contributor
struct Contributor {
    uint32_t user_id;
    std::string username;
    std::string role; // "owner", "contributor", "reviewer"
};

// Permission level
enum class PermissionLevel {
    NONE,
    READ,
    WRITE,
    ADMIN
};

// Conflict resolution
struct ConflictResolution {
    std::string file_path;
    std::vector<uint8_t> resolved_content;
    std::string resolution_description;
};

// Content version control class
class ContentVersionControl {
public:
    ContentVersionControl();
    ~ContentVersionControl();

    // Repository management
    bool create_repository(const std::string& repo_path);
    bool clone_repository(const std::string& remote_url, const std::string& local_path);
    bool pull_repository(const std::string& repo_path);
    bool push_repository(const std::string& repo_path);

    // Version operations
    ContentVersion create_version(const ContentPackage& package, const std::string& commit_message);
    ContentVersion get_version(uint64_t version_id);
    std::vector<ContentVersion> get_version_history(uint64_t package_id);
    bool checkout_version(uint64_t version_id, ContentPackage& output_package);

    // Branch and merge operations
    bool create_branch(uint64_t package_id, const std::string& branch_name);
    bool merge_branches(const std::string& source_branch, const std::string& target_branch);
    bool resolve_conflicts(const std::vector<ConflictResolution>& resolutions);

    // Collaboration features
    bool assign_contributor(uint64_t package_id, uint32_t user_id);
    bool revoke_contributor(uint64_t package_id, uint32_t user_id);
    std::vector<Contributor> get_contributors(uint64_t package_id);

    // Security and permissions
    bool set_permissions(uint64_t package_id, uint32_t user_id, PermissionLevel level);
    PermissionLevel get_permissions(uint64_t package_id, uint32_t user_id);
    bool validate_permission(uint64_t package_id, uint32_t user_id, PermissionLevel required);

private:
    // Helper functions
    uint64_t generate_version_id();
    std::array<char, 64> calculate_checksum(const ContentPackage& package);
    std::vector<ContentDiff> calculate_diff(const ContentPackage& old_package, const ContentPackage& new_package);
    bool save_version_to_storage(const ContentVersion& version);
    ContentVersion load_version_from_storage(uint64_t version_id);
};

} // namespace ptu_system