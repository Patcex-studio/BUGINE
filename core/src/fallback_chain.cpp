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
#include "ai_core/fallback_chain.h"
#include "ai_core/decision_engine.h"  // For DecisionRequest

FallbackChain::FallbackChain(std::unique_ptr<INeuralAdapter> adapter, std::unique_ptr<IDecisionEngine> utility)
    : adapter_(std::move(adapter)), utility_(std::move(utility)) {}

FallbackDecision FallbackChain::Resolve(const DecisionRequest& ctx, const AIConfig& profile) {
    // 1. Try Neural Network
    if (profile.nn_enabled && adapter_) {
        // Encode context to tensor (placeholder)
        Tensor input;
        // TODO: Implement EncodeToTensor(ctx)
        input.data = {0.5f, 0.3f, 0.8f};  // Placeholder
        input.shape = {3};
        
        auto nn_result = adapter_->Forward(input);
        if (nn_result.has_value()) {
            const Tensor& output = *nn_result;
            // Assume last element is confidence
            float confidence = output.data.back();
            if (confidence >= profile.nn_min_confidence) {
                // Decode command from output
                Command cmd;
                cmd.type = CommandType::ATTACK;  // Placeholder
                return {DecisionSource::NeuralNetwork, cmd, confidence};
            }
        }
    }
    
    // 2. Fallback to Utility AI
    std::vector<Command> commands;
    DecisionRequest req = ctx;  // Copy
    DecisionResult result = utility_->Execute(req, commands);
    if (result == DecisionResult::Success && !commands.empty()) {
        return {DecisionSource::UtilityAI, commands[0], 0.8f};
    }
    
    // 3. Safe Default
    Command safe_cmd;
    safe_cmd.type = CommandType::SEEK_COVER;
    return {DecisionSource::SafeDefault, safe_cmd, 1.0f};
}