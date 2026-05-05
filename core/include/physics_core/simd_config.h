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

/**
 * @file simd_config.h
 * @brief SIMD capability detection and configuration for physics engine
 * 
 * This header provides unified SIMD level detection and fallback mechanisms
 * across all physics systems (constraint solver, collision detection, SPH, ballistics, terrain).
 * 
 * SIMD Levels (in order of preference):
 * 1. SIMD_LEVEL_AVX512 - Intel AVX-512 (up to 16 floats per vector)
 * 2. SIMD_LEVEL_AVX2   - Intel AVX2 (up to 8 floats per vector) - preferred
 * 3. SIMD_LEVEL_SSE2   - Intel SSE2 (up to 4 floats per vector)
 * 4. SIMD_LEVEL_SCALAR - Pure scalar fallback
 */

// ============================================================================
// SIMD LEVEL DETECTION
// ============================================================================

#if defined(__AVX512F__) && defined(__AVX512CD__)
    #define SIMD_LEVEL_AVX512 1
    #define SIMD_BATCH_SIZE 16  // 16 floats per __m512
    #define SIMD_BATCH_NAME "AVX-512"
#elif defined(__AVX2__) && defined(__FMA__)
    #define SIMD_LEVEL_AVX2 1
    #define SIMD_BATCH_SIZE 8   // 8 floats per __m256
    #define SIMD_BATCH_NAME "AVX2"
#elif defined(__SSE2__)
    #define SIMD_LEVEL_SSE2 1
    #define SIMD_BATCH_SIZE 4   // 4 floats per __m128
    #define SIMD_BATCH_NAME "SSE2"
#else
    #define SIMD_LEVEL_SCALAR 1
    #define SIMD_BATCH_SIZE 1
    #define SIMD_BATCH_NAME "Scalar"
#endif

// ============================================================================
// FEATURE AVAILABILITY MACROS
// ============================================================================

#ifdef SIMD_LEVEL_AVX512
    #define SIMD_ENABLED_CONSTRAINT_SOLVER   1
    #define SIMD_ENABLED_COLLISION_SUPPORT   1
    #define SIMD_ENABLED_TERRAIN_QUERY       1
    #define SIMD_ENABLED_BALLISTICS_BATCH    1
    #define SIMD_ENABLED_SPH_KERNEL          1
#elif defined(SIMD_LEVEL_AVX2)
    #define SIMD_ENABLED_CONSTRAINT_SOLVER   1
    #define SIMD_ENABLED_COLLISION_SUPPORT   1
    #define SIMD_ENABLED_TERRAIN_QUERY       1
    #define SIMD_ENABLED_BALLISTICS_BATCH    1
    #define SIMD_ENABLED_SPH_KERNEL          1
#elif defined(SIMD_LEVEL_SSE2)
    #define SIMD_ENABLED_CONSTRAINT_SOLVER   0  // Reduced batch size
    #define SIMD_ENABLED_COLLISION_SUPPORT   1
    #define SIMD_ENABLED_TERRAIN_QUERY       0
    #define SIMD_ENABLED_BALLISTICS_BATCH    0
    #define SIMD_ENABLED_SPH_KERNEL          1
#else
    #define SIMD_ENABLED_CONSTRAINT_SOLVER   0
    #define SIMD_ENABLED_COLLISION_SUPPORT   0
    #define SIMD_ENABLED_TERRAIN_QUERY       0
    #define SIMD_ENABLED_BALLISTICS_BATCH    0
    #define SIMD_ENABLED_SPH_KERNEL          0
#endif

// ============================================================================
// COMPILER HELPERS
// ============================================================================

/**
 * @def SIMD_ALWAYS_INLINE
 * Force inline for performance-critical SIMD functions
 */
#if defined(_MSC_VER)
    #define SIMD_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define SIMD_ALWAYS_INLINE inline __attribute__((always_inline))
#else
    #define SIMD_ALWAYS_INLINE inline
#endif

/**
 * @def SIMD_ALIGNED
 * Cache-line alignment for SIMD-friendly data structures
 */
#define SIMD_ALIGNED alignas(64)

/**
 * @def SIMD_ALIGNED_STACK
 * Stack alignment for temporary SIMD arrays
 */
#define SIMD_ALIGNED_STACK alignas(32)

// ============================================================================
// COMMON SIMD UTILITIES (inline for header-only use)
// ============================================================================

#include <immintrin.h>
#include <cstring>

namespace physics_core {
namespace simd {

/**
 * @brief Horizontal sum for __m256 (all 8 floats)
 * Reduces 8 float values to a single sum
 */
SIMD_ALWAYS_INLINE float horizontal_sum_m256(__m256 v) {
#ifdef SIMD_LEVEL_AVX2
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
#else
    float result = 0.0f;
    alignas(32) float values[8];
    _mm256_storeu_ps(values, v);
    for (int i = 0; i < 8; ++i) result += values[i];
    return result;
#endif
}

/**
 * @brief Horizontal max for __m256 (all 8 floats)
 * Finds maximum of 8 float values and returns its index
 */
SIMD_ALWAYS_INLINE int horizontal_max_index_m256(__m256 v) {
#ifdef SIMD_LEVEL_AVX2
    alignas(32) float values[8];
    _mm256_storeu_ps(values, v);
    int max_idx = 0;
    for (int i = 1; i < 8; ++i) {
        if (values[i] > values[max_idx]) max_idx = i;
    }
    return max_idx;
#else
    alignas(32) float values[8];
    _mm256_storeu_ps(values, v);
    int max_idx = 0;
    for (int i = 1; i < 8; ++i) {
        if (values[i] > values[max_idx]) max_idx = i;
    }
    return max_idx;
#endif
}

/**
 * @brief Blend two __m256 values based on mask (SIMD version of conditional select)
 */
SIMD_ALWAYS_INLINE __m256 blendv_ps(__m256 a, __m256 b, __m256 mask) {
#ifdef SIMD_LEVEL_AVX2
    return _mm256_blendv_ps(a, b, mask);
#else
    alignas(32) float a_vals[8], b_vals[8], mask_vals[8];
    _mm256_storeu_ps(a_vals, a);
    _mm256_storeu_ps(b_vals, b);
    _mm256_storeu_ps(mask_vals, mask);
    
    alignas(32) float result[8];
    for (int i = 0; i < 8; ++i) {
        result[i] = (mask_vals[i] != 0.0f) ? b_vals[i] : a_vals[i];
    }
    return _mm256_loadu_ps(result);
#endif
}

} // namespace simd
} // namespace physics_core
