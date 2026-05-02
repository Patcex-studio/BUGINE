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

// Main include file for electrical system module

#include "electrical_types.h"
#include "components.h"
#include "electrical_system.h"
#include "damage_system_integration.h"
#include "network_sync.h"
#include "ai_integration.h"

namespace electrical_system {

// Re-export commonly used symbols
using ElectricalSystem_t = ElectricalSystem;
using GeneratorComponent_t = GeneratorComponent;
using ConsumerComponent_t = ConsumerComponent;
using ElectricalGridComponent_t = ElectricalGridComponent;
using AIElectricalStatus_t = AIElectricalStatus;
using DamageSystemIntegration_t = DamageSystemIntegration;
using NetworkElectricalSync_t = NetworkElectricalSync;

} // namespace electrical_system
