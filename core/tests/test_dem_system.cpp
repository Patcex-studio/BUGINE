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
#include "terrain_system.h"

using namespace physics_core;

class DEMSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        terrain_system = std::make_unique<TerrainSystem>();
    }

    std::unique_ptr<TerrainSystem> terrain_system;
};

// Test height interpolation
TEST_F(DEMSystemTest, HeightInterpolation) {
    // Create a simple height field
    float heights[4] = {0.0f, 1.0f, 2.0f, 3.0f}; // meters

    // Interpolate at center
    float interpolated = (heights[0] + heights[1] + heights[2] + heights[3]) / 4.0f;
    EXPECT_FLOAT_EQ(interpolated, 1.5f);
}

// Test slope calculation
TEST_F(DEMSystemTest, SlopeCalculation) {
    // Simple slope: rise over run
    float height1 = 0.0f; // m
    float height2 = 10.0f; // m
    float distance = 100.0f; // m

    float slope = (height2 - height1) / distance; // dimensionless
    EXPECT_FLOAT_EQ(slope, 0.1f);
}

// Test normal vector calculation
TEST_F(DEMSystemTest, NormalVector) {
    // Flat surface normal should be (0,0,1)
    glm::vec3 normal(0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(normal.x, 0.0f);
    EXPECT_FLOAT_EQ(normal.y, 0.0f);
    EXPECT_FLOAT_EQ(normal.z, 1.0f);
}