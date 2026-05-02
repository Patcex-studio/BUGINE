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
#include <vector>
#include <string>
#include <functional>
#include <imgui.h>

namespace ui {

struct LogEntry {
    std::string message;
    ImU32 color = IM_COL32(255, 255, 255, 255);
};

class Console : public IWidget {
public:
    static constexpr size_t kMaxLogEntries = 200;

    Console(std::function<void(const std::string&)> onCommand = nullptr);

    void Draw(const WidgetContext& ctx) override;
    void AddLog(const std::string& message, ImU32 color = IM_COL32(255, 255, 255, 255));

private:
    std::vector<LogEntry> logs_;
    std::string input_;
    std::function<void(const std::string&)> on_command_;
};

} // namespace ui