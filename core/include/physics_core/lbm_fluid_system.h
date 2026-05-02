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

namespace physics_core {

// Модель D3Q19 для LBM
class LBMFluidSystem {
public:
    LBMFluidSystem(int grid_size_x, int grid_size_y, int grid_size_z);
    ~LBMFluidSystem();

    void update(float dt);

    // Доступ к данным для рендеринга
    const std::vector<float>& get_density() const { return density_; }
    const std::vector<Vec3>& get_velocity() const { return velocity_; }

    // Добавление эмиттера
    void add_emitter(const Vec3& position, float strength);

private:
    int grid_size_x_, grid_size_y_, grid_size_z_;
    int total_cells_;

    // Функции распределения (двойная буферизация)
    std::vector<float> f_[19];  // Текущий
    std::vector<float> f_next_[19];  // Следующий

    // Макроскопические величины
    std::vector<float> density_;
    std::vector<Vec3> velocity_;

    float tau_;  // Время релаксации

    // Направления D3Q19
    static constexpr int Q = 19;
    static const Vec3 e_[Q];
    static const float w_[Q];  // Веса

    void stream();
    void collide();
    void compute_macros();

    int get_index(int x, int y, int z) const;
    bool is_valid(int x, int y, int z) const;
};

} // namespace physics_core