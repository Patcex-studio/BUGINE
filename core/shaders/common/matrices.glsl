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
// Common matrix and vector utility functions

#ifndef COMMON_MATRICES_GLSL
#define COMMON_MATRICES_GLSL

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

/**
 * Extract translation from 4x4 matrix
 */
vec3 matrix_translation(mat4 m) {
    return m[3].xyz;
}

/**
 * Extract scale from 4x4 matrix
 */
vec3 matrix_scale(mat4 m) {
    return vec3(
        length(m[0].xyz),
        length(m[1].xyz),
        length(m[2].xyz)
    );
}

/**
 * Create translation matrix
 */
mat4 translate(vec3 t) {
    return mat4(
        vec4(1, 0, 0, 0),
        vec4(0, 1, 0, 0),
        vec4(0, 0, 1, 0),
        vec4(t, 1)
    );
}

/**
 * Create scale matrix
 */
mat4 scale(vec3 s) {
    return mat4(
        vec4(s.x, 0, 0, 0),
        vec4(0, s.y, 0, 0),
        vec4(0, 0, s.z, 0),
        vec4(0, 0, 0, 1)
    );
}

/**
 * Create rotation matrix around X axis
 */
mat4 rotate_x(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        vec4(1, 0, 0, 0),
        vec4(0, c, -s, 0),
        vec4(0, s, c, 0),
        vec4(0, 0, 0, 1)
    );
}

/**
 * Create rotation matrix around Y axis
 */
mat4 rotate_y(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        vec4(c, 0, s, 0),
        vec4(0, 1, 0, 0),
        vec4(-s, 0, c, 0),
        vec4(0, 0, 0, 1)
    );
}

/**
 * Create rotation matrix around Z axis
 */
mat4 rotate_z(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        vec4(c, -s, 0, 0),
        vec4(s, c, 0, 0),
        vec4(0, 0, 1, 0),
        vec4(0, 0, 0, 1)
    );
}

// ============================================================================
// VECTOR OPERATIONS
// ============================================================================

/**
 * Compute tangent-space normal mapping
 */
vec3 calculate_normal_map(
    vec3 normal_sample,
    vec3 normal,
    vec3 tangent,
    vec3 bitangent
) {
    normal_sample = normalize(normal_sample * 2.0 - 1.0);
    
    vec3 nm = normalize(
        normal_sample.x * tangent +
        normal_sample.y * bitangent +
        normal_sample.z * normal
    );
    
    return nm;
}

/**
 * Compute bitangent from normal and tangent
 */
vec3 compute_bitangent(vec3 normal, vec3 tangent) {
    return normalize(cross(normal, tangent));
}

/**
 * Reflect vector around surface normal
 */
vec3 reflect_vector(vec3 incident, vec3 normal) {
    return incident - 2.0 * dot(incident, normal) * normal;
}

/**
 * Refract vector through surface
 */
vec3 refract_vector(vec3 incident, vec3 normal, float eta) {
    float cos_i = -dot(incident, normal);
    float sin_t2 = eta * eta * (1.0 - cos_i * cos_i);
    
    if (sin_t2 > 1.0) return vec3(0.0); // Total internal reflection
    
    float cos_t = sqrt(1.0 - sin_t2);
    return eta * incident + (eta * cos_i - cos_t) * normal;
}

/**
 * Compute parallax mapping offset
 */
vec2 parallax_mapping(sampler2D height_map, vec2 texcoord, vec3 view_dir, float height_scale) {
    float height = texture(height_map, texcoord).r;
    vec2 parallax = view_dir.xy * (height * height_scale) / view_dir.z;
    return texcoord - parallax;
}

#endif // COMMON_MATRICES_GLSL
