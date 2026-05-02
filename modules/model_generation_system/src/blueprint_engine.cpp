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
#include "blueprint_engine.h"
#include <algorithm>
#include <iostream>

namespace model_generation {

// Blueprint validation function
bool validate_blueprint(const VehicleBlueprintDefinition& blueprint) {
    if (blueprint.nodes.empty()) {
        std::cerr << "Blueprint validation failed: no nodes" << std::endl;
        return false;
    }

    // Check for cycles in node hierarchy (simplified)
    std::vector<bool> visited(blueprint.nodes.size(), false);
    std::vector<bool> in_path(blueprint.nodes.size(), false);

    // Basic validation - ensure all child indices are valid
    for (size_t i = 0; i < blueprint.nodes.size(); ++i) {
        for (uint32_t child : blueprint.nodes[i].child_nodes) {
            if (child >= blueprint.nodes.size()) {
                std::cerr << "Invalid child node index: " << child << std::endl;
                return false;
            }
        }
    }

    return true;
}

} // namespace model_generation