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
#include "ui/service_locator.h"

namespace ui {

void ServiceLocator::Register(const std::string& name, void* service) {
    services_[name] = service;
}

physics_core::PhysicsCore* ServiceLocator::GetPhysics() const {
    return physics_;
}

rendering_engine::RenderingEngine* ServiceLocator::GetRenderingEngine() const {
    return rendering_;
}

rendering_engine::ModelSystem* ServiceLocator::GetModelSystem() const {
    return models_;
}

rendering_engine::CameraController* ServiceLocator::GetCameraController() const {
    return cameraController_;
}

ai_core::AIWorld* ServiceLocator::GetAIWorld() const {
    return aiWorld_;
}

historical_vehicle_system::HistoricalVehicleSystem* ServiceLocator::GetHistoricalVehicleSystem() const {
    return historical_;
}

content_editor::VehicleEditor* ServiceLocator::GetVehicleEditor() const {
    return vehicleEditor_;
}

content_editor::MissionDesigner* ServiceLocator::GetMissionDesigner() const {
    return missionDesigner_;
}

scripting_api::ScriptingAPI* ServiceLocator::GetScriptingAPI() const {
    return scripting_;
}

// Template specializations
template<>
physics_core::PhysicsCore* ServiceLocator::Get<physics_core::PhysicsCore>() {
    return physics_;
}

template<>
rendering_engine::RenderingEngine* ServiceLocator::Get<rendering_engine::RenderingEngine>() {
    return rendering_;
}

template<>
rendering_engine::ModelSystem* ServiceLocator::Get<rendering_engine::ModelSystem>() {
    return models_;
}

template<>
rendering_engine::CameraController* ServiceLocator::Get<rendering_engine::CameraController>() {
    return cameraController_;
}

template<>
ai_core::AIWorld* ServiceLocator::Get<ai_core::AIWorld>() {
    return aiWorld_;
}

template<>
historical_vehicle_system::HistoricalVehicleSystem* ServiceLocator::Get<historical_vehicle_system::HistoricalVehicleSystem>() {
    return historical_;
}

template<>
content_editor::VehicleEditor* ServiceLocator::Get<content_editor::VehicleEditor>() {
    return vehicleEditor_;
}

template<>
content_editor::MissionDesigner* ServiceLocator::Get<content_editor::MissionDesigner>() {
    return missionDesigner_;
}

template<>
scripting_api::ScriptingAPI* ServiceLocator::Get<scripting_api::ScriptingAPI>() {
    return scripting_;
}

} // namespace ui