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

#include <cstdint>
#include <vector>
#include <immintrin.h>

// Assuming TacticalOrder is defined; simple struct
struct TacticalOrder {
    uint32_t order_type;
    uint64_t target_entity;
    __m256 destination;
};

struct TacticalSituation {
    uint64_t commander_entity;       // Commanding unit
    std::vector<uint64_t> friendly_units; // Friendly units in area
    std::vector<uint64_t> enemy_units;   // Enemy units in area
    __m256 tactical_position;         // Current tactical position
    float visibility_range;           // Current visibility range
    float threat_level;              // Overall threat assessment
    uint32_t terrain_advantage;      // Terrain tactical advantage score
};

struct TacticalDecision {
    uint32_t decision_type;          // ATTACK, DEFEND, RETREAT, FLANK, etc.
    uint64_t primary_target;         // Primary target entity
    __m256 maneuver_destination;     // Maneuver destination
    float aggression_level;           // Aggression level (0.0-1.0)
    float risk_tolerance;            // Risk tolerance level (0.0-1.0)
    std::vector<TacticalOrder> orders; // Generated orders for subunits
};

class TacticalDecisionSystem {
public:
    TacticalDecisionSystem() = default;
    ~TacticalDecisionSystem() = default;

    TacticalDecision evaluate_situation(const TacticalSituation& situation);
    void generate_unit_orders(const TacticalDecision& decision,
                            std::vector<TacticalOrder>& orders);
};