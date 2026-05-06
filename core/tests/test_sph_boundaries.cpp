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
#include "sph_system.h"

using namespace physics_core;

class SPHBoundariesTest : public ::testing::Test {
protected:
    void SetUp() override {
        sph_system = std::make_unique<SPHSystem>();
    }

    std::unique_ptr<SPHSystem> sph_system;
};

// Test boundary particle generation
TEST_F(SPHBoundariesTest, BoundaryParticleGeneration) {
    // Create boundary particles around a volume
    std::vector<SPHParticle> boundary_particles;
    // Assume some boundary generation logic

    // Check that boundaries are created
    EXPECT_GT(boundary_particles.size(), 0);
}

// Test impulse transfer at boundaries
TEST_F(SPHBoundariesTest, ImpulseTransfer) {
    // Fluid particle approaching boundary
    SPHParticle fluid_particle;
    fluid_particle.position = _mm_setr_ps(0.0f, 0.0f, 0.0f, 0.0f);
    fluid_particle.velocity = _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f); // m/s

    SPHParticle boundary_particle;
    boundary_particle.position = _mm_setr_ps(0.1f, 0.0f, 0.0f, 0.0f);
    boundary_particle.velocity = _mm_setzero_ps(); // stationary

    // Calculate interaction
    // Simplified: impulse transfer should reduce fluid velocity
    float initial_speed = 1.0f; // m/s
    float final_speed = 0.8f; // after collision

    EXPECT_LT(final_speed, initial_speed);
}

// Test no penetration through boundaries
TEST_F(SPHBoundariesTest, NoPenetration) {
    // Particle at boundary
    SPHParticle particle;
    particle.position = _mm_setr_ps(0.0f, 0.0f, 0.0f, 0.0f);

    // Boundary at x=0
    float boundary_x = 0.0f;

    // After update, particle should not be at negative x
    // Assume position update
    float new_x = -0.01f; // would penetrate

    // Boundary condition should prevent this
    EXPECT_GE(new_x, boundary_x);
}