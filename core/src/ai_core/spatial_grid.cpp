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
#include "ai_core/spatial_grid.h"
#include <cmath>

std::vector<EntityId> SpatialGrid::query_radius(float x, float y, float radius, std::function<std::pair<float, float>(EntityId)> get_pos) const {
    std::vector<EntityId> result;
    const int32_t cx = static_cast<int32_t>(std::floor(x / CELL_SIZE));
    const int32_t cy = static_cast<int32_t>(std::floor(y / CELL_SIZE));
    const int32_t cr = static_cast<int32_t>(std::ceil(radius / CELL_SIZE));
    
    for (int32_t dy = -cr; dy <= cr; ++dy) {
        for (int32_t dx = -cr; dx <= cr; ++dx) {
            auto it = cells.find(hash_cell(cx + dx, cy + dy));
            if (it != cells.end()) {
                // Фильтрация по точному радиусу (квадрат расстояния)
                const float r2 = radius * radius;
                for (EntityId id : it->second) {
                    auto [px, py] = get_pos(id);
                    const float dx = px - x, dy = py - y;
                    if (dx*dx + dy*dy <= r2) {
                        result.push_back(id);
                    }
                }
            }
        }
    }
    return result;
}