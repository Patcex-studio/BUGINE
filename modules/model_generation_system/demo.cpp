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
#include "model_generation_system.h"
#include <iostream>

int main() {
    // This is a demonstration of the Model Generation System API
    // In a real application, Vulkan device would be initialized here
    
    std::cout << "Model Generation System Demo" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Create a simple blueprint
    model_generation::VehicleBlueprintDefinition blueprint;
    blueprint.blueprint_name = "Demo Tank";
    blueprint.vehicle_class = model_generation::TANK;
    
    // Add a root node (hull)
    model_generation::BlueprintNode hull_node;
    hull_node.node_type = model_generation::PRIMITIVE;
    hull_node.local_transform = _mm256_set_ps(0, 0, 0, 1, 0, 0, 0, 1); // Identity transform
    hull_node.lod_level = 0;
    
    blueprint.nodes.push_back(hull_node);
    
    std::cout << "Blueprint created: " << blueprint.blueprint_name << std::endl;
    std::cout << "Nodes: " << blueprint.nodes.size() << std::endl;
    
    // In a real application with Vulkan initialized:
    // ModelGenerationSystem generator(device, physical_device, command_pool, queue);
    // GeneratedMesh mesh;
    // generator.generate_mesh_from_blueprint(blueprint, options, mesh);
    
    std::cout << "Demo completed successfully!" << std::endl;
    return 0;
}