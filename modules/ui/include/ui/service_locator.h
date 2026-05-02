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

#include <string>
#include <unordered_map>
#include <stdexcept>

// Forward declarations for public interfaces
namespace physics_core { class PhysicsCore; }
namespace rendering_engine { class RenderingEngine; class ModelSystem; }
namespace ai_core { class AIWorld; }
namespace historical_vehicle_system { class HistoricalVehicleSystem; }
namespace content_editor { class VehicleEditor; class MissionDesigner; }
namespace rendering_engine { class RenderingEngine; class ModelSystem; class CameraController; }
namespace scripting_api { class ScriptingAPI; }

namespace ui {

class ServiceLocator {
public:
    // Registration by name (for tests/scripts)
    void Register(const std::string& name, void* service);

    // Setters for direct assignment
    void SetPhysics(physics_core::PhysicsCore* ptr) { physics_ = ptr; }
    void SetRenderingEngine(rendering_engine::RenderingEngine* ptr) { rendering_ = ptr; }
    void SetModelSystem(rendering_engine::ModelSystem* ptr) { models_ = ptr; }
    void SetCameraController(rendering_engine::CameraController* ptr) { cameraController_ = ptr; }
    void SetAIWorld(ai_core::AIWorld* ptr) { aiWorld_ = ptr; }
    void SetHistoricalVehicleSystem(historical_vehicle_system::HistoricalVehicleSystem* ptr) { historical_ = ptr; }
    void SetVehicleEditor(content_editor::VehicleEditor* ptr) { vehicleEditor_ = ptr; }
    void SetMissionDesigner(content_editor::MissionDesigner* ptr) { missionDesigner_ = ptr; }
    void SetScriptingAPI(scripting_api::ScriptingAPI* ptr) { scripting_ = ptr; }

    // Typed getters
    physics_core::PhysicsCore* GetPhysics() const;
    rendering_engine::RenderingEngine* GetRenderingEngine() const;
    rendering_engine::ModelSystem* GetModelSystem() const;
    rendering_engine::CameraController* GetCameraController() const;
    ai_core::AIWorld* GetAIWorld() const;
    historical_vehicle_system::HistoricalVehicleSystem* GetHistoricalVehicleSystem() const;
    content_editor::VehicleEditor* GetVehicleEditor() const;
    content_editor::MissionDesigner* GetMissionDesigner() const;
    scripting_api::ScriptingAPI* GetScriptingAPI() const;

    // Universal getter (returns nullptr if type not registered)
    template<typename T>
    T* Get() {
        // Specializations defined in .cpp for each type
        return nullptr;
    }

private:
    std::unordered_map<std::string, void*> services_;

    // Direct pointers for frequently used types (for speed)
    physics_core::PhysicsCore* physics_ = nullptr;
    rendering_engine::RenderingEngine* rendering_ = nullptr;
    rendering_engine::ModelSystem* models_ = nullptr;
    rendering_engine::CameraController* cameraController_ = nullptr;
    ai_core::AIWorld* aiWorld_ = nullptr;
    historical_vehicle_system::HistoricalVehicleSystem* historical_ = nullptr;
    content_editor::VehicleEditor* vehicleEditor_ = nullptr;
    content_editor::MissionDesigner* missionDesigner_ = nullptr;
    scripting_api::ScriptingAPI* scripting_ = nullptr;
};

} // namespace ui