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

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "blueprint_engine.h"

namespace model_generation {

// Forward declarations
struct HistoricalVehicleSpecs;

struct HistoricalVehicleSpecs {
    uint32_t nation;
    uint32_t era;
    uint32_t vehicle_type;
    // Add more fields as needed
};

struct TextureSynthesisParameters {
    uint32_t nation;                // USSR, GERMANY, USA, UK, etc.
    uint32_t era;                   // WW2, COLD_WAR, MODERN
    uint32_t vehicle_type;          // TANK, TRUCK, AIRCRAFT
    uint32_t camouflage_pattern;     // Desert, Forest, Urban, Winter
    float wear_level;               // 0.0 = new, 1.0 = heavily worn
    float dirt_level;               // Environmental dirt accumulation
    float damage_level;             // Battle damage intensity
    bool has_rust;                  // Rust formation enabled
    bool has_peeling_paint;         // Paint peeling effects
};

struct GeneratedTextureSet {
    VkImage albedo_texture;         // Base color + roughness
    VkImage normal_texture;         // Normal map
    VkImage metallic_texture;       // Metallic/ambient occlusion
    VkImage emissive_texture;       // Emissive/glow map
    VkImage damage_mask;            // Damage overlay mask
    uint32_t texture_resolution;     // Resolution (512, 1024, 2048, 4096)
    VkDeviceMemory memory;          // GPU memory allocation
};

class TextureSynthesisSystem {
public:
    TextureSynthesisSystem(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue queue);
    ~TextureSynthesisSystem();

    // Main synthesis function
    bool generate_historical_textures(
        const HistoricalVehicleSpecs& vehicle_specs,
        const TextureSynthesisParameters& params,
        GeneratedTextureSet& output_textures
    );

    // Utility functions
    void cleanup_texture_set(GeneratedTextureSet& texture_set);

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VkCommandPool command_pool_;
    VkQueue queue_;

    // Internal pipelines and resources
    VkPipeline synthesis_pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;

    // Helper methods
    bool create_synthesis_pipeline();
    bool allocate_texture_memory(GeneratedTextureSet& textures);
    bool run_synthesis_compute(const TextureSynthesisParameters& params, GeneratedTextureSet& textures);
};

} // namespace model_generation