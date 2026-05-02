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
#include "physics_core/local_coordinate_frame.h"
#include <cmath>

namespace physics_core {

LocalCoordinateFrame::LocalCoordinateFrame(
    EntityID id,
    const Vec3& position,
    const Mat3x3& orientation,
    double scale
) : id_(id),
    position_(position),
    orientation_(orientation),
    scale_(scale),
    transform_cache_valid_(false),
    cache_hits_(0),
    cache_misses_(0) {
}

LocalCoordinateFrame::~LocalCoordinateFrame() = default;

// ========== Setters ==========

void LocalCoordinateFrame::set_position(const Vec3& position) {
    if (!(position_.x == position.x && position_.y == position.y && position_.z == position.z)) {
        position_ = position;
        invalidate_transform_cache();
    }
}

void LocalCoordinateFrame::set_orientation(const Mat3x3& orientation) {
    orientation_ = orientation;
    invalidate_transform_cache();
}

void LocalCoordinateFrame::set_scale(double scale) {
    if (scale_ != scale) {
        scale_ = scale;
        invalidate_transform_cache();
    }
}

// ========== Transformation Matrices ==========

const Mat4x4& LocalCoordinateFrame::get_local_to_world_transform() const {
    if (!transform_cache_valid_) {
        cache_misses_++;
        local_to_world_transform_ = compute_local_to_world_transform();
        world_to_local_transform_ = compute_world_to_local_transform();
        transform_cache_valid_ = true;
    } else {
        cache_hits_++;
    }
    return local_to_world_transform_;
}

const Mat4x4& LocalCoordinateFrame::get_world_to_local_transform() const {
    if (!transform_cache_valid_) {
        cache_misses_++;
        local_to_world_transform_ = compute_local_to_world_transform();
        world_to_local_transform_ = compute_world_to_local_transform();
        transform_cache_valid_ = true;
    } else {
        cache_hits_++;
    }
    return world_to_local_transform_;
}

void LocalCoordinateFrame::invalidate_transform_cache() {
    transform_cache_valid_ = false;
}

// ========== Transformation Operations ==========

Vec3 LocalCoordinateFrame::transform_point_to_world(const Vec3& local_point) const {
    return get_local_to_world_transform().transform_point(local_point);
}

Vec3 LocalCoordinateFrame::transform_point_to_local(const Vec3& world_point) const {
    return get_world_to_local_transform().transform_point(world_point);
}

Vec3 LocalCoordinateFrame::transform_vector_to_world(const Vec3& local_vector) const {
    return get_local_to_world_transform().transform_vector(local_vector);
}

Vec3 LocalCoordinateFrame::transform_vector_to_local(const Vec3& world_vector) const {
    return get_world_to_local_transform().transform_vector(world_vector);
}

// ========== Private: Matrix Computation ==========

Mat4x4 LocalCoordinateFrame::compute_local_to_world_transform() const {
    Mat4x4 result = Mat4x4::identity();
    
    // Apply rotation (orientation matrix) with scale
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result(i, j) = orientation_(i, j) * scale_;
        }
    }
    
    // Apply translation
    result(0, 3) = position_.x;
    result(1, 3) = position_.y;
    result(2, 3) = position_.z;
    
    return result;
}

Mat4x4 LocalCoordinateFrame::compute_world_to_local_transform() const {
    Mat4x4 local_to_world = compute_local_to_world_transform();
    return invert_4x4(local_to_world);
}

/**
 * Helper: Compute inverse of 3x3 matrix (for rotation inverse = transpose)
 */
Mat3x3 LocalCoordinateFrame::invert_3x3(const Mat3x3& m) const {
    // For rotation matrices (orthogonal), inverse = transpose
    // But for general matrices, compute the proper inverse
    
    double det = m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) -
                 m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
                 m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
    
    if (std::abs(det) < 1e-15) {
        return Mat3x3::identity();  // Fallback for singular matrix
    }
    
    Mat3x3 inv;
    inv(0, 0) = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) / det;
    inv(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) / det;
    inv(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) / det;
    inv(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) / det;
    inv(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) / det;
    inv(1, 2) = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) / det;
    inv(2, 0) = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) / det;
    inv(2, 1) = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) / det;
    inv(2, 2) = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) / det;
    
    return inv;
}

/**
 * Helper: Compute inverse of 4x4 matrix using Gaussian elimination
 */
Mat4x4 LocalCoordinateFrame::invert_4x4(const Mat4x4& m) const {
    Mat4x4 inv;
    
    inv(0, 0) = m(1, 1)*m(2, 2)*m(3, 3) - m(1, 1)*m(2, 3)*m(3, 2) - m(2, 1)*m(1, 2)*m(3, 3) +
                m(2, 1)*m(1, 3)*m(3, 2) + m(3, 1)*m(1, 2)*m(2, 3) - m(3, 1)*m(1, 3)*m(2, 2);
    
    inv(1, 0) = -m(1, 0)*m(2, 2)*m(3, 3) + m(1, 0)*m(2, 3)*m(3, 2) + m(2, 0)*m(1, 2)*m(3, 3) -
                 m(2, 0)*m(1, 3)*m(3, 2) - m(3, 0)*m(1, 2)*m(2, 3) + m(3, 0)*m(1, 3)*m(2, 2);
    
    inv(2, 0) = m(1, 0)*m(2, 1)*m(3, 3) - m(1, 0)*m(2, 3)*m(3, 1) - m(2, 0)*m(1, 1)*m(3, 3) +
                m(2, 0)*m(1, 3)*m(3, 1) + m(3, 0)*m(1, 1)*m(2, 3) - m(3, 0)*m(1, 3)*m(2, 1);
    
    inv(3, 0) = -m(1, 0)*m(2, 1)*m(3, 2) + m(1, 0)*m(2, 2)*m(3, 1) + m(2, 0)*m(1, 1)*m(3, 2) -
                 m(2, 0)*m(1, 2)*m(3, 1) - m(3, 0)*m(1, 1)*m(2, 2) + m(3, 0)*m(1, 2)*m(2, 1);
    
    inv(0, 1) = -m(0, 1)*m(2, 2)*m(3, 3) + m(0, 1)*m(2, 3)*m(3, 2) + m(2, 1)*m(0, 2)*m(3, 3) -
                 m(2, 1)*m(0, 3)*m(3, 2) - m(3, 1)*m(0, 2)*m(2, 3) + m(3, 1)*m(0, 3)*m(2, 2);
    
    inv(1, 1) = m(0, 0)*m(2, 2)*m(3, 3) - m(0, 0)*m(2, 3)*m(3, 2) - m(2, 0)*m(0, 2)*m(3, 3) +
                m(2, 0)*m(0, 3)*m(3, 2) + m(3, 0)*m(0, 2)*m(2, 3) - m(3, 0)*m(0, 3)*m(2, 2);
    
    inv(2, 1) = -m(0, 0)*m(2, 1)*m(3, 3) + m(0, 0)*m(2, 3)*m(3, 1) + m(2, 0)*m(0, 1)*m(3, 3) -
                 m(2, 0)*m(0, 3)*m(3, 1) - m(3, 0)*m(0, 1)*m(2, 3) + m(3, 0)*m(0, 3)*m(2, 1);
    
    inv(3, 1) = m(0, 0)*m(2, 1)*m(3, 2) - m(0, 0)*m(2, 2)*m(3, 1) - m(2, 0)*m(0, 1)*m(3, 2) +
                m(2, 0)*m(0, 2)*m(3, 1) + m(3, 0)*m(0, 1)*m(2, 2) - m(3, 0)*m(0, 2)*m(2, 1);
    
    inv(0, 2) = m(0, 1)*m(1, 2)*m(3, 3) - m(0, 1)*m(1, 3)*m(3, 2) - m(1, 1)*m(0, 2)*m(3, 3) +
                m(1, 1)*m(0, 3)*m(3, 2) + m(3, 1)*m(0, 2)*m(1, 3) - m(3, 1)*m(0, 3)*m(1, 2);
    
    inv(1, 2) = -m(0, 0)*m(1, 2)*m(3, 3) + m(0, 0)*m(1, 3)*m(3, 2) + m(1, 0)*m(0, 2)*m(3, 3) -
                 m(1, 0)*m(0, 3)*m(3, 2) - m(3, 0)*m(0, 2)*m(1, 3) + m(3, 0)*m(0, 3)*m(1, 2);
    
    inv(2, 2) = m(0, 0)*m(1, 1)*m(3, 3) - m(0, 0)*m(1, 3)*m(3, 1) - m(1, 0)*m(0, 1)*m(3, 3) +
                m(1, 0)*m(0, 3)*m(3, 1) + m(3, 0)*m(0, 1)*m(1, 3) - m(3, 0)*m(0, 3)*m(1, 1);
    
    inv(3, 2) = -m(0, 0)*m(1, 1)*m(3, 2) + m(0, 0)*m(1, 2)*m(3, 1) + m(1, 0)*m(0, 1)*m(3, 2) -
                 m(1, 0)*m(0, 2)*m(3, 1) - m(3, 0)*m(0, 1)*m(1, 2) + m(3, 0)*m(0, 2)*m(1, 1);
    
    inv(0, 3) = -m(0, 1)*m(1, 2)*m(2, 3) + m(0, 1)*m(1, 3)*m(2, 2) + m(1, 1)*m(0, 2)*m(2, 3) -
                 m(1, 1)*m(0, 3)*m(2, 2) - m(2, 1)*m(0, 2)*m(1, 3) + m(2, 1)*m(0, 3)*m(1, 2);
    
    inv(1, 3) = m(0, 0)*m(1, 2)*m(2, 3) - m(0, 0)*m(1, 3)*m(2, 2) - m(1, 0)*m(0, 2)*m(2, 3) +
                m(1, 0)*m(0, 3)*m(2, 2) + m(2, 0)*m(0, 2)*m(1, 3) - m(2, 0)*m(0, 3)*m(1, 2);
    
    inv(2, 3) = -m(0, 0)*m(1, 1)*m(2, 3) + m(0, 0)*m(1, 3)*m(2, 1) + m(1, 0)*m(0, 1)*m(2, 3) -
                 m(1, 0)*m(0, 3)*m(2, 1) - m(2, 0)*m(0, 1)*m(1, 3) + m(2, 0)*m(0, 3)*m(1, 1);
    
    inv(3, 3) = m(0, 0)*m(1, 1)*m(2, 2) - m(0, 0)*m(1, 2)*m(2, 1) - m(1, 0)*m(0, 1)*m(2, 2) +
                m(1, 0)*m(0, 2)*m(2, 1) + m(2, 0)*m(0, 1)*m(1, 2) - m(2, 0)*m(0, 2)*m(1, 1);
    
    double det = m(0, 0)*inv(0, 0) + m(0, 1)*inv(1, 0) + m(0, 2)*inv(2, 0) + m(0, 3)*inv(3, 0);
    
    if (std::abs(det) < 1e-15) {
        return Mat4x4::identity();  // Fallback for singular matrix
    }
    
    for (int i = 0; i < 16; ++i) {
        inv.data[i] /= det;
    }
    
    return inv;
}

}  // namespace physics_core
