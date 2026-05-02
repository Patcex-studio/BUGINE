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
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <span>
#include <functional>

using EntityId = uint64_t;

using CellHash = uint64_t;
static constexpr float CELL_SIZE = 5.0f;

class SpatialGrid {
    std::unordered_map<CellHash, std::vector<EntityId>> cells;
public:
    static CellHash hash_cell(float x, float y) noexcept {
        const int32_t cx = static_cast<int32_t>(std::floor(x / CELL_SIZE));
        const int32_t cy = static_cast<int32_t>(std::floor(y / CELL_SIZE));
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | 
                static_cast<uint32_t>(cy);
    }
    void clear() noexcept { cells.clear(); }
    void insert(EntityId id, float x, float y) noexcept {
        cells[hash_cell(x, y)].push_back(id);
    }
    // найти всех в радиусе, включая соседние клетки
    std::vector<EntityId> query_radius(float x, float y, float radius, std::function<std::pair<float, float>(EntityId)> get_pos) const;
    // найти всех в том же cell
    const std::vector<EntityId>& query_cell(CellHash hash) const {
        static const std::vector<EntityId> empty;
        auto it = cells.find(hash);
        return it != cells.end() ? it->second : empty;
    }
};