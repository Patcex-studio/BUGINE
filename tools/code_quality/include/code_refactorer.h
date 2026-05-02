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

#include "code_quality_types.h"
#include <string>
#include <vector>

namespace code_quality {

class CodeRefactorer {
public:
    CodeRefactorer();
    ~CodeRefactorer();

    // Main refactoring function
    bool apply_refactorings(const std::vector<RefactoringSuggestion>& suggestions,
                           const std::string& project_path);

    // Individual refactoring operations
    bool remove_dead_code(const std::vector<DeadFunction>& dead_functions,
                         const std::vector<DeadVariable>& dead_variables,
                         const std::string& project_path);

    bool refactor_duplicates(const std::vector<DuplicateBlock>& duplicates,
                           const std::string& project_path);

    bool fix_bugs(const std::vector<MemoryBug>& memory_bugs,
                 const std::vector<LogicBug>& logic_bugs,
                 const std::string& project_path);

    // Safety validation
    bool validate_refactoring_safety(const RefactoringSuggestion& suggestion);
    bool create_backup(const std::string& project_path);
    bool restore_backup(const std::string& project_path);

    // Testing integration
    bool run_tests_after_refactoring(const std::string& project_path);
    bool verify_functionality_preservation(const std::string& project_path);

    // Version control integration
    bool commit_changes(const std::string& project_path, const std::string& commit_message);
    bool create_branch(const std::string& project_path, const std::string& branch_name);

private:
    // Helper methods
    bool apply_single_refactoring(const RefactoringSuggestion& suggestion);
    std::string generate_refactored_code(const RefactoringSuggestion& suggestion);
    bool update_file(const std::string& file_path, const std::string& old_code, const std::string& new_code);
    std::vector<std::string> get_dependencies(const RefactoringSuggestion& suggestion);
};

} // namespace code_quality