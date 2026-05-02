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

#include "types.h"
#include <chrono>

namespace physics_core {

// ============================================================================
// Local Coordinate Frame (LCS)
// ============================================================================

/**
 * @class LocalCoordinateFrame
 * @brief Represents a local coordinate system with position, orientation, and scale
 * 
 * Criteria:
 * - Support up to 10 levels of hierarchy nesting
 * - Transformation computation < 50 ns
 * - Dynamic hierarchy change support
 */
class LocalCoordinateFrame {
public:
    LocalCoordinateFrame(EntityID id, const Vec3& position, const Mat3x3& orientation, double scale);
    ~LocalCoordinateFrame();

    // ========== Getters ==========

    EntityID get_id() const { return id_; }
    
    Vec3 get_position() const { return position_; }
    const Mat3x3& get_orientation() const { return orientation_; }
    double get_scale() const { return scale_; }

    // ========== Setters ==========

    void set_position(const Vec3& position);
    void set_orientation(const Mat3x3& orientation);
    void set_scale(double scale);

    // ========== Transformation Matrices ==========

    /**
     * Get local-to-world transformation matrix (cached)
     * Computation time: typically < 50 ns
     */
    const Mat4x4& get_local_to_world_transform() const;

    /**
     * Get world-to-local transformation matrix (cached)
     * Computed from inverse of local_to_world
     */
    const Mat4x4& get_world_to_local_transform() const;

    /**
     * Manually invalidate transform cache
     * Called when parent frame changes
     */
    void invalidate_transform_cache();

    // ========== Transformation Operations ==========

    /**
     * Transform point from local to world space quickly
     */
    Vec3 transform_point_to_world(const Vec3& local_point) const;

    /**
     * Transform point from world to local space quickly
     */
    Vec3 transform_point_to_local(const Vec3& world_point) const;

    /**
     * Transform vector (orientation only) to world space
     */
    Vec3 transform_vector_to_world(const Vec3& local_vector) const;

    /**
     * Transform vector (orientation only) to local space
     */
    Vec3 transform_vector_to_local(const Vec3& world_vector) const;

    // ========== Statistics ==========

    uint64_t get_transform_cache_hits() const { return cache_hits_; }
    uint64_t get_transform_cache_misses() const { return cache_misses_; }

private:
    EntityID id_;
    Vec3 position_;
    Mat3x3 orientation_;
    double scale_;

    // Cached transformation matrices
    mutable Mat4x4 local_to_world_transform_;
    mutable Mat4x4 world_to_local_transform_;
    mutable bool transform_cache_valid_;

    // Cache statistics
    mutable uint64_t cache_hits_;
    mutable uint64_t cache_misses_;

    // Computation methods
    Mat4x4 compute_local_to_world_transform() const;
    Mat4x4 compute_world_to_local_transform() const;

    // Helper: Compute inverse of 4x4 matrix
    Mat4x4 invert_4x4(const Mat4x4& m) const;
    Mat3x3 invert_3x3(const Mat3x3& m) const;
};

}  // namespace physics_core
