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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inColor;

layout(set = 0, binding = 0) uniform ViewConstants {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    mat4 inverse_view;
    mat4 inverse_projection;
    mat4 inverse_view_projection;
    vec4 camera_position;
    vec4 screen_dimensions;
    vec4 time_params;
} view_constants;

layout(push_constant) uniform MaterialPushConstants {
    mat4 model_matrix;
    vec4 albedo;
    vec4 material_params; // roughness, metallic, ao, emissive
} material;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec4 outColor;

void main() {
    vec4 worldPos = material.model_matrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    outWorldNormal = normalize(mat3(material.model_matrix) * inNormal);
    outTexCoord = inTexCoord;
    outColor = inColor * material.albedo;
    gl_Position = view_constants.view_projection * worldPos;
}