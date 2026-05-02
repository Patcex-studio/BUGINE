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
#include <catch2/catch.hpp>
#include "ai_core/decision_engine.h"
#include "ai_core/control_context.h"
#include "ai_core/ai_config.h"
#include "ai_core/micro_network.h"
#include "ai_core/decision_engine_stub.h"
#include "ai_core/fallback_chain.h"
#include "ai_core/neural_adapter.h"
#include "ai_core/profile_manager.h"

// Mock Neural Adapter for testing
class MockNeuralAdapter : public INeuralAdapter {
public:
    MockNeuralAdapter(float confidence) : confidence_(confidence) {}
    
    std::expected<Tensor, ErrorCode> Forward(const Tensor& input) override {
        Tensor output;
        output.data = {0.0f, 0.0f, confidence_};  // Last element is confidence
        output.shape = {3};
        return output;
    }
    
    bool LoadModel(const std::string&) override { return true; }
    
private:
    float confidence_;
};

// Mock Utility Engine
class MockUtilityEngine : public IDecisionEngine {
public:
    DecisionResult Execute(DecisionRequest& req, std::vector<Command>& out) override {
        Command cmd;
        cmd.type = CommandType::PATROL;
        out = {cmd};
        return DecisionResult::Success;
    }
    
    void ResetContext(EntityId) override {}
    std::optional<BTNodeSnapshot> SaveSnapshot(EntityId) const override { return std::nullopt; }
    bool RestoreSnapshot(EntityId, const BTNodeSnapshot&) override { return false; }
};

static WorldState MakeWorldState(uint64_t frame_id) {
    WorldState ws;
    ws.frame_id = frame_id;
    ws.time_ms = frame_id * 16.67f;  // 60 FPS
    return ws;
}

static DecisionRequest MakeRequest(EntityId entity_id, uint64_t frame_id) {
    DecisionRequest req;
    req.entity_id = entity_id;
    req.frame_id = frame_id;
    req.delta_time = 0.01667f;  // 60 FPS
    
    static WorldState ws = MakeWorldState(0);
    ws.frame_id = frame_id;
    req.world = &ws;
    
    return req;
}

// ============================================================================
// Unit Tests
// ============================================================================

TEST_CASE("IDecisionEngine Interface", "[decision_engine]") {
    SECTION("Engine creates without errors") {
        auto engine = std::make_unique<StubDecisionEngine>();
        REQUIRE(engine != nullptr);
    }
}

TEST_CASE("Control Context - State Transitions", "[control_context]") {
    ControlContext ctx;
    
    SECTION("Initial state is AI_AUTO") {
        REQUIRE(ctx.IsAIControlled());
        REQUIRE(!ctx.IsPlayerControlled());
    }
    
    SECTION("Transition to PLAYER_DIRECT") {
        ctx.TakeoverByPlayer(1000);
        REQUIRE(!ctx.IsAIControlled());
        REQUIRE(ctx.IsPlayerControlled());
        REQUIRE(ctx.takeover_time_ms == 1000);
    }
    
    SECTION("Transition back to AI_AUTO") {
        ctx.TakeoverByPlayer(1000);
        ctx.ReturnToAI();
        REQUIRE(ctx.IsAIControlled());
        REQUIRE(!ctx.IsPlayerControlled());
        REQUIRE(ctx.adaptation_time_remaining == ControlContext::ADAPTATION_DURATION);
    }
    
    SECTION("Clear cache resets adaptation") {
        ctx.adaptation_time_remaining = 0.2f;
        ctx.ClearCache();
        REQUIRE(ctx.adaptation_time_remaining == 0.0f);
    }
}

TEST_CASE("Command Interpolation (LerpCommands)", "[commands]") {
    std::vector<Command> cmd1, cmd2;
    
    Command c1;
    c1.type = CommandType::MOVE;
    c1.parameters[0] = 0.0f;
    cmd1.push_back(c1);
    
    Command c2;
    c2.type = CommandType::IDLE;
    c2.parameters[0] = 100.0f;
    cmd2.push_back(c2);
    
    SECTION("Blend 0.0f returns first command") {
        auto result = LerpCommands(cmd1, cmd2, 0.0f);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].type == CommandType::MOVE);
        REQUIRE(result[0].parameters[0] == 0.0f);
    }
    
    SECTION("Blend 1.0f returns second command") {
        auto result = LerpCommands(cmd1, cmd2, 1.0f);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].type == CommandType::IDLE);
        REQUIRE(result[0].parameters[0] == 100.0f);
    }
    
    SECTION("Blend 0.5f interpolates parameters") {
        auto result = LerpCommands(cmd1, cmd2, 0.5f);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].parameters[0] == Approx(50.0f));
    }
}

TEST_CASE("AIConfig - Difficulty Presets", "[ai_config]") {
    SECTION("Beginner config has longer reaction time") {
        auto beginner = AIConfig::ForDifficulty(DifficultyLevel::Beginner);
        auto elite = AIConfig::ForDifficulty(DifficultyLevel::Elite);
        
        REQUIRE(beginner.base_reaction_delay_ms > elite.base_reaction_delay_ms);
        REQUIRE(beginner.move_speed_multiplier < elite.move_speed_multiplier);
    }
    
    SECTION("Elite config has better accuracy") {
        auto beginner = AIConfig::ForDifficulty(DifficultyLevel::Beginner);
        auto elite = AIConfig::ForDifficulty(DifficultyLevel::Elite);
        
        REQUIRE(elite.accuracy_base > beginner.accuracy_base);
    }
}

TEST_CASE("EffectiveDelayParams Calculation", "[ai_config]") {
    auto config = AIConfig::ForDifficulty(DifficultyLevel::Veteran);
    
    SECTION("No stress/fatigue returns base delay") {
        UnitAIParams params;
        params.stress = 0.0f;
        params.fatigue = 0.0f;
        
        auto result = EffectiveDelayParams::Calculate(config, params);
        REQUIRE(result.reaction_delay_ms == Approx(config.base_reaction_delay_ms));
    }
    
    SECTION("Stress increases reaction delay") {
        UnitAIParams params_low, params_high;
        params_low.stress = 0.0f;
        params_high.stress = 1.0f;
        
        auto result_low = EffectiveDelayParams::Calculate(config, params_low);
        auto result_high = EffectiveDelayParams::Calculate(config, params_high);
        
        REQUIRE(result_high.reaction_delay_ms > result_low.reaction_delay_ms);
    }
    
    SECTION("Fatigue decreases reaction delay") {
        UnitAIParams params_low, params_high;
        params_low.fatigue = 0.0f;
        params_high.fatigue = 1.0f;
        
        auto result_low = EffectiveDelayParams::Calculate(config, params_low);
        auto result_high = EffectiveDelayParams::Calculate(config, params_high);
        
        REQUIRE(result_low.reaction_delay_ms > result_high.reaction_delay_ms);
    }
}

TEST_CASE("StubDecisionEngine - Basic Operation", "[decision_engine_stub]") {
    auto engine = std::make_unique<StubDecisionEngine>();
    std::vector<Command> out_commands;
    
    SECTION("Execute returns Success") {
        auto req = MakeRequest(1, 0);
        auto result = engine->Execute(req, out_commands);
        
        REQUIRE(result == DecisionResult::Success);
        REQUIRE(out_commands.size() > 0);
        REQUIRE(out_commands[0].type == CommandType::IDLE);
    }
    
    SECTION("Reset clears context") {
        auto req = MakeRequest(1, 0);
        engine->Execute(req, out_commands);
        engine->ResetContext(1);
        
        // After reset, first execution should succeed
        out_commands.clear();
        auto result = engine->Execute(req, out_commands);
        REQUIRE(result == DecisionResult::Success);
    }
}

TEST_CASE("StubDecisionEngine - Snapshot System", "[decision_engine_stub]") {
    auto engine = std::make_unique<StubDecisionEngine>();
    std::vector<Command> out_commands;
    
    SECTION("SaveSnapshot returns valid snapshot") {
        auto req = MakeRequest(1, 0);
        engine->Execute(req, out_commands);
        
        auto snap = engine->SaveSnapshot(1);
        REQUIRE(snap.has_value());
        REQUIRE(snap->tree_id == 0);  // Stub uses ID 0
    }
    
    SECTION("RestoreSnapshot succeeds") {
        auto req = MakeRequest(1, 0);
        engine->Execute(req, out_commands);
        
        auto snap = engine->SaveSnapshot(1);
        REQUIRE(snap.has_value());
        
        bool restored = engine->RestoreSnapshot(1, snap.value());
        REQUIRE(restored);
    }
    
    SECTION("ValidateSnapshot checks validity") {
        auto req = MakeRequest(1, 0);
        engine->Execute(req, out_commands);
        
        auto snap = engine->SaveSnapshot(1);
        auto world = MakeWorldState(0);
        
        REQUIRE(snap.has_value());
        bool valid = engine->ValidateSnapshot(snap.value(), world);
        REQUIRE(valid);
    }
}

TEST_CASE("StubDecisionEngine - Reaction Lag Caching", "[decision_engine_stub]") {
    auto engine = std::make_unique<StubDecisionEngine>();
    auto config = AIConfig::ForDifficulty(AIConfig::Difficulty::NORMAL);
    engine->SetAIConfig(config);
    
    UnitAIParams params;
    params.stress = 0.0f;  // base_reaction_delay_ms = 300ms
    params.fatigue = 0.0f;
    engine->SetUnitAIParams(1, params);
    
    std::vector<Command> out_commands;
    
    SECTION("Caches decision during lag period") {
        auto req = MakeRequest(1, 0);
        auto result1 = engine->Execute(req, out_commands);
        auto cmd1 = out_commands;
        
        // Next frame (16.67ms later, still within lag)
        auto req2 = MakeRequest(1, 1);
        auto result2 = engine->Execute(req2, out_commands);
        
        // Should return cached command
        REQUIRE(out_commands == cmd1);
    }
}

TEST_CASE("StubDecisionEngine - Unit AI Parameters", "[decision_engine_stub]") {
    auto engine = std::make_unique<StubDecisionEngine>();
    
    UnitAIParams params;
    params.stress = 0.7f;
    params.fatigue = 0.2f;
    params.morale = 1.2f;
    
    engine->SetUnitAIParams(42, params);
    
    SECTION("Parameters are stored and retrieved") {
        auto retrieved = engine->GetUnitAIParams(42);
        REQUIRE(retrieved.stress == Approx(0.7f));
        REQUIRE(retrieved.fatigue == Approx(0.2f));
        REQUIRE(retrieved.morale == Approx(1.2f));
    }
}

TEST_CASE("InputBuffer - Input Tracking", "[control_context]") {
    InputBuffer buf;
    
    SECTION("Empty buffer reports no input") {
        REQUIRE(!buf.HasInput());
    }
    
    SECTION("Button press detected") {
        buf.button_flags = 0x01;
        REQUIRE(buf.HasInput());
    }
    
    SECTION("Axis movement detected") {
        buf.axis_x = 1.0f;
        REQUIRE(buf.HasInput());
    }
    
    SECTION("Clear resets buffer") {
        buf.button_flags = 0xFF;
        buf.axis_x = 1.0f;
        buf.Clear();
        
        REQUIRE(buf.button_flags == 0);
        REQUIRE(buf.axis_x == 0.0f);
        REQUIRE(!buf.HasInput());
    }
}

TEST_CASE("InputSmoother produces smooth movement vectors", "[micro_network]") {
    InputSmoother smoother(128);
    InputState current{{1.0f, 0.0f}, static_cast<uint8_t>(InputFlags::None), Stance::Standing};
    std::vector<InputState> inputs{current};
    std::vector<Vector2> smoothed(1);

    smoother.SmoothBatch(inputs, smoothed, 0.01667f, 1, 0.05f);
    REQUIRE(smoothed[0].x == Approx(1.0f));
    REQUIRE(smoothed[0].y == Approx(0.0f));
}

TEST_CASE("StanceController transition duration scales with fatigue", "[micro_network]") {
    StanceController controller;
    controller.StartTransition(Stance::Prone, 0.8f);
    controller.Update(0.4f, 0.4f, 0.4f);
    REQUIRE(controller.state == StanceTransitionState::Transitioning);
    controller.Update(0.4f, 0.4f, 0.4f);
    REQUIRE(controller.state == StanceTransitionState::Idle);
    REQUIRE(controller.current == Stance::Prone);
}

TEST_CASE("MicroNetworkAdapter falls back when network is missing", "[micro_network]") {
    MicroNetworkAdapter adapter;
    MicroInput input;
    input.hp = 90.0f;
    input.ammo = 5.0f;
    input.fatigue = 0.2f;
    input.stress = 0.7f;
    input.nearest_threat_dist = 20.0f;
    input.cover_dist = 3.0f;
    input.stance = static_cast<uint8_t>(Stance::Crouching);
    input.role = 1;

    auto probabilities = adapter.Evaluate(input);
    REQUIRE(probabilities.size() == 5);
    for (float p : probabilities) {
        REQUIRE(p == Approx(0.2f));
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("AI→Player→AI Control Flow", "[integration]") {
    auto engine = std::make_unique<StubDecisionEngine>();
    ControlContext ctx;
    std::vector<Command> out_commands;
    
    SECTION("Natural control transition") {
        // Start with AI control
        REQUIRE(ctx.IsAIControlled());
        
        // Player takes over
        ctx.TakeoverByPlayer(100);
        REQUIRE(ctx.IsPlayerControlled());
        
        // Execute AI during player control (should be ignored)
        auto req = MakeRequest(1, 0);
        engine->Execute(req, out_commands);
        
        // Return to AI
        ctx.ReturnToAI();
        REQUIRE(ctx.IsAIControlled());
        REQUIRE(ctx.adaptation_time_remaining > 0.0f);
        
        // AI should have saved state for smooth return
        auto snap = engine->SaveSnapshot(1);
        REQUIRE(snap.has_value());
    }
}

TEST_CASE("Performance - Reaction Delay Calculation", "[performance]") {
    auto config = AIConfig::ForDifficulty(AIConfig::Difficulty::HARD);
    
    SECTION("Calculation completes quickly") {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            UnitAIParams params;
            params.stress = static_cast<float>(i % 100) / 100.0f;
            params.fatigue = static_cast<float>(i % 50) / 50.0f;
            
            auto result = EffectiveDelayParams::Calculate(config, params);
            (void)result;  // Use to avoid optimization
        }
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        REQUIRE(ms < 10);  // 1000 calculations in < 10ms
    }
}

TEST_CASE("Memory - Snapshot Size", "[memory]") {
    SECTION("BTNodeSnapshot is reasonably sized") {
        BTNodeSnapshot snap;
        // Add some data to blackboard
        snap.blackboard.resize(128);
        
        REQUIRE(sizeof(snap) <= 256);
    }
}

TEST_CASE("FallbackChain - Neural Network Integration", "[fallback_chain]") {
    DecisionRequest req;
    req.entity_id = 1;
    AIConfig config = AIConfig::ForDifficulty(DifficultyLevel::Veteran);
    
    SECTION("StubNeuralAdapter always falls back to Utility") {
        auto stub_adapter = std::make_unique<StubNeuralAdapter>();
        auto utility = std::make_unique<MockUtilityEngine>();
        FallbackChain chain(std::move(stub_adapter), std::move(utility));
        
        FallbackDecision result = chain.Resolve(req, config);
        REQUIRE(result.source == DecisionSource::UtilityAI);
        REQUIRE(result.command.type == CommandType::PATROL);
        REQUIRE(result.confidence == 0.8f);
    }
    
    SECTION("MockNeuralAdapter with high confidence uses NN") {
        auto mock_adapter = std::make_unique<MockNeuralAdapter>(0.9f);  // Above threshold
        auto utility = std::make_unique<MockUtilityEngine>();
        FallbackChain chain(std::move(mock_adapter), std::move(utility));
        
        FallbackDecision result = chain.Resolve(req, config);
        REQUIRE(result.source == DecisionSource::NeuralNetwork);
        REQUIRE(result.confidence == 0.9f);
    }
    
    SECTION("MockNeuralAdapter with low confidence falls back") {
        config.nn_min_confidence = 0.8f;  // Set high threshold
        auto mock_adapter = std::make_unique<MockNeuralAdapter>(0.6f);  // Below threshold
        auto utility = std::make_unique<MockUtilityEngine>();
        FallbackChain chain(std::move(mock_adapter), std::move(utility));
        
        FallbackDecision result = chain.Resolve(req, config);
        REQUIRE(result.source == DecisionSource::UtilityAI);
    }
}

TEST_CASE("ProfileManager - Difficulty Switching", "[profile_manager]") {
    ProfileManager pm;
    AIConfig cfg;
    
    SECTION("Apply Beginner profile") {
        pm.ApplyProfile(1, DifficultyLevel::Beginner, cfg);
        REQUIRE(cfg.difficulty == DifficultyLevel::Beginner);
        REQUIRE(cfg.nn_min_confidence == 0.8f);
    }
    
    SECTION("Apply Elite profile") {
        pm.ApplyProfile(1, DifficultyLevel::Elite, cfg);
        REQUIRE(cfg.difficulty == DifficultyLevel::Elite);
        REQUIRE(cfg.nn_min_confidence == 0.5f);
    }
}
