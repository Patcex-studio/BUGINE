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
#include "rendering_engine/model_system.h"
#include <glm/glm.hpp>

namespace rendering_engine {

// Test fixture for ModelSystem
class ModelSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize model system
        model_system_ = std::make_unique<ModelSystem>();
        model_system_->initialize(nullptr, nullptr, nullptr); // No external dependencies for basic tests
    }

    void TearDown() override {
        model_system_.reset();
    }

    std::unique_ptr<ModelSystem> model_system_;
};

// Test procedural generator validation
TEST_F(ModelSystemTest, BlueprintValidation) {
    // Valid blueprint
    VehicleBlueprint valid_blueprint;
    valid_blueprint.vehicle_name = "Test Tank";
    valid_blueprint.vehicle_type = 0; // TANK
    valid_blueprint.mass_total = 50000.0f;

    ComponentBlueprint component;
    component.component_type = 0; // HULL
    component.health_max = 100.0f;
    component.armor_thickness = 50.0f;
    component.is_destructible = true;
    valid_blueprint.components.push_back(component);

    EXPECT_TRUE(model_system_->generate_vehicle_from_blueprint(valid_blueprint, {}, ModelInstance{}));

    // Invalid blueprint - no components
    VehicleBlueprint invalid_blueprint;
    invalid_blueprint.vehicle_name = "Invalid";
    invalid_blueprint.mass_total = 1000.0f;

    ModelInstance dummy_instance;
    EXPECT_FALSE(model_system_->generate_vehicle_from_blueprint(invalid_blueprint, {}, dummy_instance));
}

// Test LOD manager
TEST_F(ModelSystemTest, LODSelection) {
    // Create test instances
    std::vector<ModelInstance*> instances;

    ModelInstance instance1;
    instance1.world_transform.f32[12] = 100.0f; // X position
    instance1.world_transform.f32[13] = 0.0f;   // Y
    instance1.world_transform.f32[14] = 0.0f;   // Z
    instances.push_back(&instance1);

    ModelInstance instance2;
    instance2.world_transform.f32[12] = 600.0f; // X position
    instance2.world_transform.f32[13] = 0.0f;   // Y
    instance2.world_transform.f32[14] = 0.0f;   // Z
    instances.push_back(&instance2);

    // Update LOD
    glm::vec3 camera_pos(0, 0, 0);
    model_system_->update_lod_selections(camera_pos, 0.016f); // 60 FPS

    // Check LOD assignments
    EXPECT_EQ(instance1.current_lod_level, 0); // Close, high detail
    EXPECT_GE(instance2.current_lod_level, 1); // Far, lower detail
}

// Test collision mesh generation
TEST_F(ModelSystemTest, CollisionMeshGeneration) {
    VehicleBlueprint blueprint;
    blueprint.vehicle_name = "Collision Test Tank";
    blueprint.vehicle_type = 0;
    blueprint.mass_total = 50000.0f;

    ComponentBlueprint hull;
    hull.component_type = 0;
    hull.health_max = 100.0f;
    hull.armor_thickness = 50.0f;
    hull.is_destructible = true;
    blueprint.components.push_back(hull);

    ComponentBlueprint turret;
    turret.component_type = 1;
    turret.health_max = 50.0f;
    turret.armor_thickness = 30.0f;
    turret.is_destructible = true;
    blueprint.components.push_back(turret);

    // Add connection between hull and turret
    SocketConnection connection;
    connection.parent_socket_id = 0;
    connection.child_socket_id = 0;
    connection.relative_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
    blueprint.connections.push_back(connection);

    ModelInstance instance;
    bool success = model_system_->generate_vehicle_from_blueprint(blueprint, {}, instance);

    EXPECT_TRUE(success);
    EXPECT_FALSE(instance.collision_vertices.empty());
    EXPECT_FALSE(instance.collision_indices.empty());
    EXPECT_FALSE(instance.collision_bvh.nodes.empty());
    EXPECT_EQ(instance.collision_bvh.nodes.size(), 3); // 2 leaves + 1 internal node
    EXPECT_EQ(instance.collision_bvh.root_index, 2); // Root is the last node
}

// Test performance targets
TEST_F(ModelSystemTest, PerformanceTargets) {
    // Generate multiple blueprints
    VehicleBlueprint blueprint;
    blueprint.vehicle_name = "Performance Test Tank";
    blueprint.vehicle_type = 0;
    blueprint.mass_total = 45000.0f;

    ComponentBlueprint hull;
    hull.component_type = 0;
    hull.health_max = 1000.0f;
    hull.armor_thickness = 100.0f;
    hull.is_destructible = true;
    blueprint.components.push_back(hull);

    ComponentBlueprint turret;
    turret.component_type = 1;
    turret.health_max = 500.0f;
    turret.armor_thickness = 80.0f;
    turret.is_destructible = true;
    blueprint.components.push_back(turret);

    // Measure generation time
    auto start = std::chrono::high_resolution_clock::now();

    ModelInstance instance;
    bool success = model_system_->generate_vehicle_from_blueprint(blueprint, {}, instance);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(success);
    EXPECT_LT(duration.count(), 15); // < 15ms target
}

// Test memory usage
TEST_F(ModelSystemTest, MemoryUsage) {
    // Create multiple instances
    std::vector<uint64_t> instance_ids;
    for (int i = 0; i < 100; ++i) {
        uint64_t id = model_system_->create_instance(i + 1, i + 100);
        instance_ids.push_back(id);
    }

    // Check memory per instance (rough estimate)
    size_t estimated_memory_per_instance = sizeof(ModelInstance);
    EXPECT_LE(estimated_memory_per_instance, 256UL); // < 256 bytes target

    // Cleanup
    for (uint64_t id : instance_ids) {
        model_system_->destroy_instance(id);
    }
}

// Test frustum culling
TEST_F(ModelSystemTest, FrustumCulling) {
    // Create test instances
    uint64_t id1 = model_system_->create_instance(1, 100);
    uint64_t id2 = model_system_->create_instance(2, 101);

    // Set up world bounds for instances
    ModelInstance* inst1 = model_system_->get_instance(id1);
    ModelInstance* inst2 = model_system_->get_instance(id2);

    // Instance 1: inside frustum
    inst1->world_bounds.min_bounds = _mm_set_ps(0, -1, -1, -1);
    inst1->world_bounds.max_bounds = _mm_set_ps(0, 1, 1, 1);

    // Instance 2: outside frustum (behind camera)
    inst2->world_bounds.min_bounds = _mm_set_ps(0, -1, -1, -10);
    inst2->world_bounds.max_bounds = _mm_set_ps(0, 1, 1, -5);

    // Create a simple view-projection matrix (perspective looking down +Z)
    glm::mat4 view = glm::lookAt(
        glm::vec3(0, 0, 0),  // eye
        glm::vec3(0, 0, 1),  // center
        glm::vec3(0, 1, 0)   // up
    );
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 view_proj = proj * view;

    // Get visible instances
    auto visible = model_system_->get_visible_instances(view_proj);

    // Should only return instance 1
    EXPECT_EQ(visible.size(), 1UL);
    EXPECT_EQ(visible[0], inst1);

    // Check render flags
    EXPECT_EQ(inst1->render_flags & RENDER_FLAG_SKIP, 0U); // Not skipped
    EXPECT_EQ(inst2->render_flags & RENDER_FLAG_SKIP, RENDER_FLAG_SKIP); // Skipped

    // Cleanup
    model_system_->destroy_instance(id1);
    model_system_->destroy_instance(id2);
}

} // namespace rendering_engine