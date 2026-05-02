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
#include "content_editor/scripting_api.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <unordered_map>
#include <physics_core/physics_core.h>
#include <physics_core/damage_system.h>
#include <ai_core/decision_engine.h>
#include <content_editor/mission_designer.h>

namespace content_editor {

// Static pointers for ManagedScriptingAPI
static physics_core::PhysicsCore* g_physics_core = nullptr;
static ai_core::IDecisionEngine* g_ai_engine = nullptr;
static MissionDesigner* g_mission_designer = nullptr;

static ScriptingAPI::Impl* get_lua_impl(lua_State* L) {
    return *static_cast<ScriptingAPI::Impl**>(lua_getextraspace(L));
}

// Lua API function implementations
static int lua_get_entity_position(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    auto* impl = get_lua_impl(L);
    
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    auto* body = impl->physics_core_->get_body(entity_id);
    if (!body) {
        luaL_error(L, "entity %d not found", entity_id);
    }
    
    lua_pushnumber(L, body->position.x);
    lua_pushnumber(L, body->position.y);
    lua_pushnumber(L, body->position.z);
    return 3;
}

static int lua_set_entity_position(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    auto* body = impl->physics_core_->get_body(entity_id);
    if (!body) {
        luaL_error(L, "entity %d not found", entity_id);
    }
    
    body->position = physics_core::Vec3(x, y, z);
    body->is_sleeping = false;
    return 0;
}

static int lua_get_entity_velocity(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    auto* impl = get_lua_impl(L);
    
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    auto* body = impl->physics_core_->get_body(entity_id);
    if (!body) {
        luaL_error(L, "entity %d not found", entity_id);
    }
    
    lua_pushnumber(L, body->velocity.x);
    lua_pushnumber(L, body->velocity.y);
    lua_pushnumber(L, body->velocity.z);
    return 3;
}

static int lua_apply_force_to_entity(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    float fx = luaL_checknumber(L, 2);
    float fy = luaL_checknumber(L, 3);
    float fz = luaL_checknumber(L, 4);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    impl->physics_core_->apply_force(entity_id, physics_core::Vec3(fx, fy, fz));
    return 0;
}

static int lua_apply_damage_to_entity(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    float damage = luaL_checknumber(L, 2);
    int damage_type = luaL_checkinteger(L, 3);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    // TODO: use DamageSystem / ECS state instead of direct PhysicsBody health mutation.
    // Current implementation uses PhysicsBody::health as a temporary integration point.
    auto* body = impl->physics_core_->get_body(entity_id);
    if (body && body->health > 0) {
        body->health -= damage;
        if (body->health < 0) body->health = 0;
    }
    
    return 0;
}

static int lua_create_hinge_joint(lua_State* L) {
    int body_a = luaL_checkinteger(L, 1);
    int body_b = luaL_checkinteger(L, 2);
    float pax = luaL_checknumber(L, 3);
    float pay = luaL_checknumber(L, 4);
    float paz = luaL_checknumber(L, 5);
    float aax = luaL_checknumber(L, 6);
    float aay = luaL_checknumber(L, 7);
    float aaz = luaL_checknumber(L, 8);
    float pbx = luaL_checknumber(L, 9);
    float pby = luaL_checknumber(L, 10);
    float pbz = luaL_checknumber(L, 11);
    float abx = luaL_checknumber(L, 12);
    float aby = luaL_checknumber(L, 13);
    float abz = luaL_checknumber(L, 14);

    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }

    size_t constraint_id = impl->physics_core_->create_hinge_constraint(
        body_a,
        body_b,
        physics_core::Vec3(pax, pay, paz),
        physics_core::Vec3(pbx, pby, pbz),
        physics_core::Vec3(aax, aay, aaz).normalized(),
        physics_core::Vec3(abx, aby, abz).normalized(),
        -3.14159265f,
        3.14159265f,
        0.0f
    );

    lua_pushinteger(L, static_cast<lua_Integer>(constraint_id));
    return 1;
}

static int lua_create_soft_rope(lua_State* L) {
    int particle_count = luaL_checkinteger(L, 1);
    float length = luaL_checknumber(L, 2);
    float radius = luaL_checknumber(L, 3);
    float sx = luaL_checknumber(L, 4);
    float sy = luaL_checknumber(L, 5);
    float sz = luaL_checknumber(L, 6);
    float dx = luaL_checknumber(L, 7);
    float dy = luaL_checknumber(L, 8);
    float dz = luaL_checknumber(L, 9);
    int pin_start = lua_toboolean(L, 10);

    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }

    size_t soft_id = impl->physics_core_->create_soft_rope(
        static_cast<uint32_t>(particle_count),
        length,
        radius,
        physics_core::Vec3(sx, sy, sz),
        physics_core::Vec3(dx, dy, dz).normalized(),
        pin_start != 0
    );
    lua_pushinteger(L, static_cast<lua_Integer>(soft_id));
    return 1;
}

static int lua_create_soft_cloth(lua_State* L) {
    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    float spacing = luaL_checknumber(L, 3);
    float ox = luaL_checknumber(L, 4);
    float oy = luaL_checknumber(L, 5);
    float oz = luaL_checknumber(L, 6);
    int pin_corners = lua_toboolean(L, 7);

    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }

    size_t soft_id = impl->physics_core_->create_soft_cloth(
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        spacing,
        physics_core::Vec3(ox, oy, oz),
        pin_corners != 0
    );
    lua_pushinteger(L, static_cast<lua_Integer>(soft_id));
    return 1;
}

static int lua_create_fluid_source(lua_State* L) {
    int type = luaL_checkinteger(L, 1);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    float rate = luaL_checknumber(L, 5);

    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }

    physics_core::FluidType fluid_type = physics_core::FluidType::WATER;
    if (type == 1) fluid_type = physics_core::FluidType::OIL;
    if (type == 2) fluid_type = physics_core::FluidType::BLOOD;
    if (type == 3) fluid_type = physics_core::FluidType::FUEL;
    if (type == 4) fluid_type = physics_core::FluidType::CHEMICAL;

    physics_core::EntityID source_id = impl->physics_core_->create_fluid_source(
        fluid_type,
        physics_core::Vec3(x, y, z),
        rate
    );
    lua_pushinteger(L, static_cast<lua_Integer>(source_id));
    return 1;
}

static int lua_get_entity_health(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    auto* impl = get_lua_impl(L);
    
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    auto* body = impl->physics_core_->get_body(entity_id);
    if (!body) {
        luaL_error(L, "entity %d not found", entity_id);
    }
    
    // TODO: map to DamageSystem or vehicle/component damage state instead of direct body health
    lua_pushnumber(L, body->health);
    return 1;
}

static int lua_set_entity_health(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    float health = luaL_checknumber(L, 2);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    auto* body = impl->physics_core_->get_body(entity_id);
    if (!body) {
        luaL_error(L, "entity %d not found", entity_id);
    }
    
    // TODO: map to DamageSystem or vehicle/component damage state instead of direct body health
    body->health = health;
    return 0;
}

static int lua_set_entity_behavior(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    const char* behavior_name = luaL_checkstring(L, 2);
    
    auto* impl = get_lua_impl(L);
    if (!impl->ai_engine_) {
        luaL_error(L, "AI system not initialized");
    }
    
    // TODO: implement AI behavior assignment using ai_engine_ or mission system.
    // For now this is a noop placeholder that acknowledges the call.
    return 0;
}

static int lua_spawn_unit_at_position(lua_State* L) {
    const char* unit_template = luaL_checkstring(L, 1);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    float rx = luaL_checknumber(L, 5);
    float ry = luaL_checknumber(L, 6);
    float rz = luaL_checknumber(L, 7);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_ || !impl->mission_designer_) {
        luaL_error(L, "systems not initialized");
    }
    
    // TODO: add model, AI, and mission registration when spawning a unit.
    auto entity_id = impl->physics_core_->create_rigid_body(physics_core::Vec3(x, y, z));
    
    lua_pushinteger(L, entity_id);
    return 1;
}

static int lua_destroy_entity(lua_State* L) {
    int entity_id = luaL_checkinteger(L, 1);
    
    auto* impl = get_lua_impl(L);
    if (!impl->physics_core_) {
        luaL_error(L, "physics system not initialized");
    }
    
    impl->physics_core_->destroy_body(entity_id);
    return 0;
}

static int lua_get_mission_time(lua_State* L) {
    auto* impl = get_lua_impl(L);
    if (!impl->mission_designer_) {
        luaL_error(L, "mission system not initialized");
    }
    
    // TODO: return real mission time from MissionDesigner
    lua_pushnumber(L, 0.0f);
    return 1;
}

static int lua_trigger_event(lua_State* L) {
    const char* event_name = luaL_checkstring(L, 1);
    
    auto* impl = get_lua_impl(L);
    if (!impl->mission_designer_) {
        luaL_error(L, "mission system not initialized");
    }
    
    // TODO: invoke mission designer event system when available
    return 0;
}

static int lua_log_message(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    std::cout << "[Lua] " << message << std::endl;
    return 0;
}

static int lua_get_random_number(lua_State* L) {
    float min = luaL_checknumber(L, 1);
    float max = luaL_checknumber(L, 2);
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(min, max);
    lua_pushnumber(L, dist(gen));
    return 1;
}

class ScriptingAPI::Impl {
public:
    lua_State* lua_state_;
    std::unordered_map<uint32_t, std::string> loaded_scripts_;
    std::vector<ScriptTimer> active_timers_;
    std::mt19937 random_engine_;
    uint32_t next_script_id_;
    uint32_t next_timer_id_;
    int instruction_count_;  // For limiting execution

    // System pointers
    physics_core::PhysicsCore* physics_core_;
    ai_core::IDecisionEngine* ai_engine_;
    MissionDesigner* mission_designer_;

    Impl() : lua_state_(nullptr), random_engine_(std::random_device{}()), next_script_id_(1), next_timer_id_(1), instruction_count_(0),
             physics_core_(nullptr), ai_engine_(nullptr), mission_designer_(nullptr) {}

    ~Impl() {
        if (lua_state_) {
            lua_close(lua_state_);
        }
    }

    bool initialize_lua() {
        lua_state_ = luaL_newstate();
        if (!lua_state_) {
            return false;
        }

        luaL_openlibs(lua_state_);
        
        // Store pointer to Impl in lua extraspace
        *static_cast<ScriptingAPI::Impl**>(lua_getextraspace(lua_state_)) = this;

        // Register API functions
        lua_register(lua_state_, "get_entity_position", lua_get_entity_position);
        lua_register(lua_state_, "set_entity_position", lua_set_entity_position);
        lua_register(lua_state_, "get_entity_velocity", lua_get_entity_velocity);
        lua_register(lua_state_, "apply_force_to_entity", lua_apply_force_to_entity);
        lua_register(lua_state_, "apply_damage_to_entity", lua_apply_damage_to_entity);
        lua_register(lua_state_, "create_hinge_joint", lua_create_hinge_joint);
        lua_register(lua_state_, "create_soft_rope", lua_create_soft_rope);
        lua_register(lua_state_, "create_soft_cloth", lua_create_soft_cloth);
        lua_register(lua_state_, "create_fluid_source", lua_create_fluid_source);
        lua_register(lua_state_, "get_entity_health", lua_get_entity_health);
        lua_register(lua_state_, "set_entity_health", lua_set_entity_health);
        lua_register(lua_state_, "set_entity_behavior", lua_set_entity_behavior);
        lua_register(lua_state_, "spawn_unit_at_position", lua_spawn_unit_at_position);
        lua_register(lua_state_, "destroy_entity", lua_destroy_entity);
        lua_register(lua_state_, "get_mission_time", lua_get_mission_time);
        lua_register(lua_state_, "trigger_event", lua_trigger_event);
        lua_register(lua_state_, "log_message", lua_log_message);
        lua_register(lua_state_, "get_random_number", lua_get_random_number);

        return true;
    }

    bool execute_script_internal(const std::string& script_code, const ScriptExecutionContext& context, ScriptExecutionResult& result) {
        if (!lua_state_) {
            result.success = false;
            result.error_message = "Lua state not initialized";
            return false;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // Set instruction limit hook
        instruction_count_ = 0;
        lua_sethook(lua_state_, [](lua_State* L, lua_Debug* ar) {
            auto* impl = get_lua_impl(L);
            impl->instruction_count_++;
            if (impl->instruction_count_ > 1000000) {  // 1 million instructions limit
                luaL_error(L, "script execution exceeded instruction limit");
            }
        }, LUA_MASKCOUNT, 1000);

        // Load and execute script
        int load_result = luaL_loadstring(lua_state_, script_code.c_str());
        if (load_result != LUA_OK) {
            result.success = false;
            result.error_message = lua_tostring(lua_state_, -1);
            lua_pop(lua_state_, 1);
            lua_sethook(lua_state_, nullptr, 0, 0);
            return false;
        }

        // Set up execution context (simplified)
        lua_pushinteger(lua_state_, context.script_id);
        lua_setglobal(lua_state_, "script_id");

        lua_pushinteger(lua_state_, context.mission_id);
        lua_setglobal(lua_state_, "mission_id");

        // Execute
        int exec_result = lua_pcall(lua_state_, 0, LUA_MULTRET, 0);
        
        // Remove hook
        lua_sethook(lua_state_, nullptr, 0, 0);
        
        if (exec_result != LUA_OK) {
            result.success = false;
            result.error_message = lua_tostring(lua_state_, -1);
            lua_pop(lua_state_, 1);
            return false;
        }

        // Get return value if any
        if (lua_gettop(lua_state_) > 0) {
            if (lua_isstring(lua_state_, -1)) {
                result.return_value = lua_tostring(lua_state_, -1);
            }
            lua_pop(lua_state_, 1);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        result.success = true;
        result.execution_time_ms = duration.count() / 1000.0f;
        result.memory_used_kb = 0; // Placeholder

        return result.execution_time_ms < 1000.0f; // Check performance target
    }

    void update_timers_internal(float delta_time) {
        for (auto it = active_timers_.begin(); it != active_timers_.end(); ) {
            it->delay_seconds -= delta_time;
            if (it->delay_seconds <= 0.0f) {
                // Execute timer script
                ScriptExecutionResult result;
                execute_script_internal(it->script_code, it->context, result);

                if (it->is_repeating && it->repeat_count > 0) {
                    it->delay_seconds = it->repeat_interval;
                    it->repeat_count--;
                } else {
                    it = active_timers_.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
};

void ScriptingAPI::initialize_systems(physics_core::PhysicsCore* physics_core,
                                      ai_core::IDecisionEngine* ai_engine,
                                      MissionDesigner* mission_designer) {
    impl_->physics_core_ = physics_core;
    impl_->ai_engine_ = ai_engine;
    impl_->mission_designer_ = mission_designer;
    
    ManagedScriptingAPI::InitializeSystems(physics_core, ai_engine, mission_designer);
}

ScriptingAPI::ScriptingAPI() : impl_(new Impl()) {}

ScriptingAPI::~ScriptingAPI() {
    delete impl_;
}

bool ScriptingAPI::initialize() {
    return impl_->initialize_lua();
}

void ScriptingAPI::shutdown() {
    // Cleanup handled in Impl destructor
}

bool ScriptingAPI::execute_script(const std::string& script_code, const ScriptExecutionContext& context, ScriptExecutionResult& result) {
    return impl_->execute_script_internal(script_code, context, result);
}

bool ScriptingAPI::load_script_file(const std::string& file_path, uint32_t& script_id) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    std::string script_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    script_id = impl_->next_script_id_++;
    impl_->loaded_scripts_[script_id] = script_code;

    return true;
}

bool ScriptingAPI::unload_script(uint32_t script_id) {
    auto it = impl_->loaded_scripts_.find(script_id);
    if (it != impl_->loaded_scripts_.end()) {
        impl_->loaded_scripts_.erase(it);
        return true;
    }
    return false;
}

bool ScriptingAPI::bind_physics_api(lua_State* L) {
    // Physics API already bound in initialize_lua
    return true;
}

bool ScriptingAPI::bind_damage_api(lua_State* L) {
    // Placeholder - would bind damage functions
    return true;
}

bool ScriptingAPI::bind_ai_api(lua_State* L) {
    // Placeholder - would bind AI functions
    return true;
}

bool ScriptingAPI::bind_mission_api(lua_State* L) {
    // Placeholder - would bind mission functions
    return true;
}

bool ScriptingAPI::bind_utility_api(lua_State* L) {
    // Utility API already bound in initialize_lua
    return true;
}

uint32_t ScriptingAPI::create_timer(float delay_seconds, const std::string& script_code, const ScriptExecutionContext& context, bool repeating, float repeat_interval) {
    ScriptTimer timer = {};
    timer.timer_id = impl_->next_timer_id_++;
    timer.delay_seconds = delay_seconds;
    timer.script_code = script_code;
    timer.context = context;
    timer.is_repeating = repeating;
    timer.repeat_interval = repeat_interval;
    timer.repeat_count = repeating ? 10 : 1; // Default repeat count

    impl_->active_timers_.push_back(timer);
    return timer.timer_id;
}

bool ScriptingAPI::cancel_timer(uint32_t timer_id) {
    auto it = std::find_if(impl_->active_timers_.begin(), impl_->active_timers_.end(),
                          [timer_id](const ScriptTimer& timer) { return timer.timer_id == timer_id; });

    if (it != impl_->active_timers_.end()) {
        impl_->active_timers_.erase(it);
        return true;
    }
    return false;
}

void ScriptingAPI::update_timers(float delta_time) {
    impl_->update_timers_internal(delta_time);
}

bool ScriptingAPI::validate_script(const std::string& script_code, std::string& error_message) {
    if (!impl_->lua_state_) {
        error_message = "Lua state not initialized";
        return false;
    }

    int result = luaL_loadstring(impl_->lua_state_, script_code.c_str());
    if (result != LUA_OK) {
        error_message = lua_tostring(impl_->lua_state_, -1);
        lua_pop(impl_->lua_state_, 1);
        return false;
    }

    // Clean up loaded chunk
    lua_pop(impl_->lua_state_, 1);
    return true;
}

bool ScriptingAPI::sandbox_script(const std::string& script_code, ScriptExecutionResult& result) {
    // Create a new Lua state for sandboxing
    lua_State* sandbox_state = luaL_newstate();
    if (!sandbox_state) {
        result.success = false;
        result.error_message = "Failed to create sandbox state";
        return false;
    }

    // Load only safe libraries
    luaL_requiref(sandbox_state, "_G", luaopen_base, 1);
    luaL_requiref(sandbox_state, "math", luaopen_math, 1);
    luaL_requiref(sandbox_state, "string", luaopen_string, 1);
    luaL_requiref(sandbox_state, "table", luaopen_table, 1);
    // Do not load io, os, package, etc.

    lua_pop(sandbox_state, 4);  // Remove the loaded modules from stack

    // Register only safe API functions
    lua_register(sandbox_state, "log_message", lua_log_message);
    lua_register(sandbox_state, "get_random_number", lua_get_random_number);

    // Execute in sandbox
    int load_result = luaL_loadstring(sandbox_state, script_code.c_str());
    if (load_result != LUA_OK) {
        result.success = false;
        result.error_message = lua_tostring(sandbox_state, -1);
        lua_close(sandbox_state);
        return false;
    }

    int exec_result = lua_pcall(sandbox_state, 0, LUA_MULTRET, 0);
    if (exec_result != LUA_OK) {
        result.success = false;
        result.error_message = lua_tostring(sandbox_state, -1);
    } else {
        result.success = true;
    }

    lua_close(sandbox_state);
    return result.success;
}

bool ScriptingAPI::initialize_dotnet_runtime() {
    // Placeholder for .NET runtime initialization
    return true;
}

void ScriptingAPI::shutdown_dotnet_runtime() {
    // Placeholder for .NET runtime shutdown
}

bool ScriptingAPI::set_script_permissions(uint32_t script_id, uint32_t permissions_mask) {
    // Placeholder for permission setting
    return true;
}

bool ScriptingAPI::check_script_safety(const std::string& script_code, std::vector<std::string>& warnings) {
    warnings.clear();

    // Basic safety checks
    if (script_code.find("os.execute") != std::string::npos) {
        warnings.push_back("Script contains os.execute which may be unsafe");
    }

    if (script_code.find("io.popen") != std::string::npos) {
        warnings.push_back("Script contains io.popen which may be unsafe");
    }

    if (script_code.find("loadfile") != std::string::npos) {
        warnings.push_back("Script contains loadfile which may allow file system access");
    }

    if (script_code.find("dofile") != std::string::npos) {
        warnings.push_back("Script contains dofile which may allow file system access");
    }

    if (script_code.find("require") != std::string::npos) {
        warnings.push_back("Script contains require which may allow module loading");
    }

    return warnings.empty();
}

// ManagedScriptingAPI implementations
void ManagedScriptingAPI::InitializeSystems(physics_core::PhysicsCore* physics, ai_core::IDecisionEngine* ai, MissionDesigner* mission) {
    g_physics_core = physics;
    g_ai_engine = ai;
    g_mission_designer = mission;
}

physics_core::Vec3 ManagedScriptingAPI::GetEntityPosition(int entityId) {
    if (!g_physics_core) return physics_core::Vec3();
    auto* body = g_physics_core->get_body(entityId);
    return body ? body->position : physics_core::Vec3();
}

void ManagedScriptingAPI::SetEntityPosition(int entityId, physics_core::Vec3 position) {
    if (!g_physics_core) return;
    auto* body = g_physics_core->get_body(entityId);
    if (body) {
        body->position = position;
        body->is_sleeping = false;
    }
}

physics_core::Vec3 ManagedScriptingAPI::GetEntityVelocity(int entityId) {
    if (!g_physics_core) return physics_core::Vec3();
    auto* body = g_physics_core->get_body(entityId);
    return body ? body->velocity : physics_core::Vec3();
}

void ManagedScriptingAPI::ApplyForceToEntity(int entityId, physics_core::Vec3 force) {
    if (g_physics_core) {
        g_physics_core->apply_force(entityId, force);
    }
}

void ManagedScriptingAPI::ApplyDamageToEntity(int entityId, float damage, int damageType) {
    // TODO: Implement damage application using DamageSystem
}

float ManagedScriptingAPI::GetEntityHealth(int entityId) {
    if (!g_physics_core) return 0.0f;
    auto* body = g_physics_core->get_body(entityId);
    return body ? body->health : 0.0f;  // Assuming health field exists
}

void ManagedScriptingAPI::SetEntityHealth(int entityId, float health) {
    if (!g_physics_core) return;
    auto* body = g_physics_core->get_body(entityId);
    if (body) {
        body->health = health;
    }
}

void ManagedScriptingAPI::SetEntityBehavior(int entityId, const std::string& behaviorName) {
    // TODO: implement AI behavior assignment using ai_engine_ or mission designer.
}

void ManagedScriptingAPI::IssueCommandToEntity(int entityId, const std::string& command, const std::vector<std::string>& parameters) {
    // TODO: implement command dispatch to AI/mission systems.
}

int ManagedScriptingAPI::SpawnUnitAtPosition(const std::string& unitTemplate, physics_core::Vec3 position, physics_core::Vec3 rotation) {
    if (!g_physics_core) return 0;
    // TODO: add model, AI, crew, and mission registration when spawning a unit.
    return g_physics_core->create_rigid_body(position);
}

void ManagedScriptingAPI::DestroyEntity(int entityId) {
    if (g_physics_core) {
        g_physics_core->destroy_body(entityId);
    }
}

float ManagedScriptingAPI::GetMissionTime() {
    // TODO: return mission designer time
    return 0.0f;
}

void ManagedScriptingAPI::TriggerEvent(const std::string& eventName) {
    // TODO: route event to MissionDesigner when implemented
}

void ManagedScriptingAPI::LogMessage(const std::string& message) {
    std::cout << "[Managed] " << message << std::endl;
}

float ManagedScriptingAPI::GetRandomNumber(float min, float max) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

void ManagedScriptingAPI::WaitForSeconds(float seconds) {
    // Placeholder
}

} // namespace content_editor

extern "C" {

void GetEntityPosition(int entityId, float* position) {
    if (!position) return;
    if (!content_editor::g_physics_core) {
        position[0] = position[1] = position[2] = 0.0f;
        return;
    }
    auto* body = content_editor::g_physics_core->get_body(entityId);
    if (body) {
        position[0] = body->position.x;
        position[1] = body->position.y;
        position[2] = body->position.z;
    } else {
        position[0] = position[1] = position[2] = 0.0f;
    }
}

void SetEntityPosition(int entityId, float x, float y, float z) {
    if (!content_editor::g_physics_core) return;
    auto* body = content_editor::g_physics_core->get_body(entityId);
    if (body) {
        body->position = physics_core::Vec3(x, y, z);
        body->is_sleeping = false;
    }
}

void GetEntityVelocity(int entityId, float* velocity) {
    if (!velocity) return;
    if (!content_editor::g_physics_core) {
        velocity[0] = velocity[1] = velocity[2] = 0.0f;
        return;
    }
    auto* body = content_editor::g_physics_core->get_body(entityId);
    if (body) {
        velocity[0] = body->velocity.x;
        velocity[1] = body->velocity.y;
        velocity[2] = body->velocity.z;
    } else {
        velocity[0] = velocity[1] = velocity[2] = 0.0f;
    }
}

void ApplyForceToEntity(int entityId, float fx, float fy, float fz) {
    if (!content_editor::g_physics_core) return;
    content_editor::g_physics_core->apply_force(entityId, physics_core::Vec3(fx, fy, fz));
}

void ApplyDamageToEntity(int entityId, float damage, int damageType) {
    if (!content_editor::g_physics_core) return;
    auto* body = content_editor::g_physics_core->get_body(entityId);
    if (body && body->health > 0) {
        body->health -= damage;
        if (body->health < 0.0f) body->health = 0.0f;
    }
}

float GetEntityHealth(int entityId) {
    if (!content_editor::g_physics_core) return 0.0f;
    auto* body = content_editor::g_physics_core->get_body(entityId);
    return body ? body->health : 0.0f;
}

void SetEntityHealth(int entityId, float health) {
    if (!content_editor::g_physics_core) return;
    auto* body = content_editor::g_physics_core->get_body(entityId);
    if (body) {
        body->health = health;
    }
}

void SetEntityBehavior(int entityId, const char* behaviorName) {
    // TODO: implement AI behavior assignment through g_ai_engine or mission designer.
}

void IssueCommandToEntity(int entityId, const char* command, const char** parameters, int paramCount) {
    // TODO: implement command dispatch to AI/mission systems.
}

int SpawnUnitAtPosition(const char* unitTemplate, float x, float y, float z, float rx, float ry, float rz) {
    if (!content_editor::g_physics_core) return 0;
    // TODO: initialize model, AI, crew, and mission state for spawned units.
    return content_editor::g_physics_core->create_rigid_body(physics_core::Vec3(x, y, z));
}

void DestroyEntity(int entityId) {
    if (!content_editor::g_physics_core) return;
    content_editor::g_physics_core->destroy_body(entityId);
}

float GetMissionTime() {
    // TODO: return mission designer time when MissionDesigner is integrated.
    return 0.0f;
}

void TriggerEvent(const char* eventName) {
    // TODO: route event to MissionDesigner when implemented.
}

void LogMessage(const char* message) {
    if (message) {
        std::cout << "[C#] " << message << std::endl;
    }
}

float GetRandomNumber(float min, float max) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

void WaitForSeconds(float seconds) {
    // TODO: implement time delay/async behavior for managed scripts.
}

}