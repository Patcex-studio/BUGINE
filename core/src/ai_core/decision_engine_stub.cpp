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
#include "../include/ai_core/decision_engine_stub.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// StubDecisionEngine Implementation
// ============================================================================

StubDecisionEngine::StubDecisionEngine()
    : ai_config_(AIConfig::ForDifficulty(AIConfig::Difficulty::NORMAL)) {
}

StubDecisionEngine::~StubDecisionEngine() {
    contexts_.clear();
}

void StubDecisionEngine::SetAIConfig(const AIConfig& config) {
    ai_config_ = config;
}

void StubDecisionEngine::SetUnitAIParams(EntityId entity_id, const UnitAIParams& params) {
    auto it = contexts_.find(entity_id);
    if (it != contexts_.end()) {
        it->second.ai_params = params;
    } else {
        EntityContext ctx;
        ctx.ai_params = params;
        contexts_[entity_id] = ctx;
    }
}

UnitAIParams StubDecisionEngine::GetUnitAIParams(EntityId entity_id) const {
    auto it = contexts_.find(entity_id);
    if (it != contexts_.end()) {
        return it->second.ai_params;
    }
    return UnitAIParams();
}

float StubDecisionEngine::CalculateEffectiveDelay(const UnitAIParams& params) const {
    // Formula: base_delay * (1.0 + stress * stress_sensitivity - fatigue * fatigue_sensitivity)
    float multiplier = 1.0f 
        + params.stress * ai_config_.stress_sensitivity
        - params.fatigue * ai_config_.fatigue_sensitivity;
    
    float effective_delay = ai_config_.base_reaction_delay_ms * multiplier;
    
    // Clamp to reasonable range
    return std::max(50.0f, std::min(2000.0f, effective_delay));
}

bool StubDecisionEngine::IsCacheValid(EntityId entity_id, uint64_t frame_id) const {
    auto it = contexts_.find(entity_id);
    if (it == contexts_.end()) {
        return false;
    }
    
    const EntityContext& ctx = it->second;
    
    // If we haven't made any decision yet, cache is invalid
    if (ctx.last_decision_frame == 0) {
        return false;
    }
    
    // Calculate effective delay for this unit
    float effective_delay_ms = CalculateEffectiveDelay(ctx.ai_params);
    
    // Consider cache valid if we're still within the delay window
    // Note: This is a simplified check - real implementation would use time_ms
    // For now, assume 60 FPS -> each frame is ~16.67ms
    float ms_per_frame = 16.67f;
    uint64_t delay_frames = static_cast<uint64_t>(effective_delay_ms / ms_per_frame);
    
    return (frame_id - ctx.last_decision_frame) < delay_frames;
}

std::vector<Command> StubDecisionEngine::GenerateIdleCommand() const {
    std::vector<Command> commands;
    commands.push_back(MakeNullCommand());
    return commands;
}

DecisionResult StubDecisionEngine::Execute(DecisionRequest& req,
                                          std::vector<Command>& out_commands) {
    // Ensure entity context exists
    auto it = contexts_.find(req.entity_id);
    if (it == contexts_.end()) {
        contexts_[req.entity_id] = EntityContext();
        it = contexts_.find(req.entity_id);
    }
    
    EntityContext& ctx = it->second;
    
    // Check if we should use cached decision (reaction lag)
    if (req.world && IsCacheValid(req.entity_id, req.world->frame_id)) {
        // Return cached command without re-evaluating
        out_commands = ctx.cached_commands;
        return DecisionResult::Running;  // Continue using cached decision
    }
    
    // Generate new decision (stub just returns idle)
    out_commands = GenerateIdleCommand();
    
    // Update decision cache
    ctx.cached_commands = out_commands;
    if (req.world) {
        ctx.last_decision_frame = req.world->frame_id;
    }
    
    // Calculate effective delay
    float effective_delay_ms = CalculateEffectiveDelay(ctx.ai_params);
    ctx.last_decision_time_ms = effective_delay_ms;
    
    return DecisionResult::Success;
}

void StubDecisionEngine::ResetContext(EntityId entity_id) {
    auto it = contexts_.find(entity_id);
    if (it != contexts_.end()) {
        EntityContext& ctx = it->second;
        ctx.last_decision_frame = 0;
        ctx.last_decision_time_ms = 0.0f;
        ctx.cached_commands.clear();
        ctx.saved_state.reset();
        // Keep ai_params unchanged
    }
}

std::optional<BTNodeSnapshot> StubDecisionEngine::SaveSnapshot(EntityId entity_id) const {
    auto it = contexts_.find(entity_id);
    if (it == contexts_.end()) {
        return std::nullopt;
    }
    
    const EntityContext& ctx = it->second;
    
    // Return the saved state if it exists
    if (ctx.saved_state.has_value()) {
        return ctx.saved_state;
    }
    
    // For stub, create a minimal snapshot
    BTNodeSnapshot snap;
    snap.tree_id = 0;
    snap.node_index = 0;
    snap.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return snap;
}

bool StubDecisionEngine::RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snap) {
    auto it = contexts_.find(entity_id);
    if (it == contexts_.end()) {
        contexts_[entity_id] = EntityContext();
        it = contexts_.find(entity_id);
    }
    
    EntityContext& ctx = it->second;
    ctx.saved_state = snap;
    
    // Reset decision frame to force re-evaluation
    ctx.last_decision_frame = 0;
    
    return true;
}

bool StubDecisionEngine::ValidateSnapshot(const BTNodeSnapshot& snap,
                                          const WorldState& world) const {
    // Stub implementation: always valid
    // Real implementation would check:
    // - Target entity still exists
    // - Critical parameters unchanged (distance, cover status)
    // - World state hasn't changed significantly
    
    if (!snap.tree_id || !snap.node_index) {
        return false;  // Invalid snapshot
    }
    
    // For stub, accept valid-looking snapshots
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<Command> LerpCommands(const std::vector<Command>& cmd1,
                                  const std::vector<Command>& cmd2,
                                  float blend_factor) {
    // Clamp blend to [0, 1]
    blend_factor = std::max(0.0f, std::min(1.0f, blend_factor));
    
    std::vector<Command> result;
    
    // Simple linear interpolation between command sets
    // If command counts differ, use the longer set
    size_t max_size = std::max(cmd1.size(), cmd2.size());
    result.reserve(max_size);
    
    for (size_t i = 0; i < max_size; ++i) {
        Command blend_cmd;
        
        const Command& c1 = (i < cmd1.size()) ? cmd1[i] : MakeNullCommand();
        const Command& c2 = (i < cmd2.size()) ? cmd2[i] : MakeNullCommand();
        
        // Blend command type (use c2 if blend > 0.5)
        blend_cmd.type = (blend_factor > 0.5f) ? c2.type : c1.type;
        
        // Interpolate priority and parameters
        blend_cmd.priority = c1.priority * (1.0f - blend_factor) + c2.priority * blend_factor;
        blend_cmd.target_id = (blend_factor > 0.5f) ? c2.target_id : c1.target_id;
        
        for (int j = 0; j < 4; ++j) {
            blend_cmd.parameters[j] = 
                c1.parameters[j] * (1.0f - blend_factor) + 
                c2.parameters[j] * blend_factor;
        }
        
        result.push_back(blend_cmd);
    }
    
    return result;
}
