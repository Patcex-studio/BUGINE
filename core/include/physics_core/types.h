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

#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <immintrin.h>
#include <array>
#include <iostream>

namespace physics_core {

// ============================================================================
// SIMD-Aligned Vector Types (Optimized for Cache)
// ============================================================================

// 32-byte aligned 3D vector for SIMD operations
struct alignas(32) Vec3 {
    double x, y, z;
    double _pad;  // Padding for 32-byte alignment

    Vec3() : x(0.0), y(0.0), z(0.0), _pad(0.0) {}
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_), _pad(0.0) {}

    Vec3 operator+(const Vec3& v) const {
        return Vec3(x + v.x, y + v.y, z + v.z);
    }

    Vec3 operator-(const Vec3& v) const {
        return Vec3(x - v.x, y - v.y, z - v.z);
    }

    Vec3 operator*(double scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    Vec3 operator/(double scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }

    Vec3& operator+=(const Vec3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vec3& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vec3& operator/=(double scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    bool operator==(const Vec3& v) const {
        const double EPSILON = 1e-10;
        return std::abs(x - v.x) < EPSILON && std::abs(y - v.y) < EPSILON && std::abs(z - v.z) < EPSILON;
    }

    bool operator!=(const Vec3& v) const {
        return !(*this == v);
    }

    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    double dot(const Vec3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    Vec3 cross(const Vec3& v) const {
        return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 normalized() const {
        double mag = magnitude();
        if (mag < 1e-9) return Vec3(0, 0, 0);
        return (*this) / mag;
    }

    void normalize() {
        double mag = magnitude();
        if (mag > 1e-9) {
            x /= mag;
            y /= mag;
            z /= mag;
        }
    }
};

// 3x3 Matrix for inertia tensor and rotations
struct Mat3x3 {
    std::array<double, 9> data;  // Row-major layout

    Mat3x3() {
        std::fill(data.begin(), data.end(), 0.0);
    }

    Mat3x3(const std::array<double, 9>& d) : data(d) {}

    // Identity matrix
    static Mat3x3 identity() {
        Mat3x3 m;
        m.data[0] = m.data[4] = m.data[8] = 1.0;
        return m;
    }

    // Access elements: m(row, col)
    double& operator()(int row, int col) {
        return data[row * 3 + col];
    }

    double operator()(int row, int col) const {
        return data[row * 3 + col];
    }

    Vec3 operator*(const Vec3& v) const {
        return Vec3(
            data[0] * v.x + data[1] * v.y + data[2] * v.z,
            data[3] * v.x + data[4] * v.y + data[5] * v.z,
            data[6] * v.x + data[7] * v.y + data[8] * v.z
        );
    }

    Mat3x3 operator*(const Mat3x3& m) const {
        Mat3x3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result(i, j) = 0;
                for (int k = 0; k < 3; ++k) {
                    result(i, j) += (*this)(i, k) * m(k, j);
                }
            }
        }
        return result;
    }
<<<<<<< HEAD
=======

    Mat3x3 operator+(const Mat3x3& m) const {
        Mat3x3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result(i, j) = (*this)(i, j) + m(i, j);
            }
        }
        return result;
    }
>>>>>>> c308d63 (Helped the rabbits find a home)
};

// 4x4 Homogeneous transformation matrix
struct alignas(64) Mat4x4 {
    std::array<double, 16> data;  // Row-major layout

    Mat4x4() {
        std::fill(data.begin(), data.end(), 0.0);
    }

    // Identity matrix
    static Mat4x4 identity() {
        Mat4x4 m;
        m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1.0;
        return m;
    }

    double& operator()(int row, int col) {
        return data[row * 4 + col];
    }

    double operator()(int row, int col) const {
        return data[row * 4 + col];
    }

    Vec3 transform_point(const Vec3& v) const {
        double w = (*this)(3, 0) * v.x + (*this)(3, 1) * v.y + (*this)(3, 2) * v.z + (*this)(3, 3);
        return Vec3(
            ((*this)(0, 0) * v.x + (*this)(0, 1) * v.y + (*this)(0, 2) * v.z + (*this)(0, 3)) / w,
            ((*this)(1, 0) * v.x + (*this)(1, 1) * v.y + (*this)(1, 2) * v.z + (*this)(1, 3)) / w,
            ((*this)(2, 0) * v.x + (*this)(2, 1) * v.y + (*this)(2, 2) * v.z + (*this)(2, 3)) / w
        );
    }

    Vec3 transform_vector(const Vec3& v) const {
        return Vec3(
            (*this)(0, 0) * v.x + (*this)(0, 1) * v.y + (*this)(0, 2) * v.z,
            (*this)(1, 0) * v.x + (*this)(1, 1) * v.y + (*this)(1, 2) * v.z,
            (*this)(2, 0) * v.x + (*this)(2, 1) * v.y + (*this)(2, 2) * v.z
        );
    }

    Mat4x4 operator*(const Mat4x4& m) const {
        Mat4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = 0;
                for (int k = 0; k < 4; ++k) {
                    result(i, j) += (*this)(i, k) * m(k, j);
                }
            }
        }
        return result;
    }
};

// ============================================================================
// Entity ID and Handle Types
// ============================================================================

using EntityID = uint64_t;

const EntityID INVALID_ENTITY_ID = 0;

// ============================================================================
// Constraint precision constants
// ============================================================================

const double COORD_EPSILON = 1e-9;  // 1 nanosecond precision in meters
const double MIN_MASS = 1e-6;
const double MAX_COORDINATE = 1e15;

}  // namespace physics_core
