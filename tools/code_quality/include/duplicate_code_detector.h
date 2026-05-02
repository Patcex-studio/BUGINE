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

struct Token {
    std::string type;
    std::string value;
};

class DuplicateCodeDetector {
public:
    DuplicateCodeDetector();
    ~DuplicateCodeDetector();

    // Main analysis function
    DuplicateCodeAnalysisResult analyze_project(const std::string& project_path);

    // Token-based comparison
    std::vector<Token> tokenize_code(const std::string& code);
    float calculate_similarity(const std::vector<Token>& tokens1, const std::vector<Token>& tokens2);
    bool are_blocks_similar(const std::string& block1, const std::string& block2, float threshold = 0.8f);

    // AST-based comparison
    std::vector<std::string> extract_ast_nodes(const std::string& code);
    float calculate_ast_similarity(const std::vector<std::string>& ast1, const std::vector<std::string>& ast2);

    // Sliding window approach
    std::vector<DuplicateBlock> find_duplicates_sliding_window(const std::vector<std::string>& files);
    std::vector<DuplicateBlock> find_duplicates_suffix_tree(const std::vector<std::string>& files);

    // Refactoring suggestions
    std::vector<RefactoringSuggestion> suggest_refactoring(const std::vector<DuplicateBlock>& duplicates);

private:
    // Helper methods
    std::vector<std::string> get_all_source_files(const std::string& project_path);
    std::string normalize_code(const std::string& code);
    std::vector<std::string> split_into_blocks(const std::string& code, size_t block_size);
};

} // namespace code_quality