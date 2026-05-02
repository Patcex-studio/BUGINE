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

struct TaintResult {
    std::string variable;
    std::string source;
    std::vector<std::string> sinks;
    bool is_tainted;
};

struct DataFlowResult {
    std::string variable;
    std::vector<std::string> definitions;
    std::vector<std::string> uses;
    bool may_be_null;
    bool may_be_uninitialized;
};

struct ConcurrencyIssue {
    std::string type;
    std::string description;
    std::string file_path;
    uint32_t line_number;
    std::string code_snippet;
};

class BugDetector {
public:
    BugDetector();
    ~BugDetector();

    // Main analysis function
    BugDetectionResult analyze_project(const std::string& project_path);

    // Individual bug detection methods
    std::vector<MemoryBug> detect_memory_bugs(const std::string& project_path);
    std::vector<LogicBug> detect_logic_bugs(const std::string& project_path);
    std::vector<ConcurrencyBug> detect_concurrency_bugs(const std::string& project_path);
    std::vector<PerformanceBug> detect_performance_bugs(const std::string& project_path);

    // Taint analysis
    std::vector<TaintResult> perform_taint_analysis(const std::string& code);
    bool detect_buffer_overflow(const TaintResult& taint_result);
    bool detect_use_after_free(const TaintResult& taint_result);

    // Data flow analysis
    std::vector<DataFlowResult> perform_data_flow_analysis(const std::string& code);
    bool detect_null_pointer_dereference(const DataFlowResult& flow_result);
    bool detect_division_by_zero(const DataFlowResult& flow_result);

    // Concurrency analysis
    std::vector<ConcurrencyIssue> detect_race_conditions(const std::string& code);
    std::vector<ConcurrencyIssue> detect_deadlocks(const std::string& code);
    std::vector<ConcurrencyIssue> detect_atomicity_violations(const std::string& code);

private:
    // Helper methods
    std::vector<std::string> get_all_source_files(const std::string& project_path);
    std::string extract_code_snippet(const std::string& file_path, uint32_t line_number, uint32_t context_lines = 3);
};

} // namespace code_quality