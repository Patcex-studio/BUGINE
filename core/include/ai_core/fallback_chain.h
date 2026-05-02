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

#include <memory>
#include "ai_core/ai_config.h"
#include "ai_core/neural_adapter.h"
#include "ai_core/decision_engine.h"  // For Command

enum class DecisionSource {
    NeuralNetwork,
    UtilityAI,
    BehaviorTree,
    SafeDefault
};

struct FallbackDecision {
    DecisionSource source;
    Command command;
    float confidence;  // 0..1
};

class FallbackChain {
public:
    FallbackChain(std::unique_ptr<INeuralAdapter> adapter, std::unique_ptr<IDecisionEngine> utility);
    
    FallbackDecision Resolve(const DecisionRequest& ctx, const AIConfig& profile);

private:
    std::unique_ptr<INeuralAdapter> adapter_;
    std::unique_ptr<IDecisionEngine> utility_;
};