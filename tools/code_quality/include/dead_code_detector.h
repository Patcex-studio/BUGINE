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
#include <unordered_map>

namespace clang {
class Decl;
class ASTContext;
} // namespace clang

namespace code_quality {

class DeadCodeDetector {
public:
    DeadCodeDetector();
    ~DeadCodeDetector();

    // Main analysis functions
    DeadCodeAnalysisResult analyze_project(const std::string& project_path);

    // Individual detection methods
    std::vector<DeadFunction> find_unused_functions(const std::string& project_path);
    std::vector<DeadVariable> find_unused_variables(const std::string& project_path);
    std::vector<DeadClass> find_unused_classes(const std::string& project_path);
    std::vector<DeadInclude> find_unused_includes(const std::string& project_path);
    std::vector<DeadCodeBlock> find_unreachable_code(const std::string& project_path);

private:
    // AST-based analysis
    bool analyze_ast_for_usage(const clang::Decl* decl, const std::string& project_path);
    bool is_function_referenced(const std::string& function_name, const std::string& project_path);
    bool is_variable_referenced(const std::string& variable_name, const std::string& project_path);

    // Call graph analysis
    std::unordered_map<std::string, std::vector<std::string>> build_call_graph(const std::string& project_path);
    std::vector<std::string> find_unreachable_functions(const std::string& entry_point, const std::unordered_map<std::string, std::vector<std::string>>& call_graph);

    // Template and macro handling
    bool is_template_instantiated(const std::string& template_name, const std::string& project_path);
    bool is_macro_used(const std::string& macro_name, const std::string& project_path);

    // Helper methods
    std::vector<std::string> get_all_source_files(const std::string& project_path);
    std::string extract_function_signature(const std::string& code, const std::string& function_name);
    uint32_t calculate_function_size(const std::string& code);
};

} // namespace code_quality