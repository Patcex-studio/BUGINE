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

#include "i_widget.h"
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

struct UnitEntry {
    uint64_t id;
    std::string name;
    std::string type;
    uint32_t health = 100;
};

using UnitAction = std::function<void(uint64_t)>;

class UnitList : public IWidget {
public:
    UnitList(const std::vector<UnitEntry>& units,
             UnitAction onDoubleClick = nullptr,
             UnitAction onContextMenu = nullptr);

    void Draw(const WidgetContext& ctx) override;

private:
    void UpdateFilteredItems();
    std::vector<UnitEntry> units_;
    std::vector<size_t> filtered_indices_;
    std::string filter_;
    int selected_index_ = -1;
    UnitAction on_double_click_;
    UnitAction on_context_menu_;
};

} // namespace ui