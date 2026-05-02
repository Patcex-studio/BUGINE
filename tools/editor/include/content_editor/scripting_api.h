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
#include <string>
#include <functional>
#include "physics_core/types.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace physics_core {
class PhysicsCore;
}

namespace ai_core {
class IDecisionEngine;
}

namespace content_editor {

// Lua API binding structure
struct LuaAPIBinding {
    // Physics system access
    lua_CFunction get_entity_position;
    lua_CFunction set_entity_position;
    lua_CFunction get_entity_velocity;
    lua_CFunction apply_force_to_entity;
    lua_CFunction get_physics_property;
    lua_CFunction set_physics_property;

    // Damage system access
    lua_CFunction apply_damage_to_entity;
    lua_CFunction get_entity_health;
    lua_CFunction set_entity_health;
    lua_CFunction get_damage_type;
    lua_CFunction create_explosion;

    // AI system access
    lua_CFunction set_entity_behavior;
    lua_CFunction get_entity_target;
    lua_CFunction issue_command_to_entity;
    lua_CFunction get_ai_state;
    lua_CFunction set_ai_parameter;

    // Mission system access
    lua_CFunction spawn_unit_at_position;
    lua_CFunction destroy_entity;
    lua_CFunction get_mission_time;
    lua_CFunction set_mission_time;
    lua_CFunction trigger_event;
    lua_CFunction get_player_info;

    // Utility functions
    lua_CFunction log_message;
    lua_CFunction get_random_number;
    lua_CFunction wait_seconds;
    lua_CFunction run_script_after_delay;
    lua_CFunction create_timer;
    lua_CFunction cancel_timer;
};

// Script execution context
struct ScriptExecutionContext {
    uint32_t script_id;
    std::string script_name;
    physics_core::EntityID executing_entity;
    uint32_t mission_id;
    float execution_time;
    std::unordered_map<std::string, std::string> variables;
};

// Script execution result
struct ScriptExecutionResult {
    bool success;
    std::string error_message;
    std::string return_value;
    float execution_time_ms;
    uint32_t memory_used_kb;
};

// Timer structure for delayed execution
struct ScriptTimer {
    uint32_t timer_id;
    float delay_seconds;
    std::string script_code;
    ScriptExecutionContext context;
    bool is_repeating;
    float repeat_interval;
    uint32_t repeat_count;
};

// C# API wrapper (for .NET interop)
#ifdef _WIN32
#ifdef CONTENT_EDITOR_EXPORTS
#define CONTENT_EDITOR_API __declspec(dllexport)
#else
#define CONTENT_EDITOR_API __declspec(dllimport)
#endif
#else
#define CONTENT_EDITOR_API
#endif

extern "C" {
    // Physics access
    CONTENT_EDITOR_API void GetEntityPosition(int entityId, float* position);
    CONTENT_EDITOR_API void SetEntityPosition(int entityId, float x, float y, float z);
    CONTENT_EDITOR_API void GetEntityVelocity(int entityId, float* velocity);
    CONTENT_EDITOR_API void ApplyForceToEntity(int entityId, float fx, float fy, float fz);

    // Damage system
    CONTENT_EDITOR_API void ApplyDamageToEntity(int entityId, float damage, int damageType);
    CONTENT_EDITOR_API float GetEntityHealth(int entityId);
    CONTENT_EDITOR_API void SetEntityHealth(int entityId, float health);

    // AI system
    CONTENT_EDITOR_API void SetEntityBehavior(int entityId, const char* behaviorName);
    CONTENT_EDITOR_API void IssueCommandToEntity(int entityId, const char* command, const char** parameters, int paramCount);

    // Mission system
    CONTENT_EDITOR_API int SpawnUnitAtPosition(const char* unitTemplate, float x, float y, float z, float rx, float ry, float rz);
    CONTENT_EDITOR_API void DestroyEntity(int entityId);
    CONTENT_EDITOR_API float GetMissionTime();
    CONTENT_EDITOR_API void TriggerEvent(const char* eventName);

    // Utility
    CONTENT_EDITOR_API void LogMessage(const char* message);
    CONTENT_EDITOR_API float GetRandomNumber(float min, float max);
    CONTENT_EDITOR_API void WaitForSeconds(float seconds);
}

// Managed wrapper class for C# interop
class ManagedScriptingAPI {
public:
    static void InitializeSystems(physics_core::PhysicsCore* physics, ai_core::IDecisionEngine* ai, MissionDesigner* mission);
    
    // Physics access
    static physics_core::Vec3 GetEntityPosition(int entityId);
    static void SetEntityPosition(int entityId, physics_core::Vec3 position);
    static physics_core::Vec3 GetEntityVelocity(int entityId);
    static void ApplyForceToEntity(int entityId, physics_core::Vec3 force);

    // Damage system
    static void ApplyDamageToEntity(int entityId, float damage, int damageType);
    static float GetEntityHealth(int entityId);
    static void SetEntityHealth(int entityId, float health);

    // AI system
    static void SetEntityBehavior(int entityId, const std::string& behaviorName);
    static void IssueCommandToEntity(int entityId, const std::string& command, const std::vector<std::string>& parameters);

    // Mission system
    static int SpawnUnitAtPosition(const std::string& unitTemplate, physics_core::Vec3 position, physics_core::Vec3 rotation);
    static void DestroyEntity(int entityId);
    static float GetMissionTime();
    static void TriggerEvent(const std::string& eventName);

    // Utility
    static void LogMessage(const std::string& message);
    static float GetRandomNumber(float min, float max);
    static void WaitForSeconds(float seconds);
};

// Scripting API main class
class ScriptingAPI {
public:
    ScriptingAPI();
    ~ScriptingAPI();

    // Initialization
    bool initialize();
    void shutdown();
    void initialize_systems(physics_core::PhysicsCore* physics_core,
                           ai_core::IDecisionEngine* ai_engine,
                           MissionDesigner* mission_designer);

    // Lua script execution
    bool execute_script(const std::string& script_code, const ScriptExecutionContext& context, ScriptExecutionResult& result);
    bool load_script_file(const std::string& file_path, uint32_t& script_id);
    bool unload_script(uint32_t script_id);

    // Lua API binding
    bool bind_physics_api(lua_State* L);
    bool bind_damage_api(lua_State* L);
    bool bind_ai_api(lua_State* L);
    bool bind_mission_api(lua_State* L);
    bool bind_utility_api(lua_State* L);

    // Timer management
    uint32_t create_timer(float delay_seconds, const std::string& script_code, const ScriptExecutionContext& context, bool repeating = false, float repeat_interval = 0.0f);
    bool cancel_timer(uint32_t timer_id);
    void update_timers(float delta_time);

    // Script validation
    bool validate_script(const std::string& script_code, std::string& error_message);
    bool sandbox_script(const std::string& script_code, ScriptExecutionResult& result);

    // C# interop
    bool initialize_dotnet_runtime();
    void shutdown_dotnet_runtime();

    // Security
    bool set_script_permissions(uint32_t script_id, uint32_t permissions_mask);
    bool check_script_safety(const std::string& script_code, std::vector<std::string>& warnings);

private:
    class Impl;
    Impl* impl_;
};

} // namespace content_editor