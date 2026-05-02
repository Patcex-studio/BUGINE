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

#include "ai_core/ai_config.h"
#include <unordered_map>

class ProfileManager {
public:
    // Apply profile to entity
    void ApplyProfile(EntityId entity_id, DifficultyLevel level, AIConfig& out_config) {
        out_config = AIConfig::ForDifficulty(level);
    }
    
    // Get current level for entity (placeholder)
    DifficultyLevel GetLevel(EntityId entity_id) const {
        return DifficultyLevel::Rookie;  // Default
    }
};