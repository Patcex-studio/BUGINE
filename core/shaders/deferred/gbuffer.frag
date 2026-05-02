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
#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(push_constant) uniform MaterialPushConstants {
    mat4 model_matrix;
    vec4 albedo;
    vec4 material_params; // roughness, metallic, ao, emissive
} material;

layout(location = 0) out vec4 outAlbedoRoughness;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

void main() {
    // Albedo + roughness
    outAlbedoRoughness = vec4(inColor.rgb, material.material_params.x); // roughness
    
    // Normal (octahedral encoding)
    vec3 normal = normalize(inWorldNormal);
    vec2 oct = vec2(normal.x / (abs(normal.x) + abs(normal.y) + abs(normal.z)), 
                    normal.y / (abs(normal.x) + abs(normal.y) + abs(normal.z)));
    if (normal.z < 0.0) {
        oct = (1.0 - abs(oct.yx)) * sign(oct);
    }
    outNormal = vec4(oct * 0.5 + 0.5, 0.0, 1.0);
    
    // Material: metallic, ao, emissive, flags
    outMaterial = vec4(material.material_params.y, material.material_params.z, material.material_params.w, 0.0);
}