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

// Main header for Physics Core Library
// Include this to get all public APIs

#include "types.h"
#include "world_space_manager.h"
#include "local_coordinate_frame.h"
#include "physics_body.h"
#include "integrators.h"
#include "simd_processor.h"
#include "matter_systems.h"
#include "physics_thread_pool.h"
#include "physics_core.h"
#include "physics_core_c_api.h"
#include "flight_dynamics.h"
#include "hybrid_precision.h"
#include "armor_materials.h"
#include "projectile_properties.h"
#include "vehicle_component.h"
#include "ballistics_system.h"
#include "damage_system.h"
#include "secondary_effects_system.h"
#include "crew_damage_system.h"
#include "terrain_system.h"
#include "destruction_system.h"
#include "environment_system.h"
#include "repair_system.h"

namespace physics_core {

// Library version
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 1;  // Incremented for damage system update
constexpr int VERSION_PATCH = 0;

}  // namespace physics_core
