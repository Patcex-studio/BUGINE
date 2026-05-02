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
#include "bug_detector.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

namespace code_quality {

BugDetector::BugDetector() {
}

BugDetector::~BugDetector() {
}

BugDetectionResult BugDetector::analyze_project(const std::string& project_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    BugDetectionResult result;

    result.memory_bugs = detect_memory_bugs(project_path);
    result.logic_bugs = detect_logic_bugs(project_path);
    result.concurrency_bugs = detect_concurrency_bugs(project_path);
    result.performance_bugs = detect_performance_bugs(project_path);

    result.total_bugs_found = result.memory_bugs.size() + result.logic_bugs.size() +
                             result.concurrency_bugs.size() + result.performance_bugs.size();

    // Categorize by severity (simplified)
    result.critical_bugs = 0;
    result.high_bugs = 0;
    result.medium_bugs = 0;
    result.low_bugs = 0;

    for (const auto& bug : result.memory_bugs) {
        if (bug.severity >= 4) result.critical_bugs++;
        else if (bug.severity >= 3) result.high_bugs++;
        else if (bug.severity >= 2) result.medium_bugs++;
        else result.low_bugs++;
    }

    // Similar for other bug types...

    auto end_time = std::chrono::high_resolution_clock::now();
    result.analysis_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

std::vector<MemoryBug> BugDetector::detect_memory_bugs(const std::string& project_path) {
    std::vector<MemoryBug> memory_bugs;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::istringstream iss(content);
        std::string line;
        uint32_t line_number = 1;

        while (std::getline(iss, line)) {
            // Detect potential memory leaks (simplified)
            std::regex new_regex("\\bnew\\b");
            if (std::regex_search(line, new_regex)) {
                std::regex delete_regex("\\bdelete\\b");
                // Check if there's a corresponding delete (very simplified)
                if (!std::regex_search(content, delete_regex)) {
                    MemoryBug bug;
                    bug.bug_type = "MEMORY_LEAK";
                    bug.description = "Potential memory leak: 'new' without corresponding 'delete'";
                    bug.file_path = file;
                    bug.line_number = line_number;
                    bug.code_snippet = extract_code_snippet(file, line_number);
                    bug.suggested_fix = "Ensure proper memory deallocation with delete";
                    bug.severity = 3;
                    bug.confidence_level = "MEDIUM";
                    memory_bugs.push_back(bug);
                }
            }

            // Detect use after free
            std::regex free_regex("\\bfree\\(|\\bdelete\\b");
            std::regex use_after_free_regex("\\*\\w+\\s*=|\\w+\\s*->|\\w+\\[");
            if (std::regex_search(line, free_regex)) {
                // Check subsequent uses (simplified)
                std::string remaining_content = content.substr(iss.tellg());
                if (std::regex_search(remaining_content, use_after_free_regex)) {
                    MemoryBug bug;
                    bug.bug_type = "USE_AFTER_FREE";
                    bug.description = "Potential use after free";
                    bug.file_path = file;
                    bug.line_number = line_number;
                    bug.code_snippet = extract_code_snippet(file, line_number);
                    bug.suggested_fix = "Avoid using pointer after deallocation";
                    bug.severity = 4;
                    bug.confidence_level = "HIGH";
                    memory_bugs.push_back(bug);
                }
            }

            line_number++;
        }
    }

    return memory_bugs;
}

std::vector<LogicBug> BugDetector::detect_logic_bugs(const std::string& project_path) {
    std::vector<LogicBug> logic_bugs;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::istringstream iss(content);
        std::string line;
        uint32_t line_number = 1;

        while (std::getline(iss, line)) {
            // Detect division by zero
            std::regex div_regex("/\\s*\\w+");
            if (std::regex_search(line, div_regex)) {
                LogicBug bug;
                bug.bug_type = "DIVISION_BY_ZERO";
                bug.description = "Potential division by zero";
                bug.file_path = file;
                bug.line_number = line_number;
                bug.code_snippet = extract_code_snippet(file, line_number);
                bug.suggested_fix = "Add check for zero denominator";
                bug.severity = 3;
                bug.confidence_level = "MEDIUM";
                logic_bugs.push_back(bug);
            }

            // Detect null pointer dereference
            std::regex null_deref_regex("\\*\\w+\\s*=|\\w+\\s*->|\\w+\\[");
            if (std::regex_search(line, null_deref_regex)) {
                LogicBug bug;
                bug.bug_type = "NULL_POINTER_DEREFERENCE";
                bug.description = "Potential null pointer dereference";
                bug.file_path = file;
                bug.line_number = line_number;
                bug.code_snippet = extract_code_snippet(file, line_number);
                bug.suggested_fix = "Add null check before dereference";
                bug.severity = 4;
                bug.confidence_level = "MEDIUM";
                logic_bugs.push_back(bug);
            }

            line_number++;
        }
    }

    return logic_bugs;
}

std::vector<ConcurrencyBug> BugDetector::detect_concurrency_bugs(const std::string& project_path) {
    std::vector<ConcurrencyBug> concurrency_bugs;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::istringstream iss(content);
        std::string line;
        uint32_t line_number = 1;

        while (std::getline(iss, line)) {
            // Detect potential race conditions (simplified)
            std::regex thread_regex("\\bstd::thread\\b|\\bpthread_create\\b");
            std::regex shared_var_regex("\\bshared_var\\b|\\bglobal_var\\b"); // Placeholder

            if (std::regex_search(line, thread_regex) && std::regex_search(content, shared_var_regex)) {
                ConcurrencyBug bug;
                bug.bug_type = "RACE_CONDITION";
                bug.description = "Potential race condition with shared variables";
                bug.file_path = file;
                bug.line_number = line_number;
                bug.code_snippet = extract_code_snippet(file, line_number);
                bug.suggested_fix = "Use mutex or atomic operations for shared data";
                bug.severity = 4;
                bug.confidence_level = "MEDIUM";
                concurrency_bugs.push_back(bug);
            }

            line_number++;
        }
    }

    return concurrency_bugs;
}

std::vector<PerformanceBug> BugDetector::detect_performance_bugs(const std::string& project_path) {
    std::vector<PerformanceBug> performance_bugs;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::istringstream iss(content);
        std::string line;
        uint32_t line_number = 1;

        while (std::getline(iss, line)) {
            // Detect inefficient loops
            std::regex loop_regex("\\bfor\\s*\\(|\\bwhile\\s*\\(");
            std::regex expensive_op_regex("\\bstd::string\\s*\\+|\\bpush_back\\b|\\binsert\\b");

            if (std::regex_search(line, loop_regex) && std::regex_search(line, expensive_op_regex)) {
                PerformanceBug bug;
                bug.bug_type = "INEFFICIENT_LOOP";
                bug.description = "Potentially inefficient operation inside loop";
                bug.file_path = file;
                bug.line_number = line_number;
                bug.code_snippet = extract_code_snippet(file, line_number);
                bug.suggested_fix = "Consider moving expensive operations outside the loop";
                bug.severity = 2;
                bug.confidence_level = "LOW";
                performance_bugs.push_back(bug);
            }

            line_number++;
        }
    }

    return performance_bugs;
}

std::vector<TaintResult> BugDetector::perform_taint_analysis(const std::string& code) {
    // TODO: implement taint analysis
    return {};
}

bool BugDetector::detect_buffer_overflow(const TaintResult& taint_result) {
    // TODO: implement buffer overflow detection
    return false;
}

bool BugDetector::detect_use_after_free(const TaintResult& taint_result) {
    // TODO: implement use after free detection
    return false;
}

std::vector<DataFlowResult> BugDetector::perform_data_flow_analysis(const std::string& code) {
    // TODO: implement data flow analysis
    return {};
}

bool BugDetector::detect_null_pointer_dereference(const DataFlowResult& flow_result) {
    return flow_result.may_be_null;
}

bool BugDetector::detect_division_by_zero(const DataFlowResult& flow_result) {
    // TODO: implement division by zero detection
    return false;
}

std::vector<ConcurrencyIssue> BugDetector::detect_race_conditions(const std::string& code) {
    // TODO: implement race condition detection
    return {};
}

std::vector<ConcurrencyIssue> BugDetector::detect_deadlocks(const std::string& code) {
    // TODO: implement deadlock detection
    return {};
}

std::vector<ConcurrencyIssue> BugDetector::detect_atomicity_violations(const std::string& code) {
    // TODO: implement atomicity violation detection
    return {};
}

std::vector<std::string> BugDetector::get_all_source_files(const std::string& project_path) {
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

std::string BugDetector::extract_code_snippet(const std::string& file_path, uint32_t line_number, uint32_t context_lines) {
    std::ifstream ifs(file_path);
    std::string line;
    uint32_t current_line = 1;
    std::string snippet;

    while (std::getline(ifs, line)) {
        if (current_line >= line_number - context_lines && current_line <= line_number + context_lines) {
            if (current_line == line_number) {
                snippet += ">>> " + line + "\n";
            } else {
                snippet += "    " + line + "\n";
            }
        }
        current_line++;
    }

    return snippet;
}

} // namespace code_quality