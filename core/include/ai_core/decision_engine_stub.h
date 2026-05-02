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

#include "decision_engine.h"
#include "ai_config.h"
#include <unordered_map>
#include <memory>
#include <chrono>

// ============================================================================
// Stub Decision Engine Implementation
// ============================================================================

/**
 * StubDecisionEngine
 * 
 * Minimal implementation for Phase 1 testing
 * Returns "stay put" commands, allowing control system integration tests
 * 
 * Features implemented:
 * - Simple state reset
 * - Optional snapshot support
 * - Reaction lag caching
 * - Frame budget awareness
 * 
 * This serves as the foundation for full behavior tree implementation
 */
class StubDecisionEngine : public IDecisionEngine {
public:
    StubDecisionEngine();
    virtual ~StubDecisionEngine();
    
    /**
     * IDecisionEngine implementation
     */
    
    DecisionResult Execute(DecisionRequest& req,
                           std::vector<Command>& out_commands) override;
    
    void ResetContext(EntityId entity_id) override;
    
    std::optional<BTNodeSnapshot> SaveSnapshot(EntityId entity_id) const override;
    
    bool RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snap) override;
    
    bool ValidateSnapshot(const BTNodeSnapshot& snap,
                          const WorldState& world) const override;
    
    /**
     * Configuration
     */
    
    void SetAIConfig(const AIConfig& config);
    const AIConfig& GetAIConfig() const { return ai_config_; }
    
    /**
     * Optional: Unit's AI parameters (stress, fatigue, etc.)
     */
    void SetUnitAIParams(EntityId entity_id, const UnitAIParams& params);
    UnitAIParams GetUnitAIParams(EntityId entity_id) const;

private:
    // Per-entity context
    struct EntityContext {
        uint64_t last_decision_frame = 0;  // Frame when last decision was made
        float last_decision_time_ms = 0.0f;
        std::vector<Command> cached_commands;
        std::optional<BTNodeSnapshot> saved_state;
        UnitAIParams ai_params;
    };
    
    AIConfig ai_config_;
    std::unordered_map<EntityId, EntityContext> contexts_;
    
    /**
     * Calculate effective reaction delay based on unit stress/fatigue
     */
    float CalculateEffectiveDelay(const UnitAIParams& params) const;
    
    /**
     * Check if cached decision is still valid
     */
    bool IsCacheValid(EntityId entity_id, uint64_t frame_id) const;
    
    /**
     * Generate minimal stub command
     */
    std::vector<Command> GenerateIdleCommand() const;
};
