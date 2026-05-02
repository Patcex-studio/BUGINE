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
#include <variant>
#include <string>
#include <vector>
#include <functional>

namespace ui {

using PropertyValue = std::variant<int, float, std::string, bool>;

struct Property {
    std::string name;
    PropertyValue value;
    bool readOnly = false;
    float min = 0.0f;
    float max = 100.0f;
    std::function<void(const PropertyValue&)> on_change;
};

class PropertyGrid : public IWidget {
public:
    PropertyGrid(const std::vector<Property>& properties);

    void Draw(const WidgetContext& ctx) override;

private:
    std::vector<Property> properties_;
};

} // namespace ui