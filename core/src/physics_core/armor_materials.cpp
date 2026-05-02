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
#include "armor_materials.h"

namespace physics_core {

// Predefined armor material database
const std::array<ArmorMaterial, static_cast<size_t>(ArmorType::COUNT)> armor_materials = {{
    // RHA - baseline
    {1.0f, 7850.0f, 0.1f, 60.0f, 1.0f, 1.0f, 0.0f},
    // CHA - cast homogeneous armor
    {0.95f, 7850.0f, 0.15f, 55.0f, 0.9f, 1.0f, 0.0f},
    // FHA - face-hardened armor
    {1.2f, 7850.0f, 0.08f, 65.0f, 1.1f, 1.0f, 0.0f},
    // COMPOSITE - modern composite
    {1.5f, 6500.0f, 0.05f, 75.0f, 1.3f, 3.0f, 0.8f},
    // REACTIVE - explosive reactive armor
    {2.0f, 7000.0f, 0.02f, 80.0f, 0.8f, 2.0f, 0.0f},
    // SPACED - spaced armor
    {0.8f, 7850.0f, 0.2f, 50.0f, 1.0f, 2.0f, 0.0f},
    // ALUMINUM - lightweight aluminum
    {0.3f, 2700.0f, 0.3f, 40.0f, 0.5f, 1.0f, 0.0f},
    // CERAMIC - ceramic armor
    {2.5f, 3800.0f, 0.01f, 85.0f, 1.5f, 1.0f, 1.0f},
    // STEEL - steel variants
    {1.1f, 7850.0f, 0.12f, 62.0f, 1.0f, 1.0f, 0.0f}
}};

} // namespace physics_core