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
#include <vector>
#include <memory>

namespace physics_core {

class SDFCollisionShape {
public:
    SDFCollisionShape();
    ~SDFCollisionShape();

    // Инициализация SDF из воксельной сетки
    void initialize(const std::vector<float>& sdf_data, const Vec3& voxel_size, const Vec3& grid_offset, int resolution);

    // Выборка расстояния в точке (трилинейная интерполяция)
    float sample_distance(const Vec3& world_pos) const;

    // Вычисление градиента (нормали поверхности)
    Vec3 compute_gradient(const Vec3& world_pos) const;

    // Проверка, является ли точка внутри поверхности
    bool is_inside(const Vec3& world_pos) const { return sample_distance(world_pos) < 0.0f; }

private:
    std::vector<float> sdf_data_;
    Vec3 voxel_size_;
    Vec3 grid_offset_;
    int resolution_;
    Vec3 grid_size_; // voxel_size * resolution

    // Вспомогательные функции
    Vec3 world_to_voxel(const Vec3& world_pos) const;
    float trilinear_sample(const Vec3& voxel_pos) const;
};

} // namespace physics_core