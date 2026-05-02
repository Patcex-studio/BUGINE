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
#include <ui/ui_context.h>
#include <widgets/unit_list.h>
#include <widgets/property_grid.h>
#include <widgets/graph_2d.h>
#include <widgets/console.h>
#include <widgets/minimap.h>
#include <widgets/theme_manager.h>
#include <vector>
#include <cmath>

int main() {
    // Initialize UI (placeholder, assume UIContext is set up)
    ui::UIContext ui_ctx;
    // ui_ctx.Initialize(...);

    ui::ThemeManager::Load("dark");

    std::vector<ui::UnitEntry> units = {
        {1, "T-34", "Tank", 82},
        {2, "Sherman", "Tank", 65},
        {3, "BMP-1", "APC", 94}
    };

    ui::Console console([](const std::string& cmd) {
        // Place command handling here
    });
    console.AddLog("Welcome to widget sandbox", IM_COL32(180, 255, 180, 255));

    ui::UnitList unit_list(units,
        [&](uint64_t id) { console.AddLog("Double-clicked unit " + std::to_string(id), IM_COL32(255, 220, 120, 255)); },
        [&](uint64_t id) { console.AddLog("Context menu for " + std::to_string(id), IM_COL32(200, 200, 255, 255)); });

    std::vector<ui::Property> props = {
        {"Health", 82, false, 0.0f, 100.0f, [&](const ui::PropertyValue& v) { console.AddLog("Health changed", IM_COL32(180, 255, 180, 255)); }},
        {"Speed", 32.0f, false, 0.0f, 200.0f, [&](const ui::PropertyValue& v) { console.AddLog("Speed changed", IM_COL32(180, 255, 180, 255)); }},
        {"Pilot", std::string("Ivan"), false, 0.0f, 0.0f, [&](const ui::PropertyValue& v) { console.AddLog("Pilot name changed", IM_COL32(180, 255, 180, 255)); }},
        {"Active", true, false, 0.0f, 0.0f, [&](const ui::PropertyValue& v) { console.AddLog("Active toggled", IM_COL32(180, 255, 180, 255)); }}
    };
    ui::PropertyGrid prop_grid(props);

    std::vector<float> graph_data(128);
    for (size_t i = 0; i < graph_data.size(); ++i) {
        graph_data[i] = 50.0f + 40.0f * std::sin(static_cast<float>(i) * 0.15f);
    }
    ui::Graph2D graph(graph_data, "FPS", 0.0f, 100.0f, ImVec2(0, 120));

    std::vector<ui::Marker> markers = {
        {100.0f, 200.0f, ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 6.0f},
        {250.0f, 420.0f, ImVec4(0.2f, 1.0f, 0.2f, 1.0f), 5.0f},
        {400.0f, 100.0f, ImVec4(0.2f, 0.5f, 1.0f, 1.0f), 5.0f}
    };
    ui::Minimap minimap(nullptr, markers, ImVec2(0, 0), ImVec2(500, 500), ImVec2(320, 240)); // Placeholder texture

    // In render loop
    while (true) {
        // ui_ctx.BeginFrame();
        ui::ThemeManager::Apply();

        ui::WidgetContext ctx;
        unit_list.Draw(ctx);
        prop_grid.Draw(ctx);
        graph.Draw(ctx);
        console.Draw(ctx);
        minimap.Draw(ctx);

        // ui_ctx.EndFrame();
        // ui_ctx.RenderUI(cmd);
    }

    return 0;
}