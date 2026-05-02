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

class TestExecutor {
public:
    TestExecutor();
    ~TestExecutor();

    // Main test execution function
    TestResults run_comprehensive_test_suite(const std::string& project_path);

    // Individual test types
    TestResults run_unit_tests(const std::string& project_path);
    TestResults run_integration_tests(const std::string& project_path);
    TestResults run_performance_tests(const std::string& project_path);
    TestResults run_stress_tests(const std::string& project_path);
    TestResults run_memory_tests(const std::string& project_path);
    TestResults run_concurrency_tests(const std::string& project_path);

    // Build and execution
    bool build_project(const std::string& project_path);
    bool build_test_targets(const std::string& project_path);

    // Test discovery
    std::vector<std::string> discover_unit_tests(const std::string& project_path);
    std::vector<std::string> discover_integration_tests(const std::string& project_path);

    // Coverage analysis
    float calculate_code_coverage(const std::string& project_path);
    std::vector<std::string> get_uncovered_lines(const std::string& project_path);

    // Performance metrics
    std::chrono::milliseconds measure_test_execution_time(const std::string& test_command);
    bool detect_memory_leaks(const std::string& project_path);

private:
    // Helper methods
    std::string find_build_system(const std::string& project_path);
    std::string generate_build_command(const std::string& build_system, const std::string& project_path);
    std::string generate_test_command(const std::string& build_system, const std::string& project_path);
    bool execute_command(const std::string& command, std::string& output, int& exit_code);
    TestResult parse_test_output(const std::string& output);
};

} // namespace code_quality