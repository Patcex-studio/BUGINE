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
#include "code_refactorer.h"
#include "test_executor.h"
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <project_path> [command]" << std::endl;
        std::cerr << "Commands:" << std::endl;
        std::cerr << "  analyze    - Run code quality analysis" << std::endl;
        std::cerr << "  refactor   - Apply automatic refactoring" << std::endl;
        std::cerr << "  test       - Run comprehensive test suite" << std::endl;
        std::cerr << "  full       - Run full analysis, testing, and refactoring" << std::endl;
        return 1;
    }

    std::string project_path = argv[1];
    std::string command = (argc >= 3) ? argv[2] : "analyze";

    // Validate project path
    if (!fs::exists(project_path) || !fs::is_directory(project_path)) {
        std::cerr << "Invalid project path: " << project_path << std::endl;
        return 1;
    }

    // Canonicalize path
    project_path = fs::canonical(project_path).string();

    code_quality::CodeQualityAnalyzer analyzer;
    code_quality::CodeRefactorer refactorer;
    code_quality::TestExecutor test_executor;

    if (command == "analyze") {
        std::cout << "=== Code Quality Analysis ===" << std::endl;
        auto report = analyzer.perform_full_analysis(project_path);

        std::cout << "\n=== Analysis Results ===" << std::endl;
        std::cout << "Dead functions: " << report.dead_code_result.total_dead_functions << std::endl;
        std::cout << "Duplicate blocks: " << report.duplicate_result.total_duplicate_blocks << std::endl;
        std::cout << "Total bugs: " << report.bug_result.total_bugs_found << std::endl;
        std::cout << "Overall quality score: " << report.overall_quality_score << "/100" << std::endl;
        std::cout << "Recommendations: " << report.recommendations << std::endl;

    } else if (command == "refactor") {
        std::cout << "=== Code Refactoring ===" << std::endl;

        // First analyze to get suggestions
        auto report = analyzer.perform_full_analysis(project_path);

        // Collect refactoring suggestions
        std::vector<code_quality::RefactoringSuggestion> suggestions = report.refactoring_suggestions;

        if (!suggestions.empty()) {
            std::cout << "Applying " << suggestions.size() << " refactoring suggestions..." << std::endl;
            if (refactorer.apply_refactorings(suggestions, project_path)) {
                std::cout << "Refactoring completed successfully!" << std::endl;
            } else {
                std::cerr << "Refactoring failed!" << std::endl;
                return 1;
            }
        } else {
            std::cout << "No refactoring suggestions available." << std::endl;
        }

    } else if (command == "test") {
        std::cout << "=== Test Execution ===" << std::endl;
        auto test_results = test_executor.run_comprehensive_test_suite(project_path);

        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << test_results.passed_tests << std::endl;
        std::cout << "Failed: " << test_results.failed_tests << std::endl;
        std::cout << "Coverage: " << test_results.overall_coverage << "%" << std::endl;
        std::cout << "Duration: " << test_results.total_duration.count() << "ms" << std::endl;

    } else if (command == "full") {
        std::cout << "=== Full Code Quality Assurance Pipeline ===" << std::endl;

        // 1. Analysis
        std::cout << "\n--- Step 1: Code Analysis ---" << std::endl;
        auto report = analyzer.perform_full_analysis(project_path);

        // 2. Testing
        std::cout << "\n--- Step 2: Test Execution ---" << std::endl;
        auto test_results = test_executor.run_comprehensive_test_suite(project_path);

        // 3. Refactoring (if quality score is low)
        if (report.overall_quality_score < 80.0f) {
            std::cout << "\n--- Step 3: Automatic Refactoring ---" << std::endl;
            std::vector<code_quality::RefactoringSuggestion> suggestions = report.refactoring_suggestions;

            if (!suggestions.empty()) {
                if (refactorer.apply_refactorings(suggestions, project_path)) {
                    std::cout << "Refactoring completed successfully!" << std::endl;

                    // Re-run tests after refactoring
                    std::cout << "\n--- Step 4: Post-Refactoring Testing ---" << std::endl;
                    test_results = test_executor.run_comprehensive_test_suite(project_path);
                } else {
                    std::cerr << "Refactoring failed!" << std::endl;
                }
            }
        }

        // Final report
        std::cout << "\n=== Final Quality Report ===" << std::endl;
        std::cout << "Quality Score: " << report.overall_quality_score << "/100" << std::endl;
        std::cout << "Tests Passed: " << test_results.passed_tests << std::endl;
        std::cout << "Tests Failed: " << test_results.failed_tests << std::endl;
        std::cout << "Code Coverage: " << test_results.overall_coverage << "%" << std::endl;

    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}