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
#include <sqlite3.h>
#include <mutex>
#include <vector>
#include <string>

namespace ptu_system {

// Search criteria
struct SearchCriteria {
    std::string query;
    ContentType content_type = ContentType::OTHER;
    uint32_t creator_id = 0;
    bool approved_only = false;
    float min_rating = 0.0f;
};

// Validation record
struct ValidationRecord {
    uint64_t package_id;
    uint32_t timestamp;
    ContentValidationResult result;
};

// User review
struct UserReview {
    uint32_t user_id;
    std::string review_text;
    uint32_t timestamp;
    float rating;
};

// Content database
class ContentDatabase {
public:
    ContentDatabase();
    ~ContentDatabase();

    // Database management
    bool initialize(const std::string& db_path);
    bool close();

    // Package management
    bool insert_package(const ContentPackage& package);
    bool update_package(const ContentPackage& package);
    bool delete_package(uint64_t package_id);
    ContentPackage get_package(uint64_t package_id);
    std::vector<ContentPackage> get_packages_by_creator(uint32_t creator_id);
    std::vector<ContentPackage> search_packages(const SearchCriteria& criteria);

    // Validation tracking
    bool record_validation_result(uint64_t package_id, const ContentValidationResult& result);
    std::vector<ValidationRecord> get_validation_history(uint64_t package_id);

    // User ratings and reviews
    bool record_user_rating(uint64_t package_id, uint32_t user_id, float rating);
    bool record_user_review(uint64_t package_id, uint32_t user_id, const std::string& review);
    float get_average_rating(uint64_t package_id);
    std::vector<UserReview> get_reviews(uint64_t package_id);

private:
    sqlite3* db_handle = nullptr;
    std::mutex database_mutex;

    // Helper functions
    bool create_tables();
    bool execute_query(const std::string& query);
    sqlite3_stmt* prepare_statement(const std::string& query);
    ContentPackage package_from_row(sqlite3_stmt* stmt);
};

} // namespace ptu_system