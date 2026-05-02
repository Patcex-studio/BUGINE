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
#include "ai_core/decision_engine.h"
#include "ai_core/perception_system.h"
#include "ai_core/spatial_grid.h"
#include "ai_core/ai_config.h"
#include "ai_core/fallback_chain.h"
#include "ai_core/profile_manager.h"
#include "ai_core/neural_adapter.h"
#include <memory>
#include <unordered_map>

// ============================================================================
// DecisionEngine Implementation
// ============================================================================

class DecisionEngine : public IDecisionEngine {
    struct BTInstance {
        uint32_t tree_id = 0;
        uint32_t current_node = 0;
        std::array<uint8_t, 64> blackboard;
        uint8_t blackboard_size = 0;
        uint64_t last_snapshot_frame = 0;
    };

public:
    DecisionEngine() 
        : perception_system_(std::make_unique<PerceptionSystem>()),
          profile_manager_(std::make_unique<ProfileManager>()),
          fallback_chain_(std::make_unique<FallbackChain>(
              std::make_unique<StubNeuralAdapter>(),  // Default to stub
              nullptr  // TODO: Integrate utility engine
          )) {}
    ~DecisionEngine() override = default;

    DecisionResult Execute(DecisionRequest& request, std::vector<Command>& out_commands) override {
        // Update perception
        if (request.units && request.world) {
            perception_system_->Update(*request.units, spatial_grid_, *request.world, request.frame_id, ai_config_);
        }

        // Get profile for entity
        AIConfig profile = ai_config_;  // Default
        profile_manager_->ApplyProfile(request.entity_id, profile.difficulty, profile);

        // Resolve through fallback chain
        FallbackDecision fb = fallback_chain_->Resolve(request, profile);
        out_commands = {fb.command};

        return (fb.source == DecisionSource::SafeDefault) ? DecisionResult::Failure : DecisionResult::Success;
    }

    void ResetContext(EntityId entity_id) override {
        auto it = instances_.find(entity_id);
        if (it != instances_.end()) {
            it->second = BTInstance{};  // Reset to default
        }
    }

    std::optional<BTNodeSnapshot> SaveSnapshot(EntityId entity_id) const override {
        auto it = instances_.find(entity_id);
        if (it == instances_.end()) return std::nullopt;
        
        BTNodeSnapshot snap;
        snap.tree_id = it->second.tree_id;
        snap.node_index = it->second.current_node;
        snap.blackboard_size = it->second.blackboard_size;
        std::copy(it->second.blackboard.begin(), 
                 it->second.blackboard.begin() + it->second.blackboard_size,
                 snap.blackboard.begin());
        snap.timestamp = it->second.last_snapshot_frame;
        return snap;
    }

    bool RestoreSnapshot(EntityId entity_id, const BTNodeSnapshot& snapshot) override {
        auto& inst = instances_[entity_id];  // Creates if not exists
        inst.tree_id = snapshot.tree_id;
        inst.current_node = snapshot.node_index;
        inst.blackboard_size = snapshot.blackboard_size;
        std::copy(snapshot.blackboard.begin(), 
                 snapshot.blackboard.begin() + snapshot.blackboard_size,
                 inst.blackboard.begin());
        inst.last_snapshot_frame = snapshot.timestamp;
        return true;
    }

    bool ValidateSnapshot(const BTNodeSnapshot& snapshot, const WorldState& world) const override {
        return world.frame_id == snapshot.timestamp;
    }

    // Additional methods for NN and profiles
    void SetNeuralAdapter(std::unique_ptr<INeuralAdapter> adapter) {
        fallback_chain_ = std::make_unique<FallbackChain>(std::move(adapter), nullptr);
    }

private:
    std::unique_ptr<PerceptionSystem> perception_system_;
    SpatialGrid spatial_grid_;
    AIConfig ai_config_;
    std::unique_ptr<ProfileManager> profile_manager_;
    std::unique_ptr<FallbackChain> fallback_chain_;
    std::unordered_map<EntityId, BTInstance> instances_;
};

// Factory function
std::unique_ptr<IDecisionEngine> CreateDecisionEngine() {
    return std::make_unique<DecisionEngine>();
}