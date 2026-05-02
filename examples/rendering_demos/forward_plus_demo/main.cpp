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
 * Forward+ Rendering Demo
 * 
 * Demonstrates the Forward+ pipeline rendering a scene with 1000+ dynamic lights.
 * This pipeline is ideal for outdoor scenes with many light sources.
 */

int main() {
    std::cout << "=== Forward+ Rendering Demo ===" << std::endl;
    
    try {
        // Initialize rendering engine
        RenderingEngine engine;
        engine.setup_vulkan(1920, 1080, false);
        engine.initialize_resources();
        
        // Switch to Forward+ pipeline
        engine.set_rendering_pipeline(RenderPipelineType::FORWARD_PLUS);
        std::cout << "Active pipeline: Forward+" << std::endl;
        
        // Create simple scene
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
        
        // Create lights
        std::vector<LightData> lights;
        for (uint32_t i = 0; i < 1000; ++i) {
            float angle = (i / 1000.0f) * glm::two_pi<float>();
            float radius = 20.0f;
            
            LightData light{
                .position = glm::vec3(
                    glm::cos(angle) * radius,
                    5.0f + glm::sin(i * 0.1f),
                    glm::sin(angle) * radius
                ),
                .radius = 15.0f,
                .color = glm::vec3(
                    0.5f + 0.5f * glm::sin(angle),
                    0.5f + 0.5f * glm::sin(angle + glm::two_pi<float>() / 3.0f),
                    0.5f + 0.5f * glm::sin(angle + 2.0f * glm::two_pi<float>() / 3.0f)
                ),
                .intensity = 1.0f,
                .light_type = static_cast<uint32_t>(LightType::POINT),
                .direction = glm::vec3(0.0f),
                .inner_angle = 0.0f,
                .outer_angle = 0.0f
            };
            lights.push_back(light);
        }
        
        std::cout << "Loaded " << lights.size() << " point lights" << std::endl;
        
        // Create renderables
        std::vector<RenderableObject> objects;
        
        RenderableObject ground_plane{
            .transform = glm::mat4(1.0f),
            .mesh_id = 0,
            .material_id = 0,
            .entity_id = 0,
            .cast_shadow = true,
            .is_dynamic = false
        };
        objects.push_back(ground_plane);
        
        std::cout << "Scene loaded with " << objects.size() << " objects" << std::endl;
        
        // Render loop (simplified)
        engine.set_performance_monitoring(true);
        
        for (int frame = 0; frame < 100; ++frame) {
            // Update camera
            camera.position.x = glm::cos(frame * 0.01f) * 15.0f;
            camera.position.z = glm::sin(frame * 0.01f) * 15.0f + 10.0f;
            
            engine.begin_frame(camera, lights);
            engine.queue_renderables(objects);
            
            // Would render to swapchain here
            // engine.render(render_target);
            
            engine.end_frame();
        }
        
        auto metrics = engine.get_performance_metrics();
        std::cout << "Performance Report:" << std::endl;
        std::cout << "  FPS: " << metrics.fps << std::endl;
        std::cout << "  Frame Time: " << metrics.frame_time_ms << " ms" << std::endl;
        std::cout << "  GPU Time: " << metrics.gpu_time_ms << " ms" << std::endl;
        
        engine.shutdown();
        
        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
