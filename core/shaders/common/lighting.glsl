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
// Common lighting calculations shared across all pipelines

#ifndef COMMON_LIGHTING_GLSL
#define COMMON_LIGHTING_GLSL

// Constants
const float PI = 3.141592653589793;
const float EPSILON = 0.0001;

// ============================================================================
// PBR MATERIAL FUNCTIONS
// ============================================================================

/**
 * Compute Fresnel-Schlick approximation
 */
vec3 fresnel_schlick(float cos_theta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

/**
 * Compute Fresnel with roughness (Disney approximation)
 */
vec3 fresnel_schlick_roughness(float cos_theta, vec3 f0, float roughness) {
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

/**
 * Trowbridge-Reitz GGX normal distribution function
 */
float distribution_ggx(vec3 n, vec3 h, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float n_dot_h = max(dot(n, h), 0.0);
    float n_dot_h2 = n_dot_h * n_dot_h;
    
    float nom = a2;
    float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / max(denom, EPSILON);
}

/**
 * Schlick-GGX geometry function
 */
float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
    
    return nom / max(denom, EPSILON);
}

/**
 * Smith's method for geometry
 */
float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
    float n_dot_v = max(dot(n, v), 0.0);
    float n_dot_l = max(dot(n, l), 0.0);
    
    float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);
    
    return ggx1 * ggx2;
}

/**
 * Cook-Torrance BRDF
 */
vec3 cook_torrance_brdf(
    vec3 light_dir,
    vec3 view_dir,
    vec3 normal,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 f0
) {
    vec3 h = normalize(light_dir + view_dir);
    
    float distance = 1.0; // Normalized
    float attenuation = 1.0;
    vec3 radiance = vec3(1.0) * attenuation;
    
    float cos_lo = max(dot(normal, light_dir), 0.0);
    float cos_li = max(dot(normal, view_dir), 0.0);
    
    if (cos_lo < EPSILON || cos_li < EPSILON) {
        return vec3(0.0);
    }
    
    // Fresnel
    vec3 f = fresnel_schlick(max(dot(h, view_dir), 0.0), f0);
    
    // NDF
    float ndf = distribution_ggx(normal, h, roughness);
    
    // Geometry
    float g = geometry_smith(normal, view_dir, light_dir, roughness);
    
    // Specular
    vec3 kd = vec3(1.0) - f;
    kd *= 1.0 - metallic;
    
    vec3 nominator = ndf * g * f;
    float denominator = 4.0 * cos_li * cos_lo;
    vec3 specular = nominator / max(denominator, EPSILON);
    
    // Combine
    vec3 ks = f;
    vec3 result = (kd * albedo / PI + specular) * radiance * cos_lo;
    
    return result;
}

// ============================================================================
// LIGHT ATTENUATION
// ============================================================================

/**
 * Compute point light attenuation with inverse square law
 */
float point_light_attenuation(float distance, float radius) {
    if (distance > radius) return 0.0;
    
    float d = distance / radius;
    float d2 = d * d;
    
    // Smooth falloff: (1 - d^4)^2
    float falloff = (1.0 - d2 * d2);
    falloff = falloff * falloff;
    
    return falloff / (1.0 + distance * distance);
}

/**
 * Compute spot light attenuation with cone
 */
float spot_light_attenuation(vec3 light_dir, vec3 to_light, float inner_angle, float outer_angle) {
    float cos_theta = dot(-to_light, light_dir);
    float cos_inner = cos(inner_angle);
    float cos_outer = cos(outer_angle);
    
    if (cos_theta < cos_outer) return 0.0;
    if (cos_theta > cos_inner) return 1.0;
    
    return smoothstep(cos_outer, cos_inner, cos_theta);
}

// ============================================================================
// NORMAL ENCODING/DECODING (Octahedral)
// ============================================================================

/**
 * Encode normalized normal to 2D octahedral coordinates
 */
vec2 encode_normal_oct(vec3 n) {
    float l1norm = abs(n.x) + abs(n.y) + abs(n.z);
    vec2 result = n.xy * (1.0 / l1norm);
    
    if (n.z < 0.0) {
        vec2 oldResult = result;
        result.x = (1.0 - abs(oldResult.y)) * (oldResult.x >= 0.0 ? 1.0 : -1.0);
        result.y = (1.0 - abs(oldResult.x)) * (oldResult.y >= 0.0 ? 1.0 : -1.0);
    }
    
    return result;
}

/**
 * Decode octahedral 2D coordinates to normalized normal
 */
vec3 decode_normal_oct(vec2 f) {
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    
    if (n.z < 0.0) {
        vec2 oldN = n.xy;
        n.x = (1.0 - abs(oldN.y)) * (oldN.x >= 0.0 ? 1.0 : -1.0);
        n.y = (1.0 - abs(oldN.x)) * (oldN.y >= 0.0 ? 1.0 : -1.0);
    }
    
    return normalize(n);
}

// ============================================================================
// TONE MAPPING (ACES)
// ============================================================================

/**
 * ACES filmic tone mapping
 */
vec3 aces_tone_mapping(vec3 hdr_color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    
    return clamp((hdr_color * (a * hdr_color + vec3(b))) / 
                 (hdr_color * (c * hdr_color + vec3(d)) + vec3(e)), 0.0, 1.0);
}

// ============================================================================
// SCREEN-SPACE FUNCTIONS
// ============================================================================

/**
 * Linearize depth from NDC space
 */
float linearize_depth(float depth, float near, float far) {
    return (2.0 * near * far) / (far + near - (2.0 * depth - 1.0) * (far - near));
}

/**
 * Get view-space position from depth
 */
vec3 get_view_position(vec2 uv, float depth, mat4 inverse_projection) {
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view_space = inverse_projection * clip_space;
    return view_space.xyz / view_space.w;
}

#endif // COMMON_LIGHTING_GLSL
