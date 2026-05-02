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
#include <cassert>

int main() {
    using namespace ptu_system;

    // Initialize PTU Manager
    PTUManager manager;
    if (!manager.initialize("test_ptu.db", "test_repo")) {
        std::cerr << "Failed to initialize PTU Manager" << std::endl;
        return 1;
    }

    // Create a test content package
    ContentPackage package;
    package.package_name = "Test Vehicle Blueprint";
    package.package_description = "A test vehicle blueprint for PTU system";
    package.content_type = ContentType::VEHICLE_BLUEPRINT;
    package.creator_id = 12345;
    package.creation_timestamp = 1640995200; // 2022-01-01
    package.compressed_data = {'t', 'e', 's', 't', ' ', 'd', 'a', 't', 'a'};
    package.uncompressed_size = 9;
    package.compressed_size = 9;
    package.file_count = 1;
    package.validation_score = 0.95f;

    // Submit the package
    SubmissionResponse response = manager.submit_content_package(package);
    if (!response.success) {
        std::cerr << "Failed to submit package: " << response.message << std::endl;
        return 1;
    }

    std::cout << "Package submitted successfully with ID: " << response.package_id << std::endl;

    // Moderate the package
    ModerationDecision decision = manager.moderate_content_package(response.package_id);
    if (decision.approved) {
        std::cout << "Package approved: " << decision.reason << std::endl;
    } else {
        std::cout << "Package rejected: " << decision.reason << std::endl;
    }

    // Test version control
    if (manager.create_version(response.package_id, "Test commit message")) {
        std::cout << "Version created successfully" << std::endl;
    }

    // Test access validation
    AccessPermission access = manager.validate_user_content_access(12345, response.package_id);
    if (access.can_access) {
        std::cout << "User has access to package" << std::endl;
    }

    std::cout << "PTU System test completed successfully" << std::endl;
    return 0;
}