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

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D albedoRoughness;
layout(set = 0, binding = 1) uniform sampler2D normal;
layout(set = 0, binding = 2) uniform sampler2D material;
layout(set = 0, binding = 3) uniform sampler2D depth;

// Light data structure matching C++ LightData with correct field order
// C++ struct alignas(16) {
//   vec3 position; float radius;
//   vec3 color; float intensity;  
//   uint light_type;
//   vec3 direction; float inner_angle;
//   float outer_angle; 
//   mat4 shadow_projection;
// };
// In GLSL std430, we pack fields into aligned chunks
struct LightData {
    // Offset 0-15: position (vec3) + radius (float)
    vec4 position_radius;
    // Offset 16-31: color (vec3) + intensity (float)
    vec4 color_intensity;
    // Offset 32-47: light_type (uint) + padding + direction(vec3) split
    uint light_type;
    float dummy1;  // padding
    float dummy2;  // padding
    float dummy3;  // padding to reach offset 48
    // Offset 48-63: direction (vec3) + inner_angle (float)
    vec3 direction;
    float inner_angle;
    // Offset 64-79: outer_angle (float) + padding for mat4 alignment
    float outer_angle;
    float dummy4;
    float dummy5;
    float dummy6;
    // Offset 80+: shadow projection (mat4, but should start at correct offset)
    mat4 shadow_projection;
};

layout(std430, set = 0, binding = 4) buffer LightBuffer {
    LightData lights[];
} lightBuffer;

layout(set = 0, binding = 5) uniform ViewConstants {
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

layout(set = 0, binding = 6) uniform LightingConstants {
    vec4 ambient_color;
    uint light_count;
    uint _pad0, _pad1, _pad2;
} lightingConsts;

layout(location = 0) out vec4 outColor;

vec3 decode_normal(vec2 oct) {
    vec3 v = vec3(oct, 1.0 - abs(oct.x) - abs(oct.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * sign(v.xy);
    }
    return normalize(v);
}

const uint LIGHT_TYPE_POINT = 0u;
const uint LIGHT_TYPE_SPOT = 1u;
const uint LIGHT_TYPE_DIRECTIONAL = 2u;

void main() {
    vec4 albedo_roughness = texture(albedoRoughness, inUV);
    vec4 normal_encoded = texture(normal, inUV);
    vec4 material_data = texture(material, inUV);
    float depthValue = texture(depth, inUV).r;
    
    vec3 albedo = albedo_roughness.rgb;
    float roughness = albedo_roughness.a;
    vec3 normal = decode_normal(normal_encoded.rg * 2.0 - 1.0);
    float metallic = material_data.r;
    float ao = material_data.g;
    float emissive = material_data.b;
    
    // Reconstruct world position
    float ndcDepth = depthValue * 2.0 - 1.0;
    vec4 clip_pos = vec4(inUV * 2.0 - 1.0, ndcDepth, 1.0);
    vec4 view_pos = view_constants.inverse_projection * clip_pos;
    view_pos /= view_pos.w;
    vec4 world_pos = view_constants.inverse_view * view_pos;
    
    vec3 view_dir = normalize(view_constants.camera_position.xyz - world_pos.xyz);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    
    vec3 color = lightingConsts.ambient_color.rgb;
    
    // Iterate over all lights
    uint active_lights = min(lightingConsts.light_count, 256u); // Max reasonable number
    for (uint i = 0u; i < active_lights; ++i) {
        LightData light = lightBuffer.lights[i];
        
        vec3 light_color = light.color_intensity.rgb;
        float intensity = light.color_intensity.w;
        
        vec3 light_dir = vec3(0.0);
        float attenuation = 1.0;
        
        if (light.light_type == LIGHT_TYPE_DIRECTIONAL) {
            // Directional light
            light_dir = normalize(-light.direction.xyz);
            attenuation = 1.0;
        } else if (light.light_type == LIGHT_TYPE_POINT) {
            // Point light
            vec3 light_pos = light.position_radius.xyz;
            vec3 to_light = light_pos - world_pos.xyz;
            float dist = length(to_light);
            
            // Check if within radius
            if (dist > light.position_radius.w) {
                continue;
            }
            
            light_dir = to_light / dist;
            // Inverse square law with smooth falloff
            attenuation = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist);
        } else if (light.light_type == LIGHT_TYPE_SPOT) {
            // Spot light
            vec3 light_pos = light.position_radius.xyz;
            vec3 to_light = light_pos - world_pos.xyz;
            float dist = length(to_light);
            
            // Check if within radius
            if (dist > light.position_radius.w) {
                continue;
            }
            
            light_dir = to_light / dist;
            
            // Compute spot cone attenuation
            vec3 light_forward = -light.direction.xyz;
            float cos_angle = dot(light_dir, light_forward);
            float inner = cos(light.inner_angle);
            float outer = cos(light.outer_angle);
            
            if (cos_angle < outer) {
                continue; // Outside cone
            }
            
            // Smooth falloff between inner and outer cone
            float cone_attenuation = smoothstep(outer, inner, cos_angle);
            
            // Distance attenuation
            float dist_attenuation = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist);
            attenuation = cone_attenuation * dist_attenuation;
        }
        
        // PBR calculation
        vec3 h = normalize(view_dir + light_dir);
        float ndotl = max(dot(normal, light_dir), 0.0);
        float ndotv = max(dot(normal, view_dir), 0.0);
        float ndoth = max(dot(normal, h), 0.0);
        float vdoth = max(dot(view_dir, h), 0.0);
        
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        float d = ndoth * ndoth * (alpha2 - 1.0) + 1.0;
        float D = alpha2 / (3.14159 * d * d);
        
        float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
        float G = ndotv / (ndotv * (1.0 - k) + k) * ndotl / (ndotl * (1.0 - k) + k);
        
        vec3 F = f0 + (1.0 - f0) * pow(1.0 - vdoth, 5.0);
        
        vec3 specular = D * G * F / (4.0 * ndotv * ndotl + 0.001);
        vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / 3.14159;
        
        color += (diffuse + specular) * light_color * intensity * ndotl * attenuation;
    }
    
    // Apply ambient occlusion and emissive
    color *= ao;
    color += albedo * emissive;
    
    outColor = vec4(color, 1.0);
}