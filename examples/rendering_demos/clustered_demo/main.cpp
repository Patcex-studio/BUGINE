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
 * Clustered Shading Demo
 * 
 * Demonstrates the Clustered Shading pipeline rendering 10,000+ lights
 * efficiently using 3D frustum clustering.
 */

int main() {
    std::cout << "=== Clustered Shading Demo ===" << std::endl;
    
    try {
        // Initialize rendering engine
        RenderingEngine engine;
        engine.setup_vulkan(1920, 1080, false);
        engine.initialize_resources();
        
        // Switch to Clustered pipeline
        engine.set_rendering_pipeline(RenderPipelineType::CLUSTERED);
        std::cout << "Active pipeline: Clustered Shading" << std::endl;
        
        // Create camera
        Camera camera{
            .position = glm::vec3(0.0f, 5.0f, 15.0f),
            .forward = glm::vec3(0.0f, -0.3f, -1.0f),
            .right = glm::vec3(1.0f, 0.0f, 0.0f),
            .up = glm::vec3(0.0f, 1.0f, 0.0f),
            .fov = glm::radians(45.0f),
            .aspect_ratio = 1920.0f / 1080.0f,
            .near_plane = 0.1f,
            .far_plane = 100.0f
        };
        
        // Create massive light setup (10K+ point lights)
        std::vector<LightData> lights;
        
        const uint32_t lights_x = 20;
        const uint32_t lights_y = 20;
        const uint32_t lights_z = 20;
        
        for (uint32_t z = 0; z < lights_z; ++z) {
            for (uint32_t y = 0; y < lights_y; ++y) {
                for (uint32_t x = 0; x < lights_x; ++x) {
                    LightData light{
                        .position = glm::vec3(
                            (x - lights_x / 2.0f) * 1.0f,
                            (y - lights_y / 2.0f) * 1.0f,
                            (z - lights_z / 2.0f) * 1.0f
                        ),
                        .radius = 2.0f,
                        .color = glm::vec3(
                            0.5f + 0.5f * glm::sin(x * 0.5f),
                            0.5f + 0.5f * glm::sin(y * 0.5f),
                            0.5f + 0.5f * glm::sin(z * 0.5f)
                        ),
                        .intensity = 1.0f,
                        .light_type = static_cast<uint32_t>(LightType::POINT)
                    };
                    lights.push_back(light);
                }
            }
        }
        
        std::cout << "Loaded " << lights.size() << " point lights" << std::endl;
        std::cout << "Grid: " << lights_x << "x" << lights_y << "x" << lights_z << std::endl;
        
        // Create simple scene geometry
        std::vector<RenderableObject> objects;
        
        // Large floor plane
        RenderableObject floor{
            .transform = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 1.0f, 20.0f)),
            .mesh_id = 0,
            .material_id = 0,
            .entity_id = 0,
            .cast_shadow = true,
            .is_dynamic = false
        };
        objects.push_back(floor);
        
        // Central column
        RenderableObject column{
            .transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 10.0f, 2.0f)),
            .mesh_id = 0,
            .material_id = 1,
            .entity_id = 1,
            .cast_shadow = true,
            .is_dynamic = false
        };
        objects.push_back(column);
        
        std::cout << "Scene loaded with " << objects.size() << " objects" << std::endl;
        
        // Render loop
        engine.set_performance_monitoring(true);
        
        for (int frame = 0; frame < 100; ++frame) {
            // Rotate camera around scene
            float angle = (frame / 100.0f) * glm::two_pi<float>();
            camera.position = glm::vec3(
                glm::cos(angle) * 15.0f,
                5.0f,
                glm::sin(angle) * 15.0f
            );
            camera.forward = glm::normalize(-camera.position);
            
            engine.begin_frame(camera, lights);
            engine.queue_renderables(objects);
            engine.end_frame();
        }
        
        auto metrics = engine.get_performance_metrics();
        std::cout << "\nPerformance Report:" << std::endl;
        std::cout << "  FPS: " << metrics.fps << std::endl;
        std::cout << "  Frame Time: " << metrics.frame_time_ms << " ms" << std::endl;
        std::cout << "  GPU Time: " << metrics.gpu_time_ms << " ms" << std::endl;
        std::cout << "  Memory Usage: " << (metrics.memory_usage / 1024 / 1024) << " MB" << std::endl;
        
        engine.shutdown();
        
        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
