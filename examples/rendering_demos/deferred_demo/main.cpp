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
 * Deferred Shading Demo
 * 
 * Demonstrates the Deferred Shading pipeline rendering complex materials
 * with physically-based rendering (PBR).
 */

int main() {
    std::cout << "=== Deferred Shading Demo ===" << std::endl;
    
    try {
        // Initialize rendering engine
        RenderingEngine engine;
        engine.setup_vulkan(1920, 1080, false);
        engine.initialize_resources();
        
        // Switch to Deferred pipeline
        engine.set_rendering_pipeline(RenderPipelineType::DEFERRED);
        std::cout << "Active pipeline: Deferred Shading" << std::endl;
        
        // Create camera
        Camera camera{
            .position = glm::vec3(0.0f, 5.0f, 10.0f),
            .forward = glm::vec3(0.0f, 0.0f, -1.0f),
            .right = glm::vec3(1.0f, 0.0f, 0.0f),
            .up = glm::vec3(0.0f, 1.0f, 0.0f),
            .fov = glm::radians(45.0f),
            .aspect_ratio = 1920.0f / 1080.0f,
            .near_plane = 0.1f,
            .far_plane = 1000.0f
        };
        
        // Create lights with varied properties
        std::vector<LightData> lights;
        
        // Main directional light (sun)
        LightData sun{
            .position = glm::vec3(50.0f, 50.0f, 50.0f),
            .radius = 1000.0f,
            .color = glm::vec3(1.0f, 0.95f, 0.8f),
            .intensity = 2.0f,
            .light_type = static_cast<uint32_t>(LightType::DIRECTIONAL),
            .direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)),
            .inner_angle = 0.0f,
            .outer_angle = 0.0f
        };
        lights.push_back(sun);
        
        // Point lights for variety
        for (uint32_t i = 0; i < 50; ++i) {
            float angle = (i / 50.0f) * glm::two_pi<float>();
            LightData light{
                .position = glm::vec3(
                    glm::cos(angle) * 10.0f,
                    3.0f,
                    glm::sin(angle) * 10.0f
                ),
                .radius = 10.0f,
                .color = glm::vec3(
                    0.8f + 0.2f * glm::sin(angle),
                    0.8f + 0.2f * glm::cos(angle),
                    0.8f + 0.2f * glm::sin(angle + glm::pi<float>() / 2.0f)
                ),
                .intensity = 1.5f,
                .light_type = static_cast<uint32_t>(LightType::POINT)
            };
            lights.push_back(light);
        }
        
        std::cout << "Loaded " << lights.size() << " lights" << std::endl;
        
        // Create scene with various materials
        std::vector<RenderableObject> objects;
        
        // Material variations
        for (uint32_t mat = 0; mat < 10; ++mat) {
            RenderableObject obj{
                .transform = glm::translate(glm::mat4(1.0f), 
                    glm::vec3(mat * 2.0f - 9.0f, 0.0f, 0.0f)),
                .mesh_id = 0,
                .material_id = mat,
                .entity_id = mat,
                .cast_shadow = true,
                .is_dynamic = false
            };
            objects.push_back(obj);
        }
        
        std::cout << "Scene loaded with " << objects.size() << " objects and multiple materials" << std::endl;
        
        // Render loop
        engine.set_performance_monitoring(true);
        
        for (int frame = 0; frame < 100; ++frame) {
            engine.begin_frame(camera, lights);
            engine.queue_renderables(objects);
            engine.end_frame();
        }
        
        auto metrics = engine.get_performance_metrics();
        std::cout << "\nPerformance Report:" << std::endl;
        std::cout << "  FPS: " << metrics.fps << std::endl;
        std::cout << "  Frame Time: " << metrics.frame_time_ms << " ms" << std::endl;
        std::cout << "  Draw Calls: " << metrics.draw_calls << std::endl;
        std::cout << "  Triangle Count: " << metrics.triangle_count << std::endl;
        
        engine.shutdown();
        
        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
