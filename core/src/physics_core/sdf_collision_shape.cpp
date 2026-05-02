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
#include "physics_core/sdf_collision_shape.h"
#include <algorithm>
#include <cmath>

namespace physics_core {

SDFCollisionShape::SDFCollisionShape()
    : resolution_(0) {
}

SDFCollisionShape::~SDFCollisionShape() = default;

void SDFCollisionShape::initialize(const std::vector<float>& sdf_data, const Vec3& voxel_size, const Vec3& grid_offset, int resolution) {
    sdf_data_ = sdf_data;
    voxel_size_ = voxel_size;
    grid_offset_ = grid_offset;
    resolution_ = resolution;
    grid_size_ = voxel_size * static_cast<float>(resolution);
}

Vec3 SDFCollisionShape::world_to_voxel(const Vec3& world_pos) const {
    Vec3 local = world_pos - grid_offset_;
    return Vec3(local.x / voxel_size_.x,
                local.y / voxel_size_.y,
                local.z / voxel_size_.z);
}

float SDFCollisionShape::trilinear_sample(const Vec3& voxel_pos) const {
    // Получение целых координат
    int x0 = static_cast<int>(std::floor(voxel_pos.x));
    int y0 = static_cast<int>(std::floor(voxel_pos.y));
    int z0 = static_cast<int>(std::floor(voxel_pos.z));

    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;

    // Клампинг в границы
    x0 = std::clamp(x0, 0, resolution_ - 1);
    y0 = std::clamp(y0, 0, resolution_ - 1);
    z0 = std::clamp(z0, 0, resolution_ - 1);
    x1 = std::clamp(x1, 0, resolution_ - 1);
    y1 = std::clamp(y1, 0, resolution_ - 1);
    z1 = std::clamp(z1, 0, resolution_ - 1);

    // Получение значений в углах куба
    auto get_value = [&](int x, int y, int z) -> float {
        size_t index = static_cast<size_t>(z) * resolution_ * resolution_ + static_cast<size_t>(y) * resolution_ + static_cast<size_t>(x);
        return sdf_data_[index];
    };

    float c000 = get_value(x0, y0, z0);
    float c100 = get_value(x1, y0, z0);
    float c010 = get_value(x0, y1, z0);
    float c110 = get_value(x1, y1, z0);
    float c001 = get_value(x0, y0, z1);
    float c101 = get_value(x1, y0, z1);
    float c011 = get_value(x0, y1, z1);
    float c111 = get_value(x1, y1, z1);

    // Дробные части
    float dx = voxel_pos.x - std::floor(voxel_pos.x);
    float dy = voxel_pos.y - std::floor(voxel_pos.y);
    float dz = voxel_pos.z - std::floor(voxel_pos.z);

    // Трилинейная интерполяция
    float c00 = c000 * (1 - dx) + c100 * dx;
    float c10 = c010 * (1 - dx) + c110 * dx;
    float c01 = c001 * (1 - dx) + c101 * dx;
    float c11 = c011 * (1 - dx) + c111 * dx;

    float c0 = c00 * (1 - dy) + c10 * dy;
    float c1 = c01 * (1 - dy) + c11 * dy;

    return c0 * (1 - dz) + c1 * dz;
}

float SDFCollisionShape::sample_distance(const Vec3& world_pos) const {
    if (sdf_data_.empty()) return 0.0f;

    Vec3 voxel_pos = world_to_voxel(world_pos);
    return trilinear_sample(voxel_pos);
}

Vec3 SDFCollisionShape::compute_gradient(const Vec3& world_pos) const {
    const float eps = voxel_size_.x * 0.5f;

    float dx = sample_distance(world_pos + Vec3(eps, 0, 0)) - sample_distance(world_pos - Vec3(eps, 0, 0));
    float dy = sample_distance(world_pos + Vec3(0, eps, 0)) - sample_distance(world_pos - Vec3(0, eps, 0));
    float dz = sample_distance(world_pos + Vec3(0, 0, eps)) - sample_distance(world_pos - Vec3(0, 0, eps));

    Vec3 grad(dx, dy, dz);
    double len = grad.magnitude();
    return len > 1e-6 ? Vec3(grad.x / len, grad.y / len, grad.z / len) : Vec3(0, 1, 0); // Нормализация, fallback вверх
}

} // namespace physics_core