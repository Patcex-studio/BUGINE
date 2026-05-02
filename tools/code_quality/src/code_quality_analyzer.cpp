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
#include "code_quality_analyzer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace code_quality {

CodeQualityAnalyzer::CodeQualityAnalyzer()
    : dead_code_detector_(), duplicate_code_detector_(), bug_detector_() {
}

CodeQualityAnalyzer::~CodeQualityAnalyzer() {
}

CodeQualityReport CodeQualityAnalyzer::perform_full_analysis(const std::string& project_path) {
    std::cout << "Starting comprehensive code quality analysis..." << std::endl;

    // Parse project structure
    if (!parse_project_structure(project_path)) {
        std::cerr << "Failed to parse project structure" << std::endl;
        return {};
    }

    // Analyze syntax and semantics
    if (!analyze_syntax_correctness(project_path)) {
        std::cerr << "Syntax errors detected" << std::endl;
    }

    if (!analyze_semantic_correctness(project_path)) {
        std::cerr << "Semantic errors detected" << std::endl;
    }

    // Perform individual analyses
    auto dead_code_result = analyze_dead_code(project_path);
    auto duplicate_result = analyze_duplicate_code(project_path);
    auto bug_result = analyze_bugs(project_path);

    // Generate comprehensive report
    auto report = generate_report(dead_code_result, duplicate_result, bug_result);

    std::cout << "Analysis completed in " << report.total_analysis_time.count() << "ms" << std::endl;
    std::cout << "Overall quality score: " << report.overall_quality_score << "/100" << std::endl;

    return report;
}

DeadCodeAnalysisResult CodeQualityAnalyzer::analyze_dead_code(const std::string& project_path) {
    std::cout << "Analyzing dead code..." << std::endl;
    return dead_code_detector_.analyze_project(project_path);
}

DuplicateCodeAnalysisResult CodeQualityAnalyzer::analyze_duplicate_code(const std::string& project_path) {
    std::cout << "Analyzing duplicate code..." << std::endl;
    return duplicate_code_detector_.analyze_project(project_path);
}

BugDetectionResult CodeQualityAnalyzer::analyze_bugs(const std::string& project_path) {
    std::cout << "Analyzing bugs..." << std::endl;
    return bug_detector_.analyze_project(project_path);
}

bool CodeQualityAnalyzer::parse_project_structure(const std::string& project_path) {
    std::cout << "Parsing project structure..." << std::endl;

    if (!fs::exists(project_path) || !fs::is_directory(project_path)) {
        std::cerr << "Invalid project path: " << project_path << std::endl;
        return false;
    }

    // Check for build files
    auto build_files = get_build_files(project_path);
    if (build_files.empty()) {
        std::cerr << "No build configuration found" << std::endl;
        return false;
    }

    std::cout << "Found build files: ";
    for (const auto& file : build_files) {
        std::cout << file << " ";
    }
    std::cout << std::endl;

    return true;
}

std::vector<std::string> CodeQualityAnalyzer::get_build_files(const std::string& project_path) {
    std::vector<std::string> build_files;

    std::vector<std::string> possible_files = {
        "CMakeLists.txt", "Makefile", "build.gradle", "pom.xml",
        "package.json", "setup.py", "Cargo.toml"
    };

    for (const auto& file : possible_files) {
        fs::path file_path = fs::path(project_path) / file;
        if (fs::exists(file_path)) {
            build_files.push_back(file);
        }
    }

    return build_files;
}

bool CodeQualityAnalyzer::analyze_syntax_correctness(const std::string& project_path) {
    std::cout << "Analyzing syntax correctness..." << std::endl;
    // TODO: implement syntax analysis using Clang
    return true; // Placeholder
}

bool CodeQualityAnalyzer::analyze_semantic_correctness(const std::string& project_path) {
    std::cout << "Analyzing semantic correctness..." << std::endl;
    // TODO: implement semantic analysis
    return true; // Placeholder
}

float CodeQualityAnalyzer::calculate_code_complexity(const std::string& file_path) {
    // TODO: implement complexity calculation (cyclomatic complexity, etc.)
    return 1.0f; // Placeholder
}

float CodeQualityAnalyzer::calculate_maintainability_index(const std::string& file_path) {
    // TODO: implement maintainability index calculation
    return 85.0f; // Placeholder
}

CodeQualityReport CodeQualityAnalyzer::generate_report(const DeadCodeAnalysisResult& dead_code,
                                                     const DuplicateCodeAnalysisResult& duplicates,
                                                     const BugDetectionResult& bugs) {
    CodeQualityReport report;
    report.dead_code_result = dead_code;
    report.duplicate_result = duplicates;
    report.bug_result = bugs;
    report.refactoring_suggestions = duplicate_code_detector_.suggest_refactoring(duplicates.duplicate_blocks);

    // Calculate overall quality score
    report.overall_quality_score = calculate_overall_quality_score(report);

    // Generate recommendations
    report.recommendations = generate_recommendations(report);

    // Calculate total analysis time
    report.total_analysis_time = dead_code.analysis_time + duplicates.analysis_time + bugs.analysis_time;

    return report;
}

std::vector<std::string> CodeQualityAnalyzer::get_all_source_files(const std::string& project_path) {
    std::vector<std::string> source_files;

    for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c++" ||
                ext == ".h" || ext == ".hpp") {
                source_files.push_back(entry.path().string());
            }
        }
    }

    return source_files;
}

uint32_t CodeQualityAnalyzer::count_total_lines_of_code(const std::string& project_path) {
    uint32_t total_lines = 0;
    auto source_files = get_all_source_files(project_path);

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        std::string line;
        while (std::getline(ifs, line)) {
            // Skip empty lines and comments
            if (!line.empty() && line.find("//") != 0 && line.find("/*") != 0) {
                total_lines++;
            }
        }
    }

    return total_lines;
}

float CodeQualityAnalyzer::calculate_overall_quality_score(const CodeQualityReport& report) {
    float score = 100.0f;

    // Deduct points for dead code
    score -= report.dead_code_result.dead_code_percentage * 0.5f;

    // Deduct points for duplicates
    score -= report.duplicate_result.duplication_percentage * 0.3f;

    // Deduct points for bugs
    score -= report.bug_result.total_bugs_found * 2.0f;

    return std::max(0.0f, score);
}

std::string CodeQualityAnalyzer::generate_recommendations(const CodeQualityReport& report) {
    std::string recommendations;

    if (report.dead_code_result.total_dead_functions > 0) {
        recommendations += "Remove " + std::to_string(report.dead_code_result.total_dead_functions) +
                          " unused functions. ";
    }

    if (report.duplicate_result.total_duplicate_blocks > 0) {
        recommendations += "Refactor " + std::to_string(report.duplicate_result.total_duplicate_blocks) +
                          " duplicate code blocks. ";
    }

    if (report.bug_result.critical_bugs > 0) {
        recommendations += "Fix " + std::to_string(report.bug_result.critical_bugs) +
                          " critical bugs immediately. ";
    }

    if (recommendations.empty()) {
        recommendations = "Code quality is good. Continue maintaining high standards.";
    }

    return recommendations;
}

} // namespace code_quality