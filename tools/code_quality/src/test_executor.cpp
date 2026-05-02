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
#include "test_executor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <cstdlib>

namespace fs = std::filesystem;

namespace code_quality {

TestExecutor::TestExecutor() {
}

TestExecutor::~TestExecutor() {
}

TestResults TestExecutor::run_comprehensive_test_suite(const std::string& project_path) {
    std::cout << "Running comprehensive test suite..." << std::endl;

    TestResults results;

    // Build project with test targets
    if (!build_test_targets(project_path)) {
        std::cerr << "Failed to build test targets" << std::endl;
        return results;
    }

    // Run different types of tests
    auto unit_results = run_unit_tests(project_path);
    auto integration_results = run_integration_tests(project_path);
    auto performance_results = run_performance_tests(project_path);

    // Combine results
    results.unit_tests = unit_results.unit_tests;
    results.integration_tests = integration_results.unit_tests; // Assuming same structure
    results.performance_tests = performance_results.unit_tests;

    // Calculate overall metrics
    results.overall_coverage = calculate_code_coverage(project_path);
    results.total_duration = unit_results.total_duration + integration_results.total_duration +
                           performance_results.total_duration;

    results.passed_tests = 0;
    results.failed_tests = 0;

    for (const auto& test : results.unit_tests) {
        if (test.passed) results.passed_tests++;
        else results.failed_tests++;
    }

    for (const auto& test : results.integration_tests) {
        if (test.passed) results.passed_tests++;
        else results.failed_tests++;
    }

    // Memory leak detection
    if (!detect_memory_leaks(project_path)) {
        std::cerr << "Memory leaks detected!" << std::endl;
    }

    std::cout << "Test suite completed: " << results.passed_tests << " passed, "
              << results.failed_tests << " failed" << std::endl;

    return results;
}

TestResults TestExecutor::run_unit_tests(const std::string& project_path) {
    std::cout << "Running unit tests..." << std::endl;

    TestResults results;
    auto start_time = std::chrono::high_resolution_clock::now();

    std::string build_system = find_build_system(project_path);
    std::string test_command = generate_test_command(build_system, project_path);

    if (test_command.empty()) {
        std::cerr << "No test command available" << std::endl;
        return results;
    }

    std::string output;
    int exit_code;
    if (execute_command(test_command, output, exit_code)) {
        // Parse test output (simplified)
        results.unit_tests = {parse_test_output(output)};
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    results.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return results;
}

TestResults TestExecutor::run_integration_tests(const std::string& project_path) {
    std::cout << "Running integration tests..." << std::endl;
    // TODO: implement integration test execution
    return {};
}

TestResults TestExecutor::run_performance_tests(const std::string& project_path) {
    std::cout << "Running performance tests..." << std::endl;
    // TODO: implement performance test execution
    return {};
}

TestResults TestExecutor::run_stress_tests(const std::string& project_path) {
    std::cout << "Running stress tests..." << std::endl;
    // TODO: implement stress test execution
    return {};
}

TestResults TestExecutor::run_memory_tests(const std::string& project_path) {
    std::cout << "Running memory tests..." << std::endl;
    // TODO: implement memory test execution
    return {};
}

TestResults TestExecutor::run_concurrency_tests(const std::string& project_path) {
    std::cout << "Running concurrency tests..." << std::endl;
    // TODO: implement concurrency test execution
    return {};
}

bool TestExecutor::build_project(const std::string& project_path) {
    std::cout << "Building project..." << std::endl;

    std::string build_system = find_build_system(project_path);
    std::string build_command = generate_build_command(build_system, project_path);

    if (build_command.empty()) {
        return false;
    }

    std::string output;
    int exit_code;
    return execute_command(build_command, output, exit_code) && exit_code == 0;
}

bool TestExecutor::build_test_targets(const std::string& project_path) {
    std::cout << "Building test targets..." << std::endl;

    // For CMake projects, tests are usually built with the main build
    return build_project(project_path);
}

std::vector<std::string> TestExecutor::discover_unit_tests(const std::string& project_path) {
    std::vector<std::string> test_files;

    for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find("test") != std::string::npos ||
                filename.find("Test") != std::string::npos) {
                test_files.push_back(entry.path().string());
            }
        }
    }

    return test_files;
}

std::vector<std::string> TestExecutor::discover_integration_tests(const std::string& project_path) {
    // TODO: implement integration test discovery
    return {};
}

float TestExecutor::calculate_code_coverage(const std::string& project_path) {
    // TODO: implement coverage calculation
    return 85.0f; // Placeholder
}

std::vector<std::string> TestExecutor::get_uncovered_lines(const std::string& project_path) {
    // TODO: implement uncovered lines detection
    return {};
}

std::chrono::milliseconds TestExecutor::measure_test_execution_time(const std::string& test_command) {
    auto start = std::chrono::high_resolution_clock::now();

    std::string output;
    int exit_code;
    execute_command(test_command, output, exit_code);

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

bool TestExecutor::detect_memory_leaks(const std::string& project_path) {
    std::cout << "Detecting memory leaks..." << std::endl;

    // TODO: implement memory leak detection (e.g., using valgrind)
    return true; // Placeholder
}

std::string TestExecutor::find_build_system(const std::string& project_path) {
    if (fs::exists(fs::path(project_path) / "CMakeLists.txt")) {
        return "cmake";
    } else if (fs::exists(fs::path(project_path) / "Makefile")) {
        return "make";
    } else if (fs::exists(fs::path(project_path) / "build.gradle")) {
        return "gradle";
    }

    return "unknown";
}

std::string TestExecutor::generate_build_command(const std::string& build_system, const std::string& project_path) {
    if (build_system == "cmake") {
        return "cd " + project_path + "/build && make -j$(nproc)";
    } else if (build_system == "make") {
        return "cd " + project_path + " && make -j$(nproc)";
    }

    return "";
}

std::string TestExecutor::generate_test_command(const std::string& build_system, const std::string& project_path) {
    if (build_system == "cmake") {
        return "cd " + project_path + "/build && ctest";
    } else if (build_system == "make") {
        return "cd " + project_path + " && make test";
    }

    return "";
}

bool TestExecutor::execute_command(const std::string& command, std::string& output, int& exit_code) {
    std::cout << "Executing: " << command << std::endl;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return false;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    exit_code = pclose(pipe);
    return true;
}

TestResult TestExecutor::parse_test_output(const std::string& output) {
    TestResult result;
    result.test_name = "unknown_test";
    result.test_suite = "unknown_suite";

    // Simple parsing (would need to be more sophisticated for real test frameworks)
    if (output.find("PASSED") != std::string::npos) {
        result.passed = true;
    } else if (output.find("FAILED") != std::string::npos) {
        result.passed = false;
        result.failure_message = "Test failed";
    }

    result.duration = std::chrono::milliseconds(100); // Placeholder
    result.coverage_percentage = 85.0f; // Placeholder

    return result;
}

} // namespace code_quality