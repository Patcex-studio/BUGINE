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
#include "physics_core/lbm_fluid_system.h"
#include <algorithm>
#include <cmath>

namespace physics_core {

// Направления D3Q19
const Vec3 LBMFluidSystem::e_[19] = {
    {0, 0, 0},    // 0
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},  // 1-6
    {1, 1, 0}, {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0},  // 7-10
    {1, 0, 1}, {1, 0, -1}, {-1, 0, 1}, {-1, 0, -1},  // 11-14
    {0, 1, 1}, {0, 1, -1}, {0, -1, 1}, {0, -1, -1}   // 15-18
};

// Веса
const float LBMFluidSystem::w_[19] = {
    1.0f/3.0f,  // 0
    1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f, 1.0f/18.0f,  // 1-6
    1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f,  // 7-10
    1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f,  // 11-14
    1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f, 1.0f/36.0f   // 15-18
};

LBMFluidSystem::LBMFluidSystem(int grid_size_x, int grid_size_y, int grid_size_z)
    : grid_size_x_(grid_size_x), grid_size_y_(grid_size_y), grid_size_z_(grid_size_z),
      total_cells_(grid_size_x * grid_size_y * grid_size_z), tau_(0.8f) {

    for (int i = 0; i < Q; ++i) {
        f_[i].resize(total_cells_, 0.0f);
        f_next_[i].resize(total_cells_, 0.0f);
    }

    density_.resize(total_cells_, 1.0f);
    velocity_.resize(total_cells_, Vec3(0, 0, 0));

    // Инициализация равновесным распределением
    for (int idx = 0; idx < total_cells_; ++idx) {
        for (int i = 0; i < Q; ++i) {
            f_[i][idx] = w_[i] * density_[idx];
        }
    }
}

LBMFluidSystem::~LBMFluidSystem() = default;

void LBMFluidSystem::update(float dt) {
    stream();
    collide();
    compute_macros();
}

void LBMFluidSystem::stream() {
    // Простая реализация stream (без оптимизаций)
    for (int z = 0; z < grid_size_z_; ++z) {
        for (int y = 0; y < grid_size_y_; ++y) {
            for (int x = 0; x < grid_size_x_; ++x) {
                int idx = get_index(x, y, z);
                for (int i = 0; i < Q; ++i) {
                    int nx = x + static_cast<int>(e_[i].x);
                    int ny = y + static_cast<int>(e_[i].y);
                    int nz = z + static_cast<int>(e_[i].z);

                    if (is_valid(nx, ny, nz)) {
                        int nidx = get_index(nx, ny, nz);
                        f_next_[i][nidx] = f_[i][idx];
                    } else {
                        // Bounce-back boundary
                        f_next_[i][idx] = f_[i][idx];
                    }
                }
            }
        }
    }

    // Swap buffers
    for (int i = 0; i < Q; ++i) {
        std::swap(f_[i], f_next_[i]);
    }
}

void LBMFluidSystem::collide() {
    for (int idx = 0; idx < total_cells_; ++idx) {
        // Вычисление плотности и скорости
        float rho = 0.0f;
        Vec3 u(0, 0, 0);
        for (int i = 0; i < Q; ++i) {
            rho += f_[i][idx];
            u += e_[i] * f_[i][idx];
        }
        if (rho > 0.0f) u /= rho;

        // BGK collision
        for (int i = 0; i < Q; ++i) {
            float ei_dot_u = e_[i].dot(u);
            float u_sq = u.dot(u);
            float f_eq = w_[i] * rho * (1.0f + 3.0f * ei_dot_u + 4.5f * ei_dot_u * ei_dot_u - 1.5f * u_sq);

            f_[i][idx] -= (1.0f / tau_) * (f_[i][idx] - f_eq);
        }
    }
}

void LBMFluidSystem::compute_macros() {
    for (int idx = 0; idx < total_cells_; ++idx) {
        density_[idx] = 0.0f;
        velocity_[idx] = Vec3(0, 0, 0);

        for (int i = 0; i < Q; ++i) {
            density_[idx] += f_[i][idx];
            velocity_[idx] += e_[i] * f_[i][idx];
        }

        if (density_[idx] > 0.0f) {
            velocity_[idx] /= density_[idx];
        }
    }
}

void LBMFluidSystem::add_emitter(const Vec3& position, float strength) {
    // Преобразование позиции в индексы сетки
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.y);
    int z = static_cast<int>(position.z);

    if (is_valid(x, y, z)) {
        int idx = get_index(x, y, z);
        // Увеличение плотности
        for (int i = 0; i < Q; ++i) {
            f_[i][idx] += w_[i] * strength;
        }
    }
}

int LBMFluidSystem::get_index(int x, int y, int z) const {
    return z * grid_size_y_ * grid_size_x_ + y * grid_size_x_ + x;
}

bool LBMFluidSystem::is_valid(int x, int y, int z) const {
    return x >= 0 && x < grid_size_x_ && y >= 0 && y < grid_size_y_ && z >= 0 && z < grid_size_z_;
}

} // namespace physics_core