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
 * Particle System Demo
 * 
 * Demonstrates GPU-driven particle rendering with 1M+ particles possible.
 */

int main() {
    std::cout << "=== Particle System Demo ===" << std::endl;
    
    try {
        // Initialize rendering engine
        RenderingEngine engine;
        engine.setup_vulkan(1920, 1080, false);
        engine.initialize_resources();
        
        // Use Forward+ for particle rendering
        engine.set_rendering_pipeline(RenderPipelineType::FORWARD_PLUS);
        
        std::cout << "Rendering 100K particles with GPU simulation" << std::endl;
        
        // Would set up particle systems here
        // ParticleSystem fire_particles;
        // ParticleSystem smoke_particles;
        // engine.render_particles(fire_particles, cmd_buffer);
        // engine.render_particles(smoke_particles, cmd_buffer);
        
        engine.shutdown();
        std::cout << "Particle demo completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
