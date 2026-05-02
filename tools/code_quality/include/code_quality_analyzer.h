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
#include "dead_code_detector.h"
#include "duplicate_code_detector.h"
#include "bug_detector.h"
#include <string>

namespace code_quality {

class CodeQualityAnalyzer {
public:
    CodeQualityAnalyzer();
    ~CodeQualityAnalyzer();

    // Main analysis pipeline
    CodeQualityReport perform_full_analysis(const std::string& project_path);

    // Individual analysis components
    DeadCodeAnalysisResult analyze_dead_code(const std::string& project_path);
    DuplicateCodeAnalysisResult analyze_duplicate_code(const std::string& project_path);
    BugDetectionResult analyze_bugs(const std::string& project_path);

    // Project structure analysis
    bool parse_project_structure(const std::string& project_path);
    std::vector<std::string> get_build_files(const std::string& project_path);

    // Syntax and semantic analysis
    bool analyze_syntax_correctness(const std::string& project_path);
    bool analyze_semantic_correctness(const std::string& project_path);

    // Code complexity and maintainability
    float calculate_code_complexity(const std::string& file_path);
    float calculate_maintainability_index(const std::string& file_path);

    // Generate comprehensive report
    CodeQualityReport generate_report(const DeadCodeAnalysisResult& dead_code,
                                    const DuplicateCodeAnalysisResult& duplicates,
                                    const BugDetectionResult& bugs);
    std::string generate_recommendations(const CodeQualityReport& report);

private:
    DeadCodeDetector dead_code_detector_;
    DuplicateCodeDetector duplicate_code_detector_;
    BugDetector bug_detector_;

    // Helper methods
    std::vector<std::string> get_all_source_files(const std::string& project_path);
    uint32_t count_total_lines_of_code(const std::string& project_path);
    float calculate_overall_quality_score(const CodeQualityReport& report);
};

} // namespace code_quality