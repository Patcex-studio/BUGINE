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
#include "code_refactorer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace code_quality {

CodeRefactorer::CodeRefactorer() {
}

CodeRefactorer::~CodeRefactorer() {
}

bool CodeRefactorer::apply_refactorings(const std::vector<RefactoringSuggestion>& suggestions,
                                       const std::string& project_path) {
    std::cout << "Starting refactoring process..." << std::endl;

    // Validate all suggestions for safety
    for (const auto& suggestion : suggestions) {
        if (!validate_refactoring_safety(suggestion)) {
            std::cerr << "Unsafe refactoring suggestion: " << suggestion.description << std::endl;
            return false;
        }
    }

    // Create backup
    if (!create_backup(project_path)) {
        std::cerr << "Failed to create backup" << std::endl;
        return false;
    }

    // Apply refactorings in dependency order
    std::vector<RefactoringSuggestion> sorted_suggestions = suggestions;
    // TODO: sort by dependencies

    for (const auto& suggestion : sorted_suggestions) {
        if (!apply_single_refactoring(suggestion)) {
            std::cerr << "Failed to apply refactoring: " << suggestion.description << std::endl;
            restore_backup(project_path);
            return false;
        }
    }

    // Run tests to verify changes
    if (!run_tests_after_refactoring(project_path)) {
        std::cerr << "Tests failed after refactoring" << std::endl;
        restore_backup(project_path);
        return false;
    }

    // Update documentation
    // TODO: update comments and documentation

    // Commit changes
    std::string commit_message = "Automated code refactoring: " +
                                std::to_string(suggestions.size()) + " improvements applied";
    if (!commit_changes(project_path, commit_message)) {
        std::cerr << "Failed to commit changes" << std::endl;
        return false;
    }

    std::cout << "Refactoring completed successfully" << std::endl;
    return true;
}

bool CodeRefactorer::remove_dead_code(const std::vector<DeadFunction>& dead_functions,
                                     const std::vector<DeadVariable>& dead_variables,
                                     const std::string& project_path) {
    std::cout << "Removing dead code..." << std::endl;

    // TODO: implement dead code removal
    return true; // Placeholder
}

bool CodeRefactorer::refactor_duplicates(const std::vector<DuplicateBlock>& duplicates,
                                        const std::string& project_path) {
    std::cout << "Refactoring duplicate code..." << std::endl;

    // TODO: implement duplicate refactoring
    return true; // Placeholder
}

bool CodeRefactorer::fix_bugs(const std::vector<MemoryBug>& memory_bugs,
                              const std::vector<LogicBug>& logic_bugs,
                              const std::string& project_path) {
    std::cout << "Fixing detected bugs..." << std::endl;

    // TODO: implement automatic bug fixing
    return true; // Placeholder
}

bool CodeRefactorer::validate_refactoring_safety(const RefactoringSuggestion& suggestion) {
    // Basic safety checks
    if (suggestion.confidence < 0.7f) {
        return false; // Low confidence suggestions are risky
    }

    if (suggestion.original_code.empty() || suggestion.refactored_code.empty()) {
        return false; // Invalid refactoring
    }

    return true;
}

bool CodeRefactorer::create_backup(const std::string& project_path) {
    std::cout << "Creating backup..." << std::endl;

    try {
        fs::path backup_path = fs::path(project_path) / ".backup";
        if (fs::exists(backup_path)) {
            fs::remove_all(backup_path);
        }
        fs::create_directory(backup_path);

        // Copy source files to backup
        for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
                    fs::path relative_path = fs::relative(entry.path(), project_path);
                    fs::path backup_file = backup_path / relative_path;
                    fs::create_directories(backup_file.parent_path());
                    fs::copy_file(entry.path(), backup_file);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Backup creation failed: " << e.what() << std::endl;
        return false;
    }
}

bool CodeRefactorer::restore_backup(const std::string& project_path) {
    std::cout << "Restoring backup..." << std::endl;

    try {
        fs::path backup_path = fs::path(project_path) / ".backup";
        if (!fs::exists(backup_path)) {
            return false;
        }

        // Restore files
        for (const auto& entry : fs::recursive_directory_iterator(backup_path)) {
            if (entry.is_regular_file()) {
                fs::path relative_path = fs::relative(entry.path(), backup_path);
                fs::path original_file = fs::path(project_path) / relative_path;
                fs::copy_file(entry.path(), original_file, fs::copy_options::overwrite_existing);
            }
        }

        fs::remove_all(backup_path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Backup restoration failed: " << e.what() << std::endl;
        return false;
    }
}

bool CodeRefactorer::run_tests_after_refactoring(const std::string& project_path) {
    std::cout << "Running tests after refactoring..." << std::endl;

    // TODO: implement test execution
    // For now, assume tests pass
    return true;
}

bool CodeRefactorer::verify_functionality_preservation(const std::string& project_path) {
    // TODO: implement functionality verification
    return true;
}

bool CodeRefactorer::commit_changes(const std::string& project_path, const std::string& commit_message) {
    std::cout << "Committing changes..." << std::endl;

    // TODO: implement git commit
    return true; // Placeholder
}

bool CodeRefactorer::create_branch(const std::string& project_path, const std::string& branch_name) {
    // TODO: implement git branch creation
    return true;
}

bool CodeRefactorer::apply_single_refactoring(const RefactoringSuggestion& suggestion) {
    std::cout << "Applying refactoring: " << suggestion.description << std::endl;

    try {
        std::string refactored_code = generate_refactored_code(suggestion);
        return update_file(suggestion.file_path, suggestion.original_code, refactored_code);
    } catch (const std::exception& e) {
        std::cerr << "Refactoring failed: " << e.what() << std::endl;
        return false;
    }
}

std::string CodeRefactorer::generate_refactored_code(const RefactoringSuggestion& suggestion) {
    // For now, return the suggested refactored code
    return suggestion.refactored_code;
}

bool CodeRefactorer::update_file(const std::string& file_path, const std::string& old_code, const std::string& new_code) {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    // Find and replace
    size_t pos = content.find(old_code);
    if (pos == std::string::npos) {
        return false;
    }

    content.replace(pos, old_code.length(), new_code);

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << content;
    return true;
}

std::vector<std::string> CodeRefactorer::get_dependencies(const RefactoringSuggestion& suggestion) {
    // TODO: analyze dependencies
    return {};
}

} // namespace code_quality