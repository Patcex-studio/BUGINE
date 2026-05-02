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
#include "content_database.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

namespace ptu_system {

ContentDatabase::ContentDatabase() = default;

ContentDatabase::~ContentDatabase() {
    close();
}

bool ContentDatabase::initialize(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_handle) != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_handle) << std::endl;
        return false;
    }
    return create_tables();
}

bool ContentDatabase::close() {
    if (db_handle) {
        sqlite3_close(db_handle);
        db_handle = nullptr;
    }
    return true;
}

bool ContentDatabase::insert_package(const ContentPackage& package) {
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = R"(
        INSERT INTO packages (package_id, package_name, package_description, package_version,
                             content_type, creator_id, creation_timestamp, last_modified_timestamp,
                             signature_hash, encryption_key, security_level, compressed_data,
                             uncompressed_size, compressed_size, file_count, validation_score,
                             is_approved, is_banned)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return false;

    // Bind parameters (simplified, need to handle arrays properly)
    sqlite3_bind_int64(stmt, 1, package.package_id);
    sqlite3_bind_text(stmt, 2, package.package_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, package.package_description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, package.package_version);
    sqlite3_bind_int(stmt, 5, static_cast<int>(package.content_type));
    sqlite3_bind_int(stmt, 6, package.creator_id);
    sqlite3_bind_int(stmt, 7, package.creation_timestamp);
    sqlite3_bind_int(stmt, 8, package.last_modified_timestamp);
    sqlite3_bind_blob(stmt, 9, package.signature_hash.data(), package.signature_hash.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 10, package.encryption_key.data(), package.encryption_key.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, static_cast<int>(package.security_level));
    sqlite3_bind_blob(stmt, 12, package.compressed_data.data(), package.compressed_data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, package.uncompressed_size);
    sqlite3_bind_int(stmt, 14, package.compressed_size);
    sqlite3_bind_int(stmt, 15, package.file_count);
    sqlite3_bind_double(stmt, 16, package.validation_score);
    sqlite3_bind_int(stmt, 17, package.is_approved ? 1 : 0);
    sqlite3_bind_int(stmt, 18, package.is_banned ? 1 : 0);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

bool ContentDatabase::update_package(const ContentPackage& package) {
    // Similar to insert, but UPDATE
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = R"(
        UPDATE packages SET package_name=?, package_description=?, package_version=?,
                           content_type=?, last_modified_timestamp=?, signature_hash=?,
                           encryption_key=?, security_level=?, compressed_data=?,
                           uncompressed_size=?, compressed_size=?, file_count=?,
                           validation_score=?, is_approved=?, is_banned=?
        WHERE package_id=?
    )";

    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return false;

    // Bind parameters
    sqlite3_bind_text(stmt, 1, package.package_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, package.package_description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, package.package_version);
    sqlite3_bind_int(stmt, 4, static_cast<int>(package.content_type));
    sqlite3_bind_int(stmt, 5, package.last_modified_timestamp);
    sqlite3_bind_blob(stmt, 6, package.signature_hash.data(), package.signature_hash.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 7, package.encryption_key.data(), package.encryption_key.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, static_cast<int>(package.security_level));
    sqlite3_bind_blob(stmt, 9, package.compressed_data.data(), package.compressed_data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, package.uncompressed_size);
    sqlite3_bind_int(stmt, 11, package.compressed_size);
    sqlite3_bind_int(stmt, 12, package.file_count);
    sqlite3_bind_double(stmt, 13, package.validation_score);
    sqlite3_bind_int(stmt, 14, package.is_approved ? 1 : 0);
    sqlite3_bind_int(stmt, 15, package.is_banned ? 1 : 0);
    sqlite3_bind_int64(stmt, 16, package.package_id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

bool ContentDatabase::delete_package(uint64_t package_id) {
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = "DELETE FROM packages WHERE package_id=?";
    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, package_id);
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

ContentPackage ContentDatabase::get_package(uint64_t package_id) {
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = "SELECT * FROM packages WHERE package_id=?";
    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return {};

    sqlite3_bind_int64(stmt, 1, package_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ContentPackage package = package_from_row(stmt);
        sqlite3_finalize(stmt);
        return package;
    }
    sqlite3_finalize(stmt);
    return {};
}

std::vector<ContentPackage> ContentDatabase::get_packages_by_creator(uint32_t creator_id) {
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = "SELECT * FROM packages WHERE creator_id=?";
    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return {};

    sqlite3_bind_int(stmt, 1, creator_id);
    std::vector<ContentPackage> packages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        packages.push_back(package_from_row(stmt));
    }
    sqlite3_finalize(stmt);
    return packages;
}

std::vector<ContentPackage> ContentDatabase::search_packages(const SearchCriteria& criteria) {
    // Simplified search implementation
    std::lock_guard<std::mutex> lock(database_mutex);
    std::string query = "SELECT * FROM packages WHERE 1=1";
    if (!criteria.query.empty()) {
        query += " AND (package_name LIKE ? OR package_description LIKE ?)";
    }
    if (criteria.content_type != ContentType::OTHER) {
        query += " AND content_type=?";
    }
    if (criteria.creator_id > 0) {
        query += " AND creator_id=?";
    }
    if (criteria.approved_only) {
        query += " AND is_approved=1";
    }

    sqlite3_stmt* stmt = prepare_statement(query);
    if (!stmt) return {};

    int param_index = 1;
    if (!criteria.query.empty()) {
        std::string like_query = "%" + criteria.query + "%";
        sqlite3_bind_text(stmt, param_index++, like_query.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, param_index++, like_query.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (criteria.content_type != ContentType::OTHER) {
        sqlite3_bind_int(stmt, param_index++, static_cast<int>(criteria.content_type));
    }
    if (criteria.creator_id > 0) {
        sqlite3_bind_int(stmt, param_index++, criteria.creator_id);
    }

    std::vector<ContentPackage> packages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        packages.push_back(package_from_row(stmt));
    }
    sqlite3_finalize(stmt);
    return packages;
}

bool ContentDatabase::record_validation_result(uint64_t package_id, const ContentValidationResult& result) {
    // Simplified: insert into validation_history table
    return true; // Placeholder
}

std::vector<ValidationRecord> ContentDatabase::get_validation_history(uint64_t package_id) {
    // Placeholder
    return {};
}

bool ContentDatabase::record_user_rating(uint64_t package_id, uint32_t user_id, float rating) {
    // Placeholder
    return true;
}

bool ContentDatabase::record_user_review(uint64_t package_id, uint32_t user_id, const std::string& review) {
    // Placeholder
    return true;
}

float ContentDatabase::get_average_rating(uint64_t package_id) {
    // Placeholder
    return 4.5f;
}

std::vector<UserReview> ContentDatabase::get_reviews(uint64_t package_id) {
    // Placeholder
    return {};
}

bool ContentDatabase::create_tables() {
    std::string create_packages_table = R"(
        CREATE TABLE IF NOT EXISTS packages (
            package_id INTEGER PRIMARY KEY,
            package_name TEXT NOT NULL,
            package_description TEXT,
            package_version INTEGER,
            content_type INTEGER,
            creator_id INTEGER,
            creation_timestamp INTEGER,
            last_modified_timestamp INTEGER,
            signature_hash BLOB,
            encryption_key BLOB,
            security_level INTEGER,
            compressed_data BLOB,
            uncompressed_size INTEGER,
            compressed_size INTEGER,
            file_count INTEGER,
            validation_score REAL,
            is_approved INTEGER,
            is_banned INTEGER
        )
    )";

    return execute_query(create_packages_table);
}

bool ContentDatabase::execute_query(const std::string& query) {
    char* error_msg = nullptr;
    int result = sqlite3_exec(db_handle, query.c_str(), nullptr, nullptr, &error_msg);
    if (result != SQLITE_OK) {
        std::cerr << "SQL error: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    return true;
}

sqlite3_stmt* ContentDatabase::prepare_statement(const std::string& query) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_handle, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_handle) << std::endl;
        return nullptr;
    }
    return stmt;
}

ContentPackage ContentDatabase::package_from_row(sqlite3_stmt* stmt) {
    ContentPackage package;
    package.package_id = sqlite3_column_int64(stmt, 0);
    package.package_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    package.package_description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    package.package_version = sqlite3_column_int(stmt, 3);
    package.content_type = static_cast<ContentType>(sqlite3_column_int(stmt, 4));
    package.creator_id = sqlite3_column_int(stmt, 5);
    package.creation_timestamp = sqlite3_column_int(stmt, 6);
    package.last_modified_timestamp = sqlite3_column_int(stmt, 7);

    // Handle BLOBs
    const void* sig_hash = sqlite3_column_blob(stmt, 8);
    int sig_size = sqlite3_column_bytes(stmt, 8);
    if (sig_hash && sig_size == 64) {
        std::memcpy(package.signature_hash.data(), sig_hash, 64);
    }

    const void* enc_key = sqlite3_column_blob(stmt, 9);
    int enc_size = sqlite3_column_bytes(stmt, 9);
    if (enc_key && enc_size == 32) {
        std::memcpy(package.encryption_key.data(), enc_key, 32);
    }

    package.security_level = static_cast<SecurityLevel>(sqlite3_column_int(stmt, 10));

    const void* comp_data = sqlite3_column_blob(stmt, 11);
    int comp_size = sqlite3_column_bytes(stmt, 11);
    if (comp_data && comp_size > 0) {
        package.compressed_data.assign(static_cast<const uint8_t*>(comp_data),
                                     static_cast<const uint8_t*>(comp_data) + comp_size);
    }

    package.uncompressed_size = sqlite3_column_int(stmt, 12);
    package.compressed_size = sqlite3_column_int(stmt, 13);
    package.file_count = sqlite3_column_int(stmt, 14);
    package.validation_score = sqlite3_column_double(stmt, 15);
    package.is_approved = sqlite3_column_int(stmt, 16) != 0;
    package.is_banned = sqlite3_column_int(stmt, 17) != 0;

    return package;
}

} // namespace ptu_system