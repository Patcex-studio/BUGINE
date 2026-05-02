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

#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>

namespace code_quality {

// Common structures for code quality analysis

struct SourceLocation {
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
};

// Dead Code Detection Structures
struct DeadFunction {
    std::string name;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string signature;
    uint32_t size_bytes;
    std::string reason;
};

struct DeadVariable {
    std::string name;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string type;
    std::string reason;
};

struct DeadClass {
    std::string name;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string reason;
};

struct DeadInclude {
    std::string include_path;
    std::string file_path;
    uint32_t line_number;
    std::string reason;
};

struct DeadCodeBlock {
    std::string code_snippet;
    std::string file_path;
    uint32_t start_line;
    uint32_t end_line;
    std::string reason;
};

struct DeadCodeAnalysisResult {
    std::vector<DeadFunction> dead_functions;
    std::vector<DeadVariable> dead_variables;
    std::vector<DeadClass> dead_classes;
    std::vector<DeadInclude> dead_includes;
    std::vector<DeadCodeBlock> dead_code_blocks;
    uint32_t total_dead_functions;
    uint32_t total_dead_variables;
    uint32_t total_dead_classes;
    float dead_code_percentage;
    std::chrono::milliseconds analysis_time;
};

// Duplicate Code Detection Structures
struct DuplicateBlock {
    std::string code_snippet;
    std::vector<SourceLocation> locations;
    float similarity_score;
    uint32_t token_count;
    uint32_t duplicate_count;
    uint32_t total_tokens_saved;
};

struct DuplicateFunction {
    std::string name;
    std::vector<SourceLocation> locations;
    float similarity_score;
    std::string signature;
};

struct DuplicateClass {
    std::string name;
    std::vector<SourceLocation> locations;
    float similarity_score;
};

struct DuplicateCodeAnalysisResult {
    std::vector<DuplicateBlock> duplicate_blocks;
    std::vector<DuplicateFunction> duplicate_functions;
    std::vector<DuplicateClass> duplicate_classes;
    uint32_t total_duplicate_blocks;
    uint32_t total_duplicate_functions;
    uint32_t total_duplicate_classes;
    float duplication_percentage;
    std::chrono::milliseconds analysis_time;
};

// Bug Detection Structures
struct MemoryBug {
    std::string bug_type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string code_snippet;
    std::string suggested_fix;
    uint32_t severity;
    std::string confidence_level;
};

struct LogicBug {
    std::string bug_type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string code_snippet;
    std::string suggested_fix;
    uint32_t severity;
    std::string confidence_level;
};

struct ConcurrencyBug {
    std::string bug_type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string code_snippet;
    std::string suggested_fix;
    uint32_t severity;
    std::string confidence_level;
};

struct PerformanceBug {
    std::string bug_type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    uint32_t column_number;
    std::string code_snippet;
    std::string suggested_fix;
    uint32_t severity;
    std::string confidence_level;
};

struct BugDetectionResult {
    std::vector<MemoryBug> memory_bugs;
    std::vector<LogicBug> logic_bugs;
    std::vector<ConcurrencyBug> concurrency_bugs;
    std::vector<PerformanceBug> performance_bugs;
    uint32_t total_bugs_found;
    uint32_t critical_bugs;
    uint32_t high_bugs;
    uint32_t medium_bugs;
    uint32_t low_bugs;
    std::chrono::milliseconds analysis_time;
};

// Refactoring Structures
struct RefactoringSuggestion {
    std::string type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    std::string original_code;
    std::string refactored_code;
    float confidence;
    std::string rationale;
};

// Test Structures
struct TestResult {
    std::string test_name;
    std::string test_suite;
    bool passed;
    std::chrono::milliseconds duration;
    std::string failure_message;
    float coverage_percentage;
};

struct TestResults {
    std::vector<TestResult> unit_tests;
    std::vector<TestResult> integration_tests;
    std::vector<TestResult> performance_tests;
    float overall_coverage;
    std::chrono::milliseconds total_duration;
    uint32_t passed_tests;
    uint32_t failed_tests;
};

// Code Quality Report
struct CodeQualityReport {
    DeadCodeAnalysisResult dead_code_result;
    DuplicateCodeAnalysisResult duplicate_result;
    BugDetectionResult bug_result;
    TestResults test_results;
    std::vector<RefactoringSuggestion> refactoring_suggestions;
    float overall_quality_score;
    std::string recommendations;
    std::chrono::milliseconds total_analysis_time;
};

} // namespace code_quality