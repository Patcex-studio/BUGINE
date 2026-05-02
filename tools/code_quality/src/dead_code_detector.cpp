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
#include "dead_code_detector.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/CommandLine.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <queue>
#include <unordered_set>

namespace fs = std::filesystem;

namespace code_quality {

DeadCodeDetector::DeadCodeDetector() {
    // Initialize Clang tooling if needed
}

DeadCodeDetector::~DeadCodeDetector() {
    // Cleanup
}

DeadCodeAnalysisResult DeadCodeDetector::analyze_project(const std::string& project_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    DeadCodeAnalysisResult result;

    result.dead_functions = find_unused_functions(project_path);
    result.dead_variables = find_unused_variables(project_path);
    result.dead_classes = find_unused_classes(project_path);
    result.dead_includes = find_unused_includes(project_path);
    result.dead_code_blocks = find_unreachable_code(project_path);

    result.total_dead_functions = result.dead_functions.size();
    result.total_dead_variables = result.dead_variables.size();
    result.total_dead_classes = result.dead_classes.size();

    // Calculate percentage (simplified)
    result.dead_code_percentage = 0.0f; // TODO: implement proper calculation

    auto end_time = std::chrono::high_resolution_clock::now();
    result.analysis_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

std::vector<DeadFunction> DeadCodeDetector::find_unused_functions(const std::string& project_path) {
    std::vector<DeadFunction> dead_functions;
    auto source_files = get_all_source_files(project_path);

    // Build call graph
    auto call_graph = build_call_graph(project_path);

    // Find entry points (main functions, exported functions, etc.)
    std::vector<std::string> entry_points = {"main"}; // TODO: detect more entry points

    // Find unreachable functions
    auto unreachable = find_unreachable_functions("main", call_graph);

    for (const auto& func : unreachable) {
        DeadFunction df;
        df.name = func;
        df.reason = "Function is not reachable from any entry point";
        // TODO: fill other fields
        dead_functions.push_back(df);
    }

    return dead_functions;
}

std::vector<DeadVariable> DeadCodeDetector::find_unused_variables(const std::string& project_path) {
    std::vector<DeadVariable> dead_variables;
    // TODO: implement variable analysis using Clang AST
    return dead_variables;
}

std::vector<DeadClass> DeadCodeDetector::find_unused_classes(const std::string& project_path) {
    std::vector<DeadClass> dead_classes;
    // TODO: implement class analysis
    return dead_classes;
}

std::vector<DeadInclude> DeadCodeDetector::find_unused_includes(const std::string& project_path) {
    std::vector<DeadInclude> dead_includes;
    // TODO: implement include analysis
    return dead_includes;
}

std::vector<DeadCodeBlock> DeadCodeDetector::find_unreachable_code(const std::string& project_path) {
    std::vector<DeadCodeBlock> dead_blocks;
    // TODO: implement unreachable code analysis
    return dead_blocks;
}

bool DeadCodeDetector::analyze_ast_for_usage(const clang::Decl* decl, const std::string& project_path) {
    // TODO: implement AST analysis for usage detection
    return false;
}

bool DeadCodeDetector::is_function_referenced(const std::string& function_name, const std::string& project_path) {
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Simple regex search for function calls
        std::regex func_call_regex("\\b" + function_name + "\\s*\\(");
        if (std::regex_search(content, func_call_regex)) {
            return true;
        }
    }

    return false;
}

bool DeadCodeDetector::is_variable_referenced(const std::string& variable_name, const std::string& project_path) {
    // TODO: implement variable reference detection
    return false;
}

std::unordered_map<std::string, std::vector<std::string>> DeadCodeDetector::build_call_graph(const std::string& project_path) {
    std::unordered_map<std::string, std::vector<std::string>> call_graph;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Simple function definition extraction
        std::regex func_def_regex("(\\w+)\\s+(\\w+)\\s*\\([^)]*\\)\\s*\\{");
        std::smatch match;
        std::string::const_iterator search_start(content.cbegin());

        while (std::regex_search(search_start, content.cend(), match, func_def_regex)) {
            std::string return_type = match[1];
            std::string func_name = match[2];
            call_graph[func_name] = {}; // Initialize empty callees list

            search_start = match.suffix().first;
        }

        // Extract function calls within each function
        // This is a simplified implementation
        std::regex func_call_regex("(\\w+)\\s*\\(");
        search_start = content.cbegin();
        while (std::regex_search(search_start, content.cend(), match, func_call_regex)) {
            std::string called_func = match[1];
            // TODO: associate with calling function
            search_start = match.suffix().first;
        }
    }

    return call_graph;
}

std::vector<std::string> DeadCodeDetector::find_unreachable_functions(const std::string& entry_point,
    const std::unordered_map<std::string, std::vector<std::string>>& call_graph) {

    std::vector<std::string> unreachable;
    std::unordered_set<std::string> visited;
    std::queue<std::string> to_visit;

    to_visit.push(entry_point);
    visited.insert(entry_point);

    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();

        auto it = call_graph.find(current);
        if (it != call_graph.end()) {
            for (const auto& callee : it->second) {
                if (visited.find(callee) == visited.end()) {
                    visited.insert(callee);
                    to_visit.push(callee);
                }
            }
        }
    }

    for (const auto& func : call_graph) {
        if (visited.find(func.first) == visited.end()) {
            unreachable.push_back(func.first);
        }
    }

    return unreachable;
}

bool DeadCodeDetector::is_template_instantiated(const std::string& template_name, const std::string& project_path) {
    // TODO: implement template instantiation detection
    return false;
}

bool DeadCodeDetector::is_macro_used(const std::string& macro_name, const std::string& project_path) {
    // TODO: implement macro usage detection
    return false;
}

std::vector<std::string> DeadCodeDetector::get_all_source_files(const std::string& project_path) {
    std::vector<std::string> source_files;

    for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c++") {
                source_files.push_back(entry.path().string());
            }
        }
    }

    return source_files;
}

std::string DeadCodeDetector::extract_function_signature(const std::string& code, const std::string& function_name) {
    // TODO: implement signature extraction
    return function_name + "()";
}

uint32_t DeadCodeDetector::calculate_function_size(const std::string& code) {
    return code.size();
}

} // namespace code_quality