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

#include "physics_core/damage_system.h"

namespace medical_system {

// Treatment and medical intervention module
class TreatmentSystem {
public:
    // Apply first aid
    static bool apply_first_aid(physics_core::CrewDamageState& crew, float medic_skill, float dt);

    // Stabilize wounded crew member
    static bool stabilize(physics_core::CrewDamageState& crew, float medic_skill);

    // Request evacuation
    static bool request_evacuation(const physics_core::CrewDamageState& crew);
};

} // namespace medical_system