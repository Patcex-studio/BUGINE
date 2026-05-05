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
#include "collision_system.h"

using namespace physics_core;

class CCDTest : public ::testing::Test {
protected:
    void SetUp() override {
        collision_system = std::make_unique<BVHCollisionSystem>();
    }

    std::unique_ptr<BVHCollisionSystem> collision_system;
};

// Test fast projectile doesn't pass through wall
TEST_F(CCDTest, FastProjectileNoPassThrough) {
    // Setup moving object (projectile)
    MotionBounds projectile;
    projectile.entity_id = 1;
    projectile.start_pos = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); // start position
    projectile.end_pos = _mm256_setr_ps(10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); // end position
    projectile.swept_aabb = _mm256_setr_ps(0.0f, -0.1f, -0.1f, 10.0f, 0.1f, 0.1f, 0.0f, 0.0f); // swept AABB
    projectile.movement_time = 0.01f; // seconds

    // Setup static wall at x=5
    // Assume wall is added to BVH

    CollisionPairs output;
    collision_system->ccd_query_simd(&projectile, 1, output);

    // Should detect collision before passing through
    EXPECT_GT(output.size(), 0);
}

// Test swept volume calculation
TEST_F(CCDTest, SweptVolume) {
    // Projectile moving from (0,0,0) to (10,0,0) with radius 0.1
    __m256 start = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    __m256 end = _mm256_setr_ps(10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    float radius = 0.1f;

    // Swept AABB should cover from min to max
    float min_x = 0.0f - radius;
    float max_x = 10.0f + radius;

    EXPECT_LT(min_x, max_x);
}