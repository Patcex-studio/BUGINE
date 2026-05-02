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
#include "duplicate_code_detector.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <cmath>
#include <set>
#include <iterator>
#include <unordered_map>
#include <sstream>

namespace fs = std::filesystem;

namespace code_quality {

DuplicateCodeDetector::DuplicateCodeDetector() {
}

DuplicateCodeDetector::~DuplicateCodeDetector() {
}

DuplicateCodeAnalysisResult DuplicateCodeDetector::analyze_project(const std::string& project_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    DuplicateCodeAnalysisResult result;
    auto source_files = get_all_source_files(project_path);

    result.duplicate_blocks = find_duplicates_sliding_window(source_files);
    // TODO: implement duplicate functions and classes detection
    result.duplicate_functions = {};
    result.duplicate_classes = {};

    result.total_duplicate_blocks = result.duplicate_blocks.size();
    result.total_duplicate_functions = result.duplicate_functions.size();
    result.total_duplicate_classes = result.duplicate_classes.size();

    // Calculate duplication percentage (simplified)
    result.duplication_percentage = 0.0f;

    auto end_time = std::chrono::high_resolution_clock::now();
    result.analysis_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

std::vector<Token> DuplicateCodeDetector::tokenize_code(const std::string& code) {
    std::vector<Token> tokens;

    // Simple tokenization using regex
    std::regex token_regex("(\\w+|\\S)");
    std::sregex_iterator iter(code.begin(), code.end(), token_regex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        Token token;
        token.value = iter->str();
        // Simple type classification
        if (std::regex_match(token.value, std::regex("\\w+"))) {
            if (std::regex_match(token.value, std::regex("(int|float|double|char|bool|void|class|struct|if|for|while|return)"))) {
                token.type = "keyword";
            } else {
                token.type = "identifier";
            }
        } else {
            token.type = "symbol";
        }
        tokens.push_back(token);
    }

    return tokens;
}

float DuplicateCodeDetector::calculate_similarity(const std::vector<Token>& tokens1, const std::vector<Token>& tokens2) {
    if (tokens1.empty() || tokens2.empty()) return 0.0f;

    // Simple Jaccard similarity for tokens
    std::set<std::string> set1, set2;

    for (const auto& token : tokens1) {
        set1.insert(token.value);
    }
    for (const auto& token : tokens2) {
        set2.insert(token.value);
    }

    std::set<std::string> intersection;
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                         std::inserter(intersection, intersection.begin()));

    std::set<std::string> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(),
                  std::inserter(union_set, union_set.begin()));

    return static_cast<float>(intersection.size()) / union_set.size();
}

bool DuplicateCodeDetector::are_blocks_similar(const std::string& block1, const std::string& block2, float threshold) {
    auto tokens1 = tokenize_code(block1);
    auto tokens2 = tokenize_code(block2);
    return calculate_similarity(tokens1, tokens2) >= threshold;
}

std::vector<std::string> DuplicateCodeDetector::extract_ast_nodes(const std::string& code) {
    // TODO: implement AST extraction using Clang
    return {};
}

float DuplicateCodeDetector::calculate_ast_similarity(const std::vector<std::string>& ast1, const std::vector<std::string>& ast2) {
    // TODO: implement AST similarity
    return 0.0f;
}

std::vector<DuplicateBlock> DuplicateCodeDetector::find_duplicates_sliding_window(const std::vector<std::string>& files) {
    std::vector<DuplicateBlock> duplicates;
    std::unordered_map<std::string, std::vector<SourceLocation>> block_map;

    const size_t BLOCK_SIZE = 10; // lines per block

    for (const auto& file : files) {
        std::ifstream ifs(file);
        std::string line;
        std::vector<std::string> lines;
        uint32_t line_number = 1;

        while (std::getline(ifs, line)) {
            lines.push_back(line);
            line_number++;
        }

        // Create sliding windows
        for (size_t i = 0; i + BLOCK_SIZE <= lines.size(); ++i) {
            std::string block;
            for (size_t j = 0; j < BLOCK_SIZE; ++j) {
                block += lines[i + j] + "\n";
            }

            std::string normalized = normalize_code(block);
            if (!normalized.empty()) {
                SourceLocation loc{file, static_cast<uint32_t>(i + 1), 1};
                block_map[normalized].push_back(loc);
            }
        }
    }

    // Find duplicates
    for (const auto& pair : block_map) {
        if (pair.second.size() > 1) {
            DuplicateBlock dup;
            dup.code_snippet = pair.first;
            dup.locations = pair.second;
            dup.similarity_score = 1.0f; // Exact match
            dup.token_count = tokenize_code(pair.first).size();
            dup.duplicate_count = pair.second.size();
            dup.total_tokens_saved = dup.token_count * (dup.duplicate_count - 1);
            duplicates.push_back(dup);
        }
    }

    return duplicates;
}

std::vector<DuplicateBlock> DuplicateCodeDetector::find_duplicates_suffix_tree(const std::vector<std::string>& files) {
    // TODO: implement suffix tree based duplicate detection
    return {};
}

std::vector<RefactoringSuggestion> DuplicateCodeDetector::suggest_refactoring(const std::vector<DuplicateBlock>& duplicates) {
    std::vector<RefactoringSuggestion> suggestions;

    for (const auto& dup : duplicates) {
        if (dup.duplicate_count >= 3) {
            RefactoringSuggestion sug;
            sug.type = "extract_function";
            sug.description = "Extract duplicate code block into a separate function";
            sug.file_path = dup.locations[0].file_path;
            sug.line_number = dup.locations[0].line_number;
            sug.original_code = dup.code_snippet;
            sug.refactored_code = "// TODO: extract to function\n" + dup.code_snippet;
            sug.confidence = 0.8f;
            sug.rationale = "Code block duplicated " + std::to_string(dup.duplicate_count) + " times";
            suggestions.push_back(sug);
        }
    }

    return suggestions;
}

std::vector<std::string> DuplicateCodeDetector::get_all_source_files(const std::string& project_path) {
    std::vector<std::string> source_files;

    for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c++" || ext == ".h" || ext == ".hpp") {
                source_files.push_back(entry.path().string());
            }
        }
    }

    return source_files;
}

std::string DuplicateCodeDetector::normalize_code(const std::string& code) {
    std::string normalized = code;

    // Remove comments
    std::regex comment_regex("//.*|/\\*.*?\\*/");
    normalized = std::regex_replace(normalized, comment_regex, "");

    // Remove extra whitespace
    std::regex whitespace_regex("\\s+");
    normalized = std::regex_replace(normalized, whitespace_regex, " ");

    // Trim
    normalized.erase(normalized.begin(), std::find_if(normalized.begin(), normalized.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), normalized.end());

    return normalized;
}

std::vector<std::string> DuplicateCodeDetector::split_into_blocks(const std::string& code, size_t block_size) {
    std::vector<std::string> blocks;
    std::istringstream iss(code);
    std::string line;
    std::string current_block;

    size_t line_count = 0;
    while (std::getline(iss, line)) {
        current_block += line + "\n";
        line_count++;

        if (line_count >= block_size) {
            blocks.push_back(current_block);
            current_block.clear();
            line_count = 0;
        }
    }

    if (!current_block.empty()) {
        blocks.push_back(current_block);
    }

    return blocks;
}

} // namespace code_quality