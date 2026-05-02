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
#include "procedural_armor_factory/armor_generator.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace procedural_armor_factory {

// Helper: Pack normalize glm::vec4 color to uint32_t RGBA
inline uint32_t pack_color(glm::vec4 color) {
    // Clamp to [0, 1]
    color.r = std::max(0.0f, std::min(1.0f, color.r));
    color.g = std::max(0.0f, std::min(1.0f, color.g));
    color.b = std::max(0.0f, std::min(1.0f, color.b));
    color.a = std::max(0.0f, std::min(1.0f, color.a));

    uint32_t r = static_cast<uint32_t>(color.r * 255.0f);
    uint32_t g = static_cast<uint32_t>(color.g * 255.0f);
    uint32_t b = static_cast<uint32_t>(color.b * 255.0f);
    uint32_t a = static_cast<uint32_t>(color.a * 255.0f);

    return (r << 24) | (g << 16) | (b << 8) | a;
}

ProceduralArmorGenerator::ProceduralArmorGenerator()
    : vertex_offset_(0), index_offset_(0) {
    // Preallocate reasonably sized pools (can be tuned)
    vertex_pool_.reserve(65536);
    index_pool_.reserve(65536 * 3);
}

ProceduralArmorGenerator::~ProceduralArmorGenerator() = default;

std::vector<Vertex>::iterator ProceduralArmorGenerator::allocate_vertices(size_t count) {
    if (vertex_offset_ + count > vertex_pool_.capacity()) {
        return vertex_pool_.end(); // Pool exhausted
    }
    auto it = vertex_pool_.begin() + vertex_offset_;
    vertex_offset_ += count;
    return it;
}

std::vector<uint32_t>::iterator ProceduralArmorGenerator::allocate_indices(size_t count) {
    if (index_offset_ + count > index_pool_.capacity()) {
        return index_pool_.end();
    }
    auto it = index_pool_.begin() + index_offset_;
    index_offset_ += count;
    return it;
}

void ProceduralArmorGenerator::reset_pools() {
    vertex_offset_ = 0;
    index_offset_ = 0;
    vertex_pool_.clear();
    index_pool_.clear();
}

MaterialColor ProceduralArmorGenerator::default_materials() {
    return MaterialColor{
        .hull = glm::vec4(0.15f, 0.25f, 0.15f, 1.0f),       // dark green
        .turret = glm::vec4(0.12f, 0.22f, 0.12f, 1.0f),     // darker green
        .wheels = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f),     // black
        .gun = glm::vec4(0.10f, 0.18f, 0.10f, 1.0f),        // dark steel green
        .details = glm::vec4(0.20f, 0.20f, 0.20f, 1.0f)     // gray
    };
}

ModelInstance ProceduralArmorGenerator::generate(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials
) {
    reset_pools();
    
    ModelInstance instance;
    instance.entity_id = 0; // Will be set by caller
    instance.base_model_id = tmpl.seed;
    instance.current_lod_level = 0;
    instance.render_flags = rendering_engine::RENDER_FLAG_SHADOWS;

    // Generate all components
    generate_hull(tmpl, materials, instance.visual_vertices, instance.visual_indices);
    generate_suspension(tmpl, materials, instance.visual_vertices, instance.visual_indices);
    generate_turret(tmpl, materials, instance.visual_vertices, instance.visual_indices);
    generate_gun(tmpl, materials, instance.visual_vertices, instance.visual_indices);
    generate_details(tmpl, materials, instance.visual_vertices, instance.visual_indices);

    // Compute bounding box
    if (!instance.visual_vertices.empty()) {
        glm::vec3 min = instance.visual_vertices[0].position;
        glm::vec3 max = instance.visual_vertices[0].position;
        for (const auto& v : instance.visual_vertices) {
            min = glm::min(min, v.position);
            max = glm::max(max, v.position);
        }
        instance.world_bounds.min_bounds = _mm_setr_ps(min.x, min.y, min.z, 0.0f);
        instance.world_bounds.max_bounds = _mm_setr_ps(max.x, max.y, max.z, 0.0f);
    }

    // Initialize world transform to identity
    instance.world_transform.row0 = _mm256_setr_ps(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    instance.world_transform.row1 = _mm256_setr_ps(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    instance.world_transform.row2 = _mm256_setr_ps(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    instance.world_transform.row3 = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    return instance;
}

void ProceduralArmorGenerator::generate_hull(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials,
    std::vector<Vertex>& out_vertices,
    std::vector<uint32_t>& out_indices
) {
    const float half_len = tmpl.hull_length * 0.5f;
    const float half_wid = tmpl.hull_width * 0.5f;
    const float half_hei = tmpl.hull_height * 0.5f;

    // Hull front angle: tilt upper vertices backward along X-axis
    float angle_rad = glm::radians(tmpl.hull_front_angle);
    float tilt_offset = half_hei * std::tan(angle_rad);

    uint32_t base_idx = static_cast<uint32_t>(out_vertices.size());

    // Define 8 hull corners with front armor angle
    glm::vec3 hull_corners[8] = {
        {-half_len, -half_wid, -half_hei},              // 0: bottom-back-left
        {half_len, -half_wid, -half_hei},               // 1: bottom-back-right
        {half_len, half_wid, -half_hei},                // 2: top-back-right (back is highest)
        {-half_len, half_wid, -half_hei},               // 3: top-back-left
        {-half_len - tilt_offset, -half_wid, half_hei}, // 4: bottom-front-left (sloped)
        {half_len - tilt_offset, -half_wid, half_hei},  // 5: bottom-front-right
        {half_len - tilt_offset, half_wid, half_hei},   // 6: top-front-right
        {-half_len - tilt_offset, half_wid, half_hei}   // 7: top-front-left
    };

    // Face vertex indices
    const uint32_t face_indices[6][4] = {
        {0, 1, 2, 3}, // back
        {4, 5, 6, 7}, // front
        {0, 3, 7, 4}, // left
        {1, 5, 6, 2}, // right
        {3, 2, 6, 7}, // top
        {0, 4, 5, 1}  // bottom
    };

    // Normal per face
    const glm::vec3 normals[6] = {
        {0.0f, 0.0f, -1.0f},  // back
        {0.0f, 0.0f, 1.0f},   // front
        {-1.0f, 0.0f, 0.0f},  // left
        {1.0f, 0.0f, 0.0f},   // right
        {0.0f, 1.0f, 0.0f},   // top
        {0.0f, -1.0f, 0.0f}   // bottom
    };

    glm::vec4 hull_color = materials.hull;

    // Create 6 faces (2 triangles each)
    for (int face = 0; face < 6; ++face) {
        uint32_t face_base = static_cast<uint32_t>(out_vertices.size());

        for (int i = 0; i < 4; ++i) {
            Vertex v;
            v.position = hull_corners[face_indices[face][i]];
            v.normal = normals[face];
            v.tangent = glm::vec3(0.0f);
            v.texcoord = glm::vec2(i % 2 == 0 ? 0.0f : 1.0f, i < 2 ? 0.0f : 1.0f);
            v.color = pack_color(hull_color);
            out_vertices.push_back(v);
        }

        // Two triangles per quad face
        out_indices.push_back(face_base + 0);
        out_indices.push_back(face_base + 1);
        out_indices.push_back(face_base + 2);
        out_indices.push_back(face_base + 0);
        out_indices.push_back(face_base + 2);
        out_indices.push_back(face_base + 3);
    }

    // Add simple mud guards / fenders on sides
    float fender_height = tmpl.hull_height * 0.3f;
    float fender_overhang = tmpl.hull_width * 0.15f;

    // Left fender
    glm::vec3 fender_verts[4] = {
        {-half_len, -half_wid, half_hei},
        {half_len, -half_wid, half_hei},
        {half_len, -half_wid - fender_overhang, half_hei - fender_height},
        {-half_len, -half_wid - fender_overhang, half_hei - fender_height}
    };

    uint32_t fender_base = static_cast<uint32_t>(out_vertices.size());
    for (int i = 0; i < 4; ++i) {
        Vertex v;
        v.position = fender_verts[i];
        v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v.texcoord = glm::vec2(i % 2, i < 2 ? 0.0f : 1.0f);
        v.color = pack_color(materials.hull);
        out_vertices.push_back(v);
    }

    out_indices.push_back(fender_base + 0);
    out_indices.push_back(fender_base + 1);
    out_indices.push_back(fender_base + 2);
    out_indices.push_back(fender_base + 0);
    out_indices.push_back(fender_base + 2);
    out_indices.push_back(fender_base + 3);
}

void ProceduralArmorGenerator::generate_suspension(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials,
    std::vector<Vertex>& out_vertices,
    std::vector<uint32_t>& out_indices
) {
    const float half_len = tmpl.hull_length * 0.5f;
    const float half_wid = tmpl.hull_width * 0.5f;
    const float wheel_spacing = tmpl.hull_length / (tmpl.road_wheels_count + 1);
    const float wheel_y_offset = half_wid + tmpl.wheel_radius * 0.5f;

    glm::vec4 wheel_color = materials.wheels;
    uint32_t segments = 16;

    // Generate left and right tracks
    for (int side = 0; side < 2; ++side) {
        float y_pos = (side == 0) ? -wheel_y_offset : wheel_y_offset;
        float y_sign = (side == 0) ? -1.0f : 1.0f;

        // Generate road wheels
        for (uint8_t i = 0; i < tmpl.road_wheels_count; ++i) {
            float x_pos = -half_len + (i + 1) * wheel_spacing;

            // Create cylinder for wheel
            float angle_step = 2.0f * glm::pi<float>() / segments;
            uint32_t wheel_base = static_cast<uint32_t>(out_vertices.size());

            // Side vertices
            for (uint32_t j = 0; j <= segments; ++j) {
                float angle = static_cast<float>(j) * angle_step;
                float x = tmpl.wheel_radius * cosf(angle);
                float z = tmpl.wheel_radius * sinf(angle);

                // Left disc
                Vertex v;
                v.position = glm::vec3(x_pos, y_pos, z);
                v.normal = glm::normalize(glm::vec3(0.0f, y_sign, 0.0f));
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.texcoord = glm::vec2(angle / (2.0f * glm::pi<float>()), 0.0f);
                v.color = glm::packUnorm4x8(wheel_color);
                out_vertices.push_back(v);

                // Right disc
                v.position = glm::vec3(x_pos, y_pos + tmpl.track_width * y_sign, z);
                v.texcoord.y = 1.0f;
                out_vertices.push_back(v);
            }

            // Indices for cylinder sides
            for (uint32_t j = 0; j < segments; ++j) {
                uint32_t base0 = wheel_base + j * 2;
                uint32_t base1 = base0 + 2;

                out_indices.push_back(base0);
                out_indices.push_back(base0 + 1);
                out_indices.push_back(base1 + 1);

                out_indices.push_back(base0);
                out_indices.push_back(base1 + 1);
                out_indices.push_back(base1);
            }
        }
    }
}

void ProceduralArmorGenerator::generate_turret(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials,
    std::vector<Vertex>& out_vertices,
    std::vector<uint32_t>& out_indices
) {
    const float turret_radius = tmpl.turret_ring_diameter * 0.5f;
    const float turret_z_base = tmpl.hull_height * 0.5f;

    glm::vec4 turret_color = materials.turret;
    uint32_t segments = 24;
    float angle_step = 2.0f * glm::pi<float>() / segments;

    uint32_t base_idx = static_cast<uint32_t>(out_vertices.size());

    // Cylinder for turret
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float x = turret_radius * cosf(angle);
        float y = turret_radius * sinf(angle);

        // Bottom vertex
        Vertex v_bot;
        v_bot.position = glm::vec3(x, y, turret_z_base);
        v_bot.normal = glm::normalize(glm::vec3(x, y, 0.0f));
        v_bot.tangent = glm::vec3(-sinf(angle), cosf(angle), 0.0f);
        v_bot.texcoord = glm::vec2(angle / (2.0f * glm::pi<float>()), 0.0f);
        v_bot.color = pack_color(turret_color);
        out_vertices.push_back(v_bot);

        // Top vertex
        Vertex v_top;
        v_top.position = glm::vec3(x, y, turret_z_base + tmpl.turret_height);
        v_top.normal = v_bot.normal;
        v_top.tangent = v_bot.tangent;
        v_top.texcoord = glm::vec2(angle / (2.0f * glm::pi<float>()), 1.0f);
        v_top.color = pack_color(turret_color);
        out_vertices.push_back(v_top);
    }

    // Side indices
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t idx0 = base_idx + i * 2;
        uint32_t idx1 = idx0 + 2;

        out_indices.push_back(idx0);
        out_indices.push_back(idx0 + 1);
        out_indices.push_back(idx1 + 1);

        out_indices.push_back(idx0);
        out_indices.push_back(idx1 + 1);
        out_indices.push_back(idx1);
    }

    // Turret roof (simple disc)
    uint32_t roof_center = static_cast<uint32_t>(out_vertices.size());
    Vertex roof_v;
    roof_v.position = glm::vec3(0.0f, 0.0f, turret_z_base + tmpl.turret_height);
    roof_v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    roof_v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    roof_v.texcoord = glm::vec2(0.5f, 0.5f);
    roof_v.color = pack_color(turret_color);
    out_vertices.push_back(roof_v);

    uint32_t roof_start = static_cast<uint32_t>(out_vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float x = turret_radius * cosf(angle);
        float y = turret_radius * sinf(angle);
        Vertex v;
        v.position = glm::vec3(x, y, turret_z_base + tmpl.turret_height);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v.texcoord = glm::vec2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
        v.color = pack_color(turret_color);
        out_vertices.push_back(v);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        out_indices.push_back(roof_center);
        out_indices.push_back(roof_start + i);
        out_indices.push_back(roof_start + i + 1);
    }
}

void ProceduralArmorGenerator::generate_gun(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials,
    std::vector<Vertex>& out_vertices,
    std::vector<uint32_t>& out_indices
) {
    const float turret_z_base = tmpl.hull_height * 0.5f;
    const float gun_radius = tmpl.gun_caliber * 0.0005f; // mm to m, /2 for radius
    const float gun_length = gun_radius * 2.0f * tmpl.gun_length; // calibers to length
    const float gun_z_height = turret_z_base + tmpl.turret_height * 0.6f;

    glm::vec4 gun_color = materials.gun;
    uint32_t segments = 12;
    float angle_step = 2.0f * glm::pi<float>() / segments;

    uint32_t base_idx = static_cast<uint32_t>(out_vertices.size());

    // Barrel cylinder
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float y_off = gun_radius * sinf(angle);
        float z_off = gun_radius * cosf(angle);

        // Breech
        Vertex v_breech;
        v_breech.position = glm::vec3(-gun_length * 0.2f, y_off, gun_z_height + z_off);
        v_breech.normal = glm::normalize(glm::vec3(0.0f, sinf(angle), cosf(angle)));
        v_breech.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v_breech.texcoord = glm::vec2(angle / (2.0f * glm::pi<float>()), 0.0f);
        v_breech.color = pack_color(gun_color);
        out_vertices.push_back(v_breech);

        // Muzzle
        Vertex v_muzzle;
        v_muzzle.position = glm::vec3(gun_length, y_off, gun_z_height + z_off);
        v_muzzle.normal = v_breech.normal;
        v_muzzle.tangent = v_breech.tangent;
        v_muzzle.texcoord = glm::vec2(angle / (2.0f * glm::pi<float>()), 1.0f);
        v_muzzle.color = pack_color(gun_color);
        out_vertices.push_back(v_muzzle);
    }

    // Barrel indices
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t idx0 = base_idx + i * 2;
        uint32_t idx1 = idx0 + 2;

        out_indices.push_back(idx0);
        out_indices.push_back(idx0 + 1);
        out_indices.push_back(idx1 + 1);

        out_indices.push_back(idx0);
        out_indices.push_back(idx1 + 1);
        out_indices.push_back(idx1);
    }

    // Simple muzzle brake (2 perpendicular rectangles at muzzle)
    if (tmpl.era_flags & 0x0F) { // Any era flag set
        float brake_width = gun_radius * 3.0f;
        float brake_depth = gun_radius * 2.0f;

        uint32_t brake_base = static_cast<uint32_t>(out_vertices.size());

        glm::vec3 brake_corners[8] = {
            {gun_length, -brake_width, gun_z_height - brake_depth},
            {gun_length, brake_width, gun_z_height - brake_depth},
            {gun_length, brake_width, gun_z_height + brake_depth},
            {gun_length, -brake_width, gun_z_height + brake_depth},
            {gun_length + gun_radius, -brake_width, gun_z_height - brake_depth},
            {gun_length + gun_radius, brake_width, gun_z_height - brake_depth},
            {gun_length + gun_radius, brake_width, gun_z_height + brake_depth},
            {gun_length + gun_radius, -brake_width, gun_z_height + brake_depth}
        };

        for (const auto& pos : brake_corners) {
            Vertex v;
            v.position = pos;
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            v.texcoord = glm::vec2(0.0f, 0.0f);
            v.color = pack_color(materials.details);
            out_vertices.push_back(v);
        }

        // Simple box faces
        out_indices.insert(out_indices.end(),
            {brake_base + 0, brake_base + 1, brake_base + 5,
             brake_base + 0, brake_base + 5, brake_base + 4});
    }
}

void ProceduralArmorGenerator::generate_details(
    const ParametricTankTemplate& tmpl,
    const MaterialColor& materials,
    std::vector<Vertex>& out_vertices,
    std::vector<uint32_t>& out_indices
) {
    DeterministicRNG rng(tmpl.seed);
    const float half_len = tmpl.hull_length * 0.5f;

    int detail_count = rng.next_int(2, 5);
    glm::vec4 detail_color = materials.details;

    for (int i = 0; i < detail_count; ++i) {
        // Random position on mudguard or deck
        float x = -half_len * 0.3f + rng.next_float() * tmpl.hull_length * 0.4f;
        float y = (i % 2 == 0) ? tmpl.hull_width * 0.6f : -tmpl.hull_width * 0.6f;
        float z = tmpl.hull_height * 0.8f;

        // Small box (fuel can, toolbox, etc.)
        float box_size = 0.15f;
        glm::vec3 box_center = glm::vec3(x, y, z);

        uint32_t box_base = static_cast<uint32_t>(out_vertices.size());
        glm::vec3 box_corners[8] = {
            box_center + glm::vec3(-box_size, -box_size, -box_size),
            box_center + glm::vec3(box_size, -box_size, -box_size),
            box_center + glm::vec3(box_size, box_size, -box_size),
            box_center + glm::vec3(-box_size, box_size, -box_size),
            box_center + glm::vec3(-box_size, -box_size, box_size),
            box_center + glm::vec3(box_size, -box_size, box_size),
            box_center + glm::vec3(box_size, box_size, box_size),
            box_center + glm::vec3(-box_size, box_size, box_size)
        };

        for (const auto& corner : box_corners) {
            Vertex v;
            v.position = corner;
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.texcoord = glm::vec2(0.0f, 0.0f);
            v.color = pack_color(detail_color);
            out_vertices.push_back(v);
        }

        // Simple box: 6 faces, 2 triangles each
        uint32_t faces[6][4] = {
            {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 3, 7, 4},
            {1, 5, 6, 2}, {3, 2, 6, 7}, {0, 4, 5, 1}
        };

        for (int f = 0; f < 6; ++f) {
            out_indices.push_back(box_base + faces[f][0]);
            out_indices.push_back(box_base + faces[f][1]);
            out_indices.push_back(box_base + faces[f][2]);

            out_indices.push_back(box_base + faces[f][0]);
            out_indices.push_back(box_base + faces[f][2]);
            out_indices.push_back(box_base + faces[f][3]);
        }
    }
}

PhysicsMapping ProceduralArmorGenerator::compute_physics_mapping(const ParametricTankTemplate& tmpl) {
    PhysicsMapping mapping;

    mapping.mass = tmpl.weight_tons * 1000.0f;

    // Center of mass: 1/3 height from ground, slightly toward rear
    mapping.center_of_mass = glm::vec3(
        tmpl.hull_length * -0.1f,  // 10% aft bias
        0.0f,
        tmpl.hull_height * 0.35f   // 35% height
    );

    // Inertia tensor (simplified box)
    float lx = tmpl.hull_length;
    float ly = tmpl.hull_width;
    float lz = tmpl.hull_height;
    float m = mapping.mass;

    mapping.inertia_tensor = glm::mat3(
        glm::vec3(m / 12.0f * (ly * ly + lz * lz), 0.0f, 0.0f),
        glm::vec3(0.0f, m / 12.0f * (lx * lx + lz * lz), 0.0f),
        glm::vec3(0.0f, 0.0f, m / 12.0f * (lx * lx + ly * ly))
    );

    // Simplified convex hull: 8 corners + 4 turret corners
    mapping.convex_hull = {
        {-lx * 0.5f, -ly * 0.5f, -lz * 0.5f},
        {lx * 0.5f, -ly * 0.5f, -lz * 0.5f},
        {lx * 0.5f, ly * 0.5f, -lz * 0.5f},
        {-lx * 0.5f, ly * 0.5f, -lz * 0.5f},
        {-lx * 0.5f, -ly * 0.5f, lz * 0.5f},
        {lx * 0.5f, -ly * 0.5f, lz * 0.5f},
        {lx * 0.5f, ly * 0.5f, lz * 0.5f},
        {-lx * 0.5f, ly * 0.5f, lz * 0.5f},
        // Turret points
        {0.0f, -tmpl.turret_ring_diameter * 0.5f, lz * 0.5f + tmpl.turret_height},
        {0.0f, tmpl.turret_ring_diameter * 0.5f, lz * 0.5f + tmpl.turret_height},
        {-tmpl.turret_ring_diameter * 0.5f, 0.0f, lz * 0.5f + tmpl.turret_height},
        {tmpl.turret_ring_diameter * 0.5f, 0.0f, lz * 0.5f + tmpl.turret_height}
    };

    return mapping;
}

std::vector<DamageComponentMapping> ProceduralArmorGenerator::create_damage_mapping(
    const ModelInstance& instance,
    const ParametricTankTemplate& tmpl
) {
    // Simplified: assume vertex ranges for different components
    // In production, mark vertices during generation
    std::vector<DamageComponentMapping> mappings;

    uint32_t total_verts = instance.visual_vertices.size();
    uint32_t quarter = total_verts / 4;

    float hull_health = tmpl.weight_tons * 10.0f;
    float turret_health = hull_health * 0.7f;
    float wheel_health = hull_health * 0.3f;

    // Rough division (ideally, mark components during generation)
    mappings.push_back({
        .component = TankComponent::HULL,
        .health = hull_health,
        .armor_thickness = 80.0f,
        .vertex_start = 0,
        .vertex_count = quarter * 2
    });

    mappings.push_back({
        .component = TankComponent::TURRET,
        .health = turret_health,
        .armor_thickness = 100.0f,
        .vertex_start = quarter * 2,
        .vertex_count = quarter
    });

    mappings.push_back({
        .component = TankComponent::TRACK_L,
        .health = wheel_health,
        .armor_thickness = 0.0f,
        .vertex_start = quarter * 3,
        .vertex_count = quarter / 2
    });

    mappings.push_back({
        .component = TankComponent::TRACK_R,
        .health = wheel_health,
        .armor_thickness = 0.0f,
        .vertex_start = quarter * 3 + quarter / 2,
        .vertex_count = quarter / 2
    });

    return mappings;
}

} // namespace procedural_armor_factory
