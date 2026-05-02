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
#include "rendering_engine/rendering_engine.h"
#include <iostream>
#include <glm/glm.hpp>

using namespace rendering_engine;

/**
 * Lighting Demo
 * 
 * Demonstrates comprehensive lighting features across all pipelines.
 * Shows different light types and their interactions.
 */

int main() {
    std::cout << "=== Lighting Demo ===" << std::endl;
    
    try {
        // Initialize rendering engine
        RenderingEngine engine;
        engine.setup_vulkan(1920, 1080, false);
        engine.initialize_resources();
        
        // Print available pipelines
        auto available = engine.get_available_pipelines();
        std::cout << "Available pipelines:" << std::endl;
        for (auto pipeline : available) {
            std::string name;
            switch (pipeline) {
                case RenderPipelineType::FORWARD_PLUS: name = "Forward+"; break;
                case RenderPipelineType::DEFERRED: name = "Deferred"; break;
                case RenderPipelineType::CLUSTERED: name = "Clustered"; break;
                default: name = "Unknown"; break;
            }
            std::cout << "  - " << name << std::endl;
        }
        
        // Use Forward+ for this demo
        engine.set_rendering_pipeline(RenderPipelineType::FORWARD_PLUS);
        
        // Create camera
        Camera camera{
            .position = glm::vec3(0.0f, 3.0f, 8.0f),
            .forward = glm::vec3(0.0f, -0.3f, -1.0f),
            .right = glm::vec3(1.0f, 0.0f, 0.0f),
            .up = glm::vec3(0.0f, 1.0f, 0.0f),
            .fov = glm::radians(45.0f),
            .aspect_ratio = 1920.0f / 1080.0f,
            .near_plane = 0.1f,
            .far_plane = 100.0f
        };
        
        // Create diverse lighting setup
        std::vector<LightData> lights;
        
        // Directional light (sun)
        LightData sun{
            .position = glm::vec3(50.0f, 50.0f, 50.0f),
            .radius = 1000.0f,
            .color = glm::vec3(1.0f, 0.95f, 0.85f),
            .intensity = 2.0f,
            .light_type = static_cast<uint32_t>(LightType::DIRECTIONAL),
            .direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f))
        };
        lights.push_back(sun);
        
        // Point lights - ambient fill
        for (uint32_t i = 0; i < 100; ++i) {
            float angle = (i / 100.0f) * glm::two_pi<float>();
            float dist = 5.0f + 3.0f * glm::sin(i * 0.02f);
            
            LightData light{
                .position = glm::vec3(
                    glm::cos(angle) * dist,
                    2.0f + glm::sin(angle) * 0.5f,
                    glm::sin(angle) * dist
                ),
                .radius = 8.0f,
                .color = glm::vec3(
                    0.5f + 0.5f * glm::sin(angle),
                    0.5f + 0.5f * glm::sin(angle + glm::two_pi<float>() / 3.0f),
                    0.5f + 0.5f * glm::sin(angle + 2.0f * glm::two_pi<float>() / 3.0f)
                ),
                .intensity = 0.5f,
                .light_type = static_cast<uint32_t>(LightType::POINT)
            };
            lights.push_back(light);
        }
        
        // Spot lights
        LightData spotlight1{
            .position = glm::vec3(3.0f, 4.0f, 0.0f),
            .radius = 20.0f,
            .color = glm::vec3(1.0f, 0.2f, 0.2f),
            .intensity = 1.5f,
            .light_type = static_cast<uint32_t>(LightType::SPOT),
            .direction = glm::normalize(glm::vec3(0.0f, -0.5f, -1.0f)),
            .inner_angle = glm::radians(15.0f),
            .outer_angle = glm::radians(30.0f)
        };
        lights.push_back(spotlight1);
        
        LightData spotlight2{
            .position = glm::vec3(-3.0f, 4.0f, 0.0f),
            .radius = 20.0f,
            .color = glm::vec3(0.2f, 0.2f, 1.0f),
            .intensity = 1.5f,
            .light_type = static_cast<uint32_t>(LightType::SPOT),
            .direction = glm::normalize(glm::vec3(0.0f, -0.5f, -1.0f)),
            .inner_angle = glm::radians(15.0f),
            .outer_angle = glm::radians(30.0f)
        };
        lights.push_back(spotlight2);
        
        std::cout << "Created " << lights.size() << " lights" << std::endl;
        std::cout << "  - 1 directional" << std::endl;
        std::cout << "  - 100 point lights" << std::endl;
        std::cout << "  - 2 spot lights" << std::endl;
        
        // Create scene
        std::vector<RenderableObject> objects;
        
        // Geometry
        for (int i = 0; i < 3; ++i) {
            RenderableObject obj{
                .transform = glm::translate(glm::mat4(1.0f), 
                    glm::vec3(i * 3.0f - 3.0f, 0.0f, 0.0f)),
                .mesh_id = 0,
                .material_id = i,
                .entity_id = i,
                .cast_shadow = true,
                .is_dynamic = false
            };
            objects.push_back(obj);
        }
        
        engine.set_performance_monitoring(true);
        
        // Render frames
        for (int frame = 0; frame < 100; ++frame) {
            engine.begin_frame(camera, lights);
            engine.queue_renderables(objects);
            engine.end_frame();
        }
        
        auto metrics = engine.get_performance_metrics();
        std::cout << "\nPerformance Report:" << std::endl;
        std::cout << "  FPS: " << metrics.fps << std::endl;
        std::cout << "  Frame Time: " << metrics.frame_time_ms << " ms" << std::endl;
        
        engine.shutdown();
        std::cout << "\nLighting demo completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
