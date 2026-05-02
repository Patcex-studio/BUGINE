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
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

void test_code_quality_analyzer() {
    code_quality::CodeQualityAnalyzer analyzer;

    // Test with a simple project path
    std::string test_path = "/tmp/test_project";

    // Create a simple test project
    std::filesystem::create_directories(test_path);
    std::ofstream test_file(test_path + "/test.cpp");
    test_file << "#include <iostream>\n\nvoid unused_function() {}\n\nint main() { return 0; }\n";
    test_file.close();

    auto report = analyzer.perform_full_analysis(test_path);

    // Basic assertions
    assert(report.overall_quality_score >= 0.0f && report.overall_quality_score <= 100.0f);
    assert(report.total_analysis_time.count() >= 0);

    std::cout << "Code quality analyzer test passed!" << std::endl;

    // Cleanup
    std::filesystem::remove_all(test_path);
}

int main() {
    test_code_quality_analyzer();
    return 0;
}