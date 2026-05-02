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
#include <gtest/gtest.h>
#include <filesystem>
#include "ai_core/replay_recorder.h"
#include "ai_core/replay_player.h"

TEST(ReplaySystem, Roundtrip) {
    namespace fs = std::filesystem;
    const fs::path temp_dir = fs::temp_directory_path() / "rhw_replay_test";
    fs::create_directories(temp_dir);

    const std::string filename = "roundtrip_test.rpl";
    ReplayRecorder::Config cfg;
    cfg.output_directory = temp_dir.string();
    cfg.enable_compression = false;
    cfg.enable_checksums = true;
    cfg.ring_buffer_size = 65536;
    cfg.flush_threshold = 32768;

    ReplayRecorder recorder(0xDEADBEEFULL, cfg, "test-version");
    ASSERT_TRUE(recorder.StartRecording(filename));

    CompactInput input;
    input.keys_pressed = 0x1;
    input.mouse_x = 0.5f;
    input.mouse_y = 0.5f;
    input.analog_x = 0.1f;
    input.analog_y = 0.9f;

    Command cmd;
    cmd.type = CommandType::MOVE;
    cmd.priority = 0.75f;
    cmd.target_id = 42;
    cmd.parameters[0] = 10.0f;
    cmd.parameters[1] = 20.0f;

    DecisionRecord decision;
    decision.entity_id = 1;
    decision.source = DecisionSource::BehaviorTree;
    decision.bt_node_index = 0xFFFFFFFFU;
    decision.command = CompactCommand::Encode(cmd);
    decision.confidence = 0.85f;

    std::vector<float> positions_x = {0.0f};
    std::vector<float> positions_y = {0.0f};
    std::vector<float> positions_z = {0.0f};
    std::vector<float> velocities_x = {0.0f};
    std::vector<float> velocities_y = {0.0f};
    std::vector<float> velocities_z = {0.0f};
    std::vector<float> health = {1.0f};
    std::vector<float> fatigue = {0.0f};
    std::vector<float> stress = {0.0f};
    std::vector<float> morale = {1.0f};
    std::vector<uint8_t> stance = {0};
    std::vector<float> stance_progress = {0.0f};
    std::vector<uint8_t> stance_state = {0};
    std::vector<PerceptionState<16>> perception_states = {PerceptionState<16>(50.0f, 30.0f)};

    UnitSoA units;
    units.entity_id = 1;
    units.positions_x = positions_x;
    units.positions_y = positions_y;
    units.positions_z = positions_z;
    units.velocities_x = velocities_x;
    units.velocities_y = velocities_y;
    units.velocities_z = velocities_z;
    units.health = health;
    units.fatigue = fatigue;
    units.stress = stress;
    units.morale = morale;
    units.stance = stance;
    units.stance_transition_progress = stance_progress;
    units.stance_transition_state = stance_state;
    units.perception_states = perception_states;

    recorder.RecordFrame(0, 0.016f,
                         std::span<const CompactInput>(&input, 1),
                         std::span<const DecisionRecord>(&decision, 1),
                         units);
    recorder.StopRecording();

    const fs::path replay_file = temp_dir / filename;
    ASSERT_TRUE(fs::exists(replay_file));

    ReplayPlayer player;
    ASSERT_TRUE(player.LoadReplay(replay_file.string()));
    EXPECT_EQ(player.GetHeader().seed, 0xDEADBEEFULL);
    EXPECT_EQ(player.GetHeader().format_version, REPLAY_FORMAT_VERSION);
    EXPECT_EQ(player.GetHeader().frame_count, 1u);
    EXPECT_TRUE(player.IsLoaded());

    const ReplayFrame* loaded_frame = player.GetCurrentFrame();
    ASSERT_NE(loaded_frame, nullptr);
    EXPECT_EQ(loaded_frame->frame_number, 0u);
    EXPECT_EQ(loaded_frame->player_inputs.size(), 1u);
    EXPECT_EQ(loaded_frame->ai_decisions.size(), 1u);
    EXPECT_EQ(loaded_frame->ai_decisions[0].entity_id, 1u);
    EXPECT_NEAR(loaded_frame->ai_decisions[0].confidence, 0.85f, 1e-6f);
}
