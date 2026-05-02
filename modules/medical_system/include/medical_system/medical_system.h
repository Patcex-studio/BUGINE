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
#include <unordered_map>

namespace medical_system {

// Main medical system coordinator
class MedicalSystem {
public:
    // Update all crew medical states
    void update(float dt, uint64_t frame_id);

    // Treat wounded crew member
    void treat_wounded(physics_core::EntityID medic, physics_core::EntityID patient, float dt);

    // Handle damage event
    void on_damage_event(const physics_core::DamageEvent& event, physics_core::CrewDamageState& crew);

    // Get crew states (for ECS integration)
    std::unordered_map<physics_core::EntityID, physics_core::CrewDamageState>& get_crew_states();

private:
    std::unordered_map<physics_core::EntityID, physics_core::CrewDamageState> crew_states_;
    uint64_t current_frame_ = 0;
};

} // namespace medical_system