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

#include "physics_body.h"
#include <vector>

namespace physics_core {

// ============================================================================
// SIMD Processing Unit
// ============================================================================

/**
 * @class SIMDProcessor
 * @brief SIMD-optimized physics batch processor
 * 
 * Criteria:
 * - Process 8 bodies per SIMD cycle (AVX2)
 * - Process 16 bodies per SIMD cycle (AVX-512 when available)
 * - Vectorize all arithmetic operations
 * - Support AVX2/AVX-512 instructions
 * 
 * Strategy:
 * - Convert bodies to SoA (Structure of Arrays) for cache efficiency
 * - Process in batches of 8 using __m256d (AVX2) or __m512d (AVX-512)
 * - Convert back to AoS (Array of Structures) after processing
 */
class SIMDProcessor {
public:
    SIMDProcessor();
    ~SIMDProcessor();

    // ========== Batch Operations ==========

    /**
     * Apply gravity to a batch of bodies
     * @param bodies Vector of bodies
     * @param gravity Gravity acceleration (m/s²)
     * @param dt Time step
     * @param start_index Start index in bodies array
     * @param count Number of bodies to process
     */
    void apply_gravity_batch(
        std::vector<PhysicsBody>& bodies,
        const Vec3& gravity,
        float dt,
        size_t start_index = 0,
        size_t count = ~0ull
    );

    /**
     * Update velocities in batch (v += a*dt)
     * @param bodies Vector of bodies
     * @param dt Time step
     */
    void update_velocities_batch(
        std::vector<PhysicsBody>& bodies,
        float dt,
        size_t start_index = 0,
        size_t count = ~0ull
    );

    /**
     * Update positions in batch (x += v*dt)
     * @param bodies Vector of bodies
     * @param dt Time step
     */
    void update_positions_batch(
        std::vector<PhysicsBody>& bodies,
        float dt,
        size_t start_index = 0,
        size_t count = ~0ull
    );

    /**
     * Apply damping in batch
     * @param bodies Vector of bodies
     * @param dt Time step
     */
    void apply_damping_batch(
        std::vector<PhysicsBody>& bodies,
        float dt,
        size_t start_index = 0,
        size_t count = ~0ull
    );

    /**
     * Integrate positions using Verlet (SIMD optimized)
     * x(t+dt) = 2*x(t) - x(t-dt) + a*dt²
     */
    void verlet_integrate_batch(
        std::vector<PhysicsBody>& bodies,
        const std::vector<Vec3>& prev_positions,
        float dt,
        size_t start_index = 0,
        size_t count = ~0ull
    );

    // ========== Utility ==========

    /**
     * Get optimal batch size for current CPU
     * (typically 8 for AVX2, 16 for AVX-512)
     */
    size_t get_optimal_batch_size() const { return optimal_batch_size_; }

    /**
     * Check if AVX-512 is available
     */
    bool has_avx512() const { return has_avx512_; }

    /**
     * Check if AVX2 is available
     */
    bool has_avx2() const { return has_avx2_; }

private:
    size_t optimal_batch_size_;
    bool has_avx512_;
    bool has_avx2_;

    void detect_simd_capabilities();
};

}  // namespace physics_core
