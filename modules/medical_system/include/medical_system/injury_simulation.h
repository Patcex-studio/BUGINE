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

#include <vector>
#include "physics_core/damage_system.h"

namespace medical_system {

// Injury simulation module
class InjurySimulation {
public:
    // Calculate injuries from spall fragments
    static std::vector<physics_core::Injury> calculate_injuries_from_spall(
        const physics_core::Fragment& frag, 
        const physics_core::CrewDamageState& crew,
        uint64_t frame_id
    );

    // Calculate injuries from blast
    static std::vector<physics_core::Injury> calculate_injuries_from_blast(
        float pressure, 
        float impulse, 
        const physics_core::CrewDamageState& crew,
        uint64_t frame_id
    );

    // Calculate injuries from fire
    static std::vector<physics_core::Injury> calculate_injuries_from_fire(
        float temperature, 
        float duration, 
        const physics_core::CrewDamageState& crew,
        uint64_t frame_id
    );
};

} // namespace medical_system