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

/**
 * AI Core Module
 * 
 * Comprehensive AI subsystem featuring:
 * - Universal decision engine interface (IDecisionEngine)
 * - Control interception protocol (AI ↔ Player switching)
 * - Dynamic reaction lag simulation
 * - Context caching and snapshot system
 * - Integration with frame time budeting
 * 
 * Phase 1: Core interfaces and stub implementation
 * Phase 2: Dynamic modifiers and world state
 * Phase 3: Tactical formations, suppression, and communication
 */

// Core decision engine interfaces and types
#include "decision_engine.h"

// Control context and priority management
#include "control_context.h"

// Configuration and parameters
#include "ai_config.h"

// Micro-level control, stance and neural hooks (Phase 4)
#include "micro_network.h"

// Dynamic modifiers system (Phase 2)
#include "dynamic_modifiers.h"

// Formation system (Phase 3)
#include "formation_system.h"

// Suppression system (Phase 3)
#include "suppression_system.h"

// Communication system (Phase 3)
#include "communication_system.h"

// Tactical network (Phase 3)
#include "tactical_network.h"

// Reference implementations
#include "decision_engine_stub.h"

// Behavior tree system (existing)
#include "behavior_tree.h"

// Pathfinding (in development)
#include "pathfinding.h"