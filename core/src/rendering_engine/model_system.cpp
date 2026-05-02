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
#include "rendering_engine/model_system.h"
#include "rendering_engine/rendering_engine.h"
#include "physics_core/physics_core.h"
#include <algorithm>
#include <array>
#include <execution>
#include <numeric>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering_engine {

// ============================================================================
// PROCEDURAL GENERATOR IMPLEMENTATION
// ============================================================================

ProceduralGenerator::ProceduralGenerator() {
    // Initialize SIMD constants if needed
}

ProceduralGenerator::~ProceduralGenerator() {
    // Cleanup
}

bool ProceduralGenerator::generate_vehicle_from_blueprint(
    const VehicleBlueprint& blueprint,
    const std::vector<MaterialOverride>& customization,
    ModelInstance& output_instance
) {
    // Validate blueprint first
    if (!validate_blueprint(blueprint)) {
        return false;
    }

    // Initialize output instance
    output_instance.entity_id = 0; // Will be set by caller
    output_instance.base_model_id = static_cast<AssetID>(blueprint.vehicle_type + 1);
    output_instance.current_lod_level = 0;
    output_instance.render_flags = 0;

    // Apply hierarchical transforms
    apply_hierarchical_transforms(blueprint, output_instance);

    // Generate collision mesh
    generate_collision_mesh(blueprint, output_instance);

    // Apply material customization
    apply_material_customization(customization, output_instance);

    // Compute world bounds
    output_instance.world_bounds = compute_world_bounds(output_instance);

    return true;
}

bool ProceduralGenerator::validate_blueprint(const VehicleBlueprint& blueprint) const {
    // Check basic validity
    if (blueprint.components.empty()) {
        return false;
    }

    // Validate component connections
    for (const auto& connection : blueprint.connections) {
        if (connection.parent_socket_id >= 8 || connection.child_socket_id >= 8) {
            return false;
        }
    }

    // Check mass constraints
    if (blueprint.mass_total <= 0.0f) {
        return false;
    }

    return true;
}

void ProceduralGenerator::create_cube_primitive(
    const glm::vec3& size,
    std::vector<Vertex>& output_vertices,
    std::vector<uint32_t>& output_indices
) {
    const float half_x = size.x * 0.5f;
    const float half_y = size.y * 0.5f;
    const float half_z = size.z * 0.5f;

    // Define cube vertices (8 corners)
    const glm::vec3 positions[8] = {
        {-half_x, -half_y, -half_z}, // 0: bottom-back-left
        { half_x, -half_y, -half_z}, // 1: bottom-back-right
        { half_x,  half_y, -half_z}, // 2: top-back-right
        {-half_x,  half_y, -half_z}, // 3: top-back-left
        {-half_x, -half_y,  half_z}, // 4: bottom-front-left
        { half_x, -half_y,  half_z}, // 5: bottom-front-right
        { half_x,  half_y,  half_z}, // 6: top-front-right
        {-half_x,  half_y,  half_z}  // 7: top-front-left
    };

    // Face vertex indices (quads)
    const uint32_t face_indices[6][4] = {
        {0, 1, 2, 3}, // back
        {4, 5, 6, 7}, // front
        {0, 3, 7, 4}, // left
        {1, 5, 6, 2}, // right
        {3, 2, 6, 7}, // top
        {0, 4, 5, 1}  // bottom
    };

    // Define normals for each face
    const glm::vec3 normals[6] = {
        { 0.0f,  0.0f, -1.0f}, // back
        { 0.0f,  0.0f,  1.0f}, // front
        {-1.0f,  0.0f,  0.0f}, // left
        { 1.0f,  0.0f,  0.0f}, // right
        { 0.0f,  1.0f,  0.0f}, // top
        { 0.0f, -1.0f,  0.0f}  // bottom
    };

    // UV coordinates for each face
    const glm::vec2 uvs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    output_vertices.clear();
    output_indices.clear();

    // Create vertices for each face
    for (int face = 0; face < 6; ++face) {
        uint32_t base_index = static_cast<uint32_t>(output_vertices.size());

        // Face vertices with position, normal, and UV
        for (int i = 0; i < 4; ++i) {
            Vertex v;
            v.position = positions[face_indices[face][i]];
            v.normal = normals[face];
            v.tangent = glm::vec3(0.0f); // Not calculated for simplicity
            v.texcoord = uvs[i];
            v.color = 0xFFFFFFFF; // White
            output_vertices.push_back(v);
        }

        // Two triangles per face
        output_indices.push_back(base_index + 0);
        output_indices.push_back(base_index + 1);
        output_indices.push_back(base_index + 2);
        output_indices.push_back(base_index + 0);
        output_indices.push_back(base_index + 2);
        output_indices.push_back(base_index + 3);
    }
}

void ProceduralGenerator::create_cylinder_primitive(
    float radius,
    float height,
    uint32_t segments,
    std::vector<Vertex>& output_vertices,
    std::vector<uint32_t>& output_indices
) {
    const float half_height = height * 0.5f;
    const float angle_step = 2.0f * glm::pi<float>() / static_cast<float>(segments);

    output_vertices.clear();
    output_indices.clear();

    // Generate side vertices
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        // Bottom vertex
        Vertex bottom;
        bottom.position = glm::vec3(x, -half_height, z);
        bottom.normal = glm::normalize(glm::vec3(x, 0.0f, z));
        bottom.tangent = glm::vec3(0.0f);
        bottom.texcoord = glm::vec2(static_cast<float>(i) / segments, 0.0f);
        bottom.color = 0xFFFFFFFF;
        output_vertices.push_back(bottom);

        // Top vertex
        Vertex top;
        top.position = glm::vec3(x, half_height, z);
        top.normal = glm::normalize(glm::vec3(x, 0.0f, z));
        top.tangent = glm::vec3(0.0f);
        top.texcoord = glm::vec2(static_cast<float>(i) / segments, 1.0f);
        top.color = 0xFFFFFFFF;
        output_vertices.push_back(top);
    }

    // Generate side indices
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        // Triangle 1
        output_indices.push_back(base);
        output_indices.push_back(base + 1);
        output_indices.push_back(base + 2);
        // Triangle 2
        output_indices.push_back(base + 1);
        output_indices.push_back(base + 3);
        output_indices.push_back(base + 2);
    }

    // Generate caps
    uint32_t center_bottom = static_cast<uint32_t>(output_vertices.size());
    Vertex bottom_center;
    bottom_center.position = glm::vec3(0.0f, -half_height, 0.0f);
    bottom_center.normal = glm::vec3(0.0f, -1.0f, 0.0f);
    bottom_center.tangent = glm::vec3(0.0f);
    bottom_center.texcoord = glm::vec2(0.5f, 0.5f);
    bottom_center.color = 0xFFFFFFFF;
    output_vertices.push_back(bottom_center);

    uint32_t center_top = static_cast<uint32_t>(output_vertices.size());
    Vertex top_center;
    top_center.position = glm::vec3(0.0f, half_height, 0.0f);
    top_center.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    top_center.tangent = glm::vec3(0.0f);
    top_center.texcoord = glm::vec2(0.5f, 0.5f);
    top_center.color = 0xFFFFFFFF;
    output_vertices.push_back(top_center);

    // Bottom cap vertices
    uint32_t bottom_start = static_cast<uint32_t>(output_vertices.size());
    for (uint32_t i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        Vertex v;
        v.position = glm::vec3(radius * cosf(angle), -half_height, radius * sinf(angle));
        v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        v.tangent = glm::vec3(0.0f);
        v.texcoord = glm::vec2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
        v.color = 0xFFFFFFFF;
        output_vertices.push_back(v);
    }

    // Top cap vertices
    uint32_t top_start = static_cast<uint32_t>(output_vertices.size());
    for (uint32_t i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        Vertex v;
        v.position = glm::vec3(radius * cosf(angle), half_height, radius * sinf(angle));
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.tangent = glm::vec3(0.0f);
        v.texcoord = glm::vec2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
        v.color = 0xFFFFFFFF;
        output_vertices.push_back(v);
    }

    // Bottom cap indices
    for (uint32_t i = 0; i < segments; ++i) {
        output_indices.push_back(center_bottom);
        output_indices.push_back(bottom_start + i);
        output_indices.push_back(bottom_start + (i + 1) % segments);
    }

    // Top cap indices
    for (uint32_t i = 0; i < segments; ++i) {
        output_indices.push_back(center_top);
        output_indices.push_back(top_start + (i + 1) % segments);
        output_indices.push_back(top_start + i);
    }
}

void ProceduralGenerator::create_sphere_primitive(
    float radius,
    uint32_t segments,
    std::vector<Vertex>& output_vertices,
    std::vector<uint32_t>& output_indices
) {
    output_vertices.clear();
    output_indices.clear();

    const uint32_t stacks = segments;
    const uint32_t slices = segments;

    // Generate vertices
    for (uint32_t stack = 0; stack <= stacks; ++stack) {
        float phi = glm::pi<float>() * static_cast<float>(stack) / stacks;
        float y = radius * cosf(phi);
        float r = radius * sinf(phi);

        for (uint32_t slice = 0; slice <= slices; ++slice) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(slice) / slices;
            float x = r * cosf(theta);
            float z = r * sinf(theta);

            Vertex v;
            v.position = glm::vec3(x, y, z);
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.tangent = glm::vec3(0.0f);
            v.texcoord = glm::vec2(static_cast<float>(slice) / slices, static_cast<float>(stack) / stacks);
            v.color = 0xFFFFFFFF;
            output_vertices.push_back(v);
        }
    }

    // Generate indices
    for (uint32_t stack = 0; stack < stacks; ++stack) {
        for (uint32_t slice = 0; slice < slices; ++slice) {
            uint32_t first = (stack * (slices + 1)) + slice;
            uint32_t second = first + slices + 1;

            // Triangle 1
            output_indices.push_back(first);
            output_indices.push_back(second);
            output_indices.push_back(first + 1);

            // Triangle 2
            output_indices.push_back(second);
            output_indices.push_back(second + 1);
            output_indices.push_back(first + 1);
        }
    }
}

namespace {

struct SimpleMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

static uint32_t pack_color(const glm::vec4& color) {
    uint32_t r = static_cast<uint32_t>(glm::clamp(color.r * 255.0f, 0.0f, 255.0f));
    uint32_t g = static_cast<uint32_t>(glm::clamp(color.g * 255.0f, 0.0f, 255.0f));
    uint32_t b = static_cast<uint32_t>(glm::clamp(color.b * 255.0f, 0.0f, 255.0f));
    uint32_t a = static_cast<uint32_t>(glm::clamp(color.a * 255.0f, 0.0f, 255.0f));
    return (a << 24u) | (b << 16u) | (g << 8u) | r;
}

static uint32_t default_component_color(uint32_t component_type) {
    switch (component_type) {
        case 0: return pack_color(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); // HULL
        case 1: return pack_color(glm::vec4(0.5f, 0.5f, 0.7f, 1.0f)); // TURRET
        case 2: return pack_color(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f)); // GUN
        case 3: return pack_color(glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)); // ENGINE
        default: return pack_color(glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
    }
}

static glm::mat4 build_local_matrix(const __m256& local_transform) {
    alignas(32) float raw[8] = {};
    _mm256_store_ps(raw, local_transform);

    glm::vec3 position(raw[0], raw[1], raw[2]);
    float scale = 1.0f;
    if (raw[3] > 0.0f && raw[3] != 1.0f) {
        scale = raw[3];
    }

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 quat(raw[4], raw[5], raw[6], raw[7]);
    float len2 = quat.x * quat.x + quat.y * quat.y + quat.z * quat.z + quat.w * quat.w;
    if (len2 > 1e-6f) {
        rotation = glm::normalize(glm::quat(quat.w, quat.x, quat.y, quat.z));
    }

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
    transform = transform * glm::mat4_cast(rotation);
    transform = glm::scale(transform, glm::vec3(scale));
    return transform;
}

static void transform_aabb(const glm::mat4& transform, const glm::vec3& local_min, const glm::vec3& local_max,
                           glm::vec3& out_min, glm::vec3& out_max) {
    glm::vec3 corners[8] = {
        {local_min.x, local_min.y, local_min.z},
        {local_min.x, local_min.y, local_max.z},
        {local_min.x, local_max.y, local_min.z},
        {local_min.x, local_max.y, local_max.z},
        {local_max.x, local_min.y, local_min.z},
        {local_max.x, local_min.y, local_max.z},
        {local_max.x, local_max.y, local_min.z},
        {local_max.x, local_max.y, local_max.z}
    };

    out_min = glm::vec3(std::numeric_limits<float>::infinity());
    out_max = glm::vec3(-std::numeric_limits<float>::infinity());

    for (const auto& corner : corners) {
        glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
        out_min = glm::min(out_min, glm::vec3(transformed));
        out_max = glm::max(out_max, glm::vec3(transformed));
    }
}

static BoundingBox build_bounds_from_vertices(const std::vector<Vertex>& vertices) {
    BoundingBox bounds;
    if (vertices.empty()) {
        bounds.min_bounds = _mm_setzero_ps();
        bounds.max_bounds = _mm_setzero_ps();
        return bounds;
    }

    glm::vec3 min_point(std::numeric_limits<float>::infinity());
    glm::vec3 max_point(-std::numeric_limits<float>::infinity());

    for (const auto& vertex : vertices) {
        min_point = glm::min(min_point, vertex.position);
        max_point = glm::max(max_point, vertex.position);
    }

    bounds.min_bounds = _mm_setr_ps(min_point.x, min_point.y, min_point.z, 0.0f);
    bounds.max_bounds = _mm_setr_ps(max_point.x, max_point.y, max_point.z, 0.0f);
    return bounds;
}

static BoundingBox build_bounds_from_component_transforms(const ModelInstance& instance) {
    BoundingBox bounds;
    bool first = true;
    const glm::vec3 local_min(-0.5f, -0.5f, -0.5f);
    const glm::vec3 local_max(0.5f, 0.5f, 0.5f);

    for (const auto& world_transform : instance.component_world_transforms) {
        glm::vec3 component_min;
        glm::vec3 component_max;
        transform_aabb(world_transform, local_min, local_max, component_min, component_max);

        if (first) {
            first = false;
            bounds.min_bounds = _mm_setr_ps(component_min.x, component_min.y, component_min.z, 0.0f);
            bounds.max_bounds = _mm_setr_ps(component_max.x, component_max.y, component_max.z, 0.0f);
        } else {
            float current_min_raw[4];
            float current_max_raw[4];
            _mm_storeu_ps(current_min_raw, bounds.min_bounds);
            _mm_storeu_ps(current_max_raw, bounds.max_bounds);
            glm::vec3 current_min_vec(current_min_raw[0], current_min_raw[1], current_min_raw[2]);
            glm::vec3 current_max_vec(current_max_raw[0], current_max_raw[1], current_max_raw[2]);
            current_min_vec = glm::min(current_min_vec, component_min);
            current_max_vec = glm::max(current_max_vec, component_max);
            bounds.min_bounds = _mm_setr_ps(current_min_vec.x, current_min_vec.y, current_min_vec.z, 0.0f);
            bounds.max_bounds = _mm_setr_ps(current_max_vec.x, current_max_vec.y, current_max_vec.z, 0.0f);
        }
    }

    if (first) {
        bounds.min_bounds = _mm_setzero_ps();
        bounds.max_bounds = _mm_setzero_ps();
    }

    return bounds;
}

} // namespace

void ProceduralGenerator::apply_hierarchical_transforms(
    const VehicleBlueprint& blueprint,
    ModelInstance& instance
) {
    const size_t component_count = blueprint.components.size();
    if (component_count == 0) {
        instance.component_world_transforms.clear();
        return;
    }

    std::array<int32_t, 8> socket_to_component;
    socket_to_component.fill(-1);
    for (size_t component_index = 0; component_index < component_count; ++component_index) {
        const auto& component = blueprint.components[component_index];
        for (uint32_t socket_id : component.attachment_points) {
            if (socket_id < socket_to_component.size() && socket_to_component[socket_id] == -1) {
                socket_to_component[socket_id] = static_cast<int32_t>(component_index);
            }
        }
    }

    std::vector<int32_t> parent_index(component_count, -1);
    std::vector<glm::mat4> connection_transform(component_count, glm::mat4(1.0f));
    std::vector<std::vector<size_t>> children(component_count);

    for (const auto& connection : blueprint.connections) {
        if (connection.parent_socket_id >= socket_to_component.size() ||
            connection.child_socket_id >= socket_to_component.size()) {
            continue;
        }

        int32_t parent_component = socket_to_component[connection.parent_socket_id];
        int32_t child_component = socket_to_component[connection.child_socket_id];
        if (parent_component < 0 || child_component < 0) {
            continue;
        }

        parent_index[child_component] = parent_component;
        children[parent_component].push_back(static_cast<size_t>(child_component));
        connection_transform[child_component] = connection.relative_transform;
    }

    size_t root_index = SIZE_MAX;
    for (size_t i = 0; i < component_count; ++i) {
        if (blueprint.components[i].component_type == 0) {
            root_index = i;
            break;
        }
    }
    if (root_index == SIZE_MAX) {
        for (size_t i = 0; i < component_count; ++i) {
            if (parent_index[i] < 0) {
                root_index = i;
                break;
            }
        }
    }
    if (root_index == SIZE_MAX) {
        root_index = 0u;
    }

    instance.component_world_transforms.assign(component_count, glm::mat4(1.0f));

    auto compute_world = [&](auto&& self, size_t index, const glm::mat4& parent_world)->void {
        const glm::mat4 local_transform = build_local_matrix(blueprint.components[index].local_transform);
        const glm::mat4 world_transform = parent_world * connection_transform[index] * local_transform;
        instance.component_world_transforms[index] = world_transform;

        for (size_t child_index : children[index]) {
            self(self, child_index, world_transform);
        }
    };

    compute_world(compute_world, root_index, glm::mat4(1.0f));

    for (size_t i = 0; i < component_count; ++i) {
        if (parent_index[i] < 0 && i != root_index) {
            compute_world(compute_world, i, glm::mat4(1.0f));
        }
    }

    const glm::mat4 root_world = instance.component_world_transforms[root_index];
    memcpy(&instance.world_transform.f32[0], &root_world, 16 * sizeof(float));
}

static SimpleMesh create_box_mesh(const glm::vec3& extents, uint32_t color) {
    SimpleMesh mesh;
    glm::vec3 half = extents * 0.5f;
    glm::vec3 corners[8] = {
        {-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z}, {half.x, half.y, -half.z}, {-half.x, half.y, -half.z},
        {-half.x, -half.y, half.z}, {half.x, -half.y, half.z}, {half.x, half.y, half.z}, {-half.x, half.y, half.z}
    };
    uint32_t face_indices[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,4,7, 7,3,0,
        1,5,6, 6,2,1,
        0,1,5, 5,4,0,
        3,2,6, 6,7,3
    };
    mesh.vertices.reserve(24);
    for (int i = 0; i < 8; ++i) {
        Vertex v;
        v.position = corners[i];
        v.normal = glm::normalize(corners[i] != glm::vec3(0.0f) ? corners[i] : glm::vec3(0.0f, 1.0f, 0.0f));
        v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v.texcoord = glm::vec2(0.0f);
        v.color = color;
        mesh.vertices.push_back(v);
    }
    mesh.indices.insert(mesh.indices.end(), std::begin(face_indices), std::end(face_indices));
    return mesh;
}

static SimpleMesh create_cylinder_mesh(float radius, float height, uint32_t color, uint32_t segments = 16) {
    SimpleMesh mesh;
    float half_height = height * 0.5f;
    mesh.vertices.reserve(segments * 2 + 2);

    Vertex top_center = {{0.0f, half_height, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.5f}, color};
    Vertex bottom_center = {{0.0f, -half_height, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.5f}, color};
    mesh.vertices.push_back(top_center);
    mesh.vertices.push_back(bottom_center);

    for (uint32_t i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(segments) * glm::pi<float>() * 2.0f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
        Vertex top = {{x, half_height, z}, normal, glm::vec3(-z, 0.0f, x), {static_cast<float>(i) / segments, 1.0f}, color};
        Vertex bottom = {{x, -half_height, z}, normal, glm::vec3(-z, 0.0f, x), {static_cast<float>(i) / segments, 0.0f}, color};
        mesh.vertices.push_back(top);
        mesh.vertices.push_back(bottom);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t next = (i + 1) % segments;
        uint32_t top_index = 2 + i * 2;
        uint32_t bottom_index = top_index + 1;
        uint32_t next_top_index = 2 + next * 2;
        uint32_t next_bottom_index = next_top_index + 1;

        // side quad
        mesh.indices.push_back(top_index);
        mesh.indices.push_back(bottom_index);
        mesh.indices.push_back(next_top_index);
        mesh.indices.push_back(next_top_index);
        mesh.indices.push_back(bottom_index);
        mesh.indices.push_back(next_bottom_index);

        // top cap
        mesh.indices.push_back(0);
        mesh.indices.push_back(next_top_index);
        mesh.indices.push_back(top_index);

        // bottom cap
        mesh.indices.push_back(1);
        mesh.indices.push_back(bottom_index);
        mesh.indices.push_back(next_bottom_index);
    }

    return mesh;
}

static SimpleMesh clone_mesh_for_collision(const SimpleMesh& source) {
    SimpleMesh result;
    result.vertices = source.vertices;
    result.indices = source.indices;
    return result;
}

static SimpleMesh transform_mesh(const SimpleMesh& source, const glm::mat4& transform) {
    SimpleMesh result;
    result.vertices.reserve(source.vertices.size());
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    for (const auto& vertex : source.vertices) {
        Vertex transformed = vertex;
        glm::vec4 world_position = transform * glm::vec4(vertex.position, 1.0f);
        transformed.position = glm::vec3(world_position);
        transformed.normal = glm::normalize(normal_matrix * vertex.normal);
        result.vertices.push_back(transformed);
    }

    result.indices = source.indices;
    return result;
}

static SimpleMesh create_component_mesh(const ComponentBlueprint& component, uint32_t color) {
    alignas(32) float raw[8] = {};
    _mm256_store_ps(raw, component.local_transform);
    float scale = (raw[3] > 0.0f) ? raw[3] : 1.0f;

    switch (component.component_type) {
        case 0: // HULL
            return create_box_mesh(glm::vec3(2.0f, 0.8f, 1.2f) * scale, color);
        case 1: // TURRET
            return create_cylinder_mesh(0.5f * scale, 0.4f * scale, color, 16);
        case 2: // GUN
            return create_box_mesh(glm::vec3(1.4f, 0.15f, 0.15f) * scale, color);
        case 3: // ENGINE
            return create_box_mesh(glm::vec3(1.0f, 0.5f, 0.8f) * scale, color);
        default:
            return create_box_mesh(glm::vec3(1.0f, 1.0f, 1.0f) * scale, color);
    }
}

static void append_mesh(SimpleMesh& destination, const SimpleMesh& source) {
    uint32_t base_index = static_cast<uint32_t>(destination.vertices.size());
    destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
    destination.indices.reserve(destination.indices.size() + source.indices.size());
    for (uint32_t index : source.indices) {
        destination.indices.push_back(base_index + index);
    }
}

static CollisionBVH build_collision_bvh(const std::vector<glm::mat4>& component_transforms) {
    CollisionBVH bvh;
    if (component_transforms.empty()) {
        bvh.root_index = 0;
        return bvh;
    }

    // Create leaf nodes for each component
    std::vector<BVHNode> leaves;
    leaves.reserve(component_transforms.size());

    const glm::vec3 local_min(-0.5f, -0.5f, -0.5f);
    const glm::vec3 local_max(0.5f, 0.5f, 0.5f);

    for (size_t i = 0; i < component_transforms.size(); ++i) {
        glm::vec3 component_min, component_max;
        transform_aabb(component_transforms[i], local_min, local_max, component_min, component_max);

        BVHNode leaf;
        leaf.bounds.min_bounds = _mm_setr_ps(component_min.x, component_min.y, component_min.z, 0.0f);
        leaf.bounds.max_bounds = _mm_setr_ps(component_max.x, component_max.y, component_max.z, 0.0f);
        leaf.left_child = static_cast<uint32_t>(-1);
        leaf.right_child = static_cast<uint32_t>(-1);
        leaf.component_index = static_cast<uint32_t>(i);
        leaves.push_back(leaf);
    }

    // Simple binary tree construction (not optimal, but functional)
    bvh.nodes.reserve(leaves.size() * 2 - 1);
    bvh.nodes.insert(bvh.nodes.end(), leaves.begin(), leaves.end());

    // Build internal nodes by pairing leaves
    size_t current_level_start = 0;
    size_t current_level_size = leaves.size();

    while (current_level_size > 1) {
        size_t next_level_start = bvh.nodes.size();
        size_t pairs = current_level_size / 2;

        for (size_t i = 0; i < pairs; ++i) {
            size_t left_idx = current_level_start + i * 2;
            size_t right_idx = current_level_start + i * 2 + 1;

            BVHNode internal;
            // Merge bounds
            float left_min[4], left_max[4], right_min[4], right_max[4];
            _mm_storeu_ps(left_min, bvh.nodes[left_idx].bounds.min_bounds);
            _mm_storeu_ps(left_max, bvh.nodes[left_idx].bounds.max_bounds);
            _mm_storeu_ps(right_min, bvh.nodes[right_idx].bounds.min_bounds);
            _mm_storeu_ps(right_max, bvh.nodes[right_idx].bounds.max_bounds);

            glm::vec3 merged_min(
                std::min(left_min[0], right_min[0]),
                std::min(left_min[1], right_min[1]),
                std::min(left_min[2], right_min[2])
            );
            glm::vec3 merged_max(
                std::max(left_max[0], right_max[0]),
                std::max(left_max[1], right_max[1]),
                std::max(left_max[2], right_max[2])
            );

internal.bounds.min_bounds = _mm_setr_ps(merged_min.x, merged_min.y, merged_min.z, 0.0f);
    internal.bounds.max_bounds = _mm_setr_ps(merged_max.x, merged_max.y, merged_max.z, 0.0f);
            internal.left_child = static_cast<uint32_t>(left_idx);
            internal.right_child = static_cast<uint32_t>(right_idx);
            internal.component_index = static_cast<uint32_t>(-1);

            bvh.nodes.push_back(internal);
        }

        // Handle odd node if present
        if (current_level_size % 2 == 1) {
            size_t last_idx = current_level_start + current_level_size - 1;
            BVHNode internal = bvh.nodes[last_idx];
            internal.left_child = static_cast<uint32_t>(last_idx);
            internal.right_child = static_cast<uint32_t>(-1);
            bvh.nodes.push_back(internal);
        }

        current_level_start = next_level_start;
        current_level_size = bvh.nodes.size() - next_level_start;
    }

    bvh.root_index = static_cast<uint32_t>(bvh.nodes.size() - 1);
    return bvh;
}

void ProceduralGenerator::generate_collision_mesh(
    const VehicleBlueprint& blueprint,
    ModelInstance& instance
) {
    instance.visual_vertices.clear();
    instance.visual_indices.clear();
    instance.collision_vertices.clear();
    instance.collision_indices.clear();
    instance.collision_bvh.nodes.clear();

    if (blueprint.components.empty() || instance.component_world_transforms.size() != blueprint.components.size()) {
        return;
    }

    SimpleMesh visual_mesh;
    SimpleMesh collision_mesh;

    for (size_t component_index = 0; component_index < blueprint.components.size(); ++component_index) {
        const auto& component = blueprint.components[component_index];
        uint32_t component_color = default_component_color(component.component_type);
        if (!instance.material_overrides.empty()) {
            component_color = pack_color(instance.material_overrides[0].color_override);
        }

        SimpleMesh component_mesh = create_component_mesh(component, component_color);
        SimpleMesh world_mesh = transform_mesh(component_mesh, instance.component_world_transforms[component_index]);
        append_mesh(visual_mesh, world_mesh);

        SimpleMesh collision_source = clone_mesh_for_collision(component_mesh);
        SimpleMesh collision_world = transform_mesh(collision_source, instance.component_world_transforms[component_index]);
        append_mesh(collision_mesh, collision_world);
    }

    instance.visual_vertices = std::move(visual_mesh.vertices);
    instance.visual_indices = std::move(visual_mesh.indices);
    instance.collision_vertices = std::move(collision_mesh.vertices);
    instance.collision_indices = std::move(collision_mesh.indices);

    // Build BVH for collision queries
    instance.collision_bvh = build_collision_bvh(instance.component_world_transforms);
}

void ProceduralGenerator::apply_material_customization(
    const std::vector<MaterialOverride>& customization,
    ModelInstance& instance
) {
    instance.material_overrides = customization;
    if (customization.empty()) {
        return;
    }

    uint32_t override_color = pack_color(customization[0].color_override);
    for (auto& vertex : instance.visual_vertices) {
        vertex.color = override_color;
    }
}

BoundingBox ProceduralGenerator::compute_world_bounds(const ModelInstance& instance) const {
    if (!instance.visual_vertices.empty()) {
        return build_bounds_from_vertices(instance.visual_vertices);
    }
    return build_bounds_from_component_transforms(instance);
}

// ============================================================================
// LOD MANAGER IMPLEMENTATION
// ============================================================================

LODManager::LODManager() : hysteresis_buffer_(0.2f) {
    // Initialize with default LOD levels
}

LODManager::~LODManager() {
    // Cleanup
}

void LODManager::update_lod_selections(
    std::vector<ModelInstance*>& instances,
    const glm::vec3& camera_position,
    float frame_time
) {
    // Parallel LOD update using execution policy
    std::for_each(std::execution::par, instances.begin(), instances.end(),
        [this, &camera_position](ModelInstance* instance) {
            if (!instance) return;

            float distance = glm::distance(
                camera_position,
                glm::vec3(instance->world_transform.f32[12], // X
                         instance->world_transform.f32[13], // Y
                         instance->world_transform.f32[14]) // Z
            );

            uint32_t new_lod = get_lod_for_distance(distance);

            // Apply hysteresis to prevent thrashing
            if (new_lod != instance->current_lod_level) {
                float current_max_dist = (instance->current_lod_level < lod_levels_.size()) ?
                    lod_levels_[instance->current_lod_level].max_distance : FLT_MAX;

                if (distance > current_max_dist * (1.0f + hysteresis_buffer_) ||
                    distance < current_max_dist * (1.0f - hysteresis_buffer_)) {
                    instance->current_lod_level = new_lod;
                }
            }
        });
}

void LODManager::add_lod_level(const LODLevel& level) {
    // Insert in sorted order by distance
    auto it = std::lower_bound(lod_levels_.begin(), lod_levels_.end(), level,
        [](const LODLevel& a, const LODLevel& b) {
            return a.max_distance < b.max_distance;
        });
    lod_levels_.insert(it, level);
}

uint32_t LODManager::get_lod_for_distance(float distance) const {
    for (size_t i = 0; i < lod_levels_.size(); ++i) {
        if (distance <= lod_levels_[i].max_distance) {
            return static_cast<uint32_t>(i);
        }
    }
    return static_cast<uint32_t>(lod_levels_.size() - 1); // Fallback to lowest LOD
}

// ============================================================================
// INSTANCE MANAGER IMPLEMENTATION
// ============================================================================

InstanceManager::InstanceManager() {
    instances_.reserve(1024); // Pre-allocate for performance
}

InstanceManager::~InstanceManager() {
    // Cleanup instances
}

uint64_t InstanceManager::create_instance(EntityID entity_id, AssetID base_model_id) {
    uint64_t instance_id;

    if (!free_slots_.empty()) {
        instance_id = free_slots_.back();
        free_slots_.pop_back();
        instances_[instance_id] = ModelInstance{};
    } else {
        instance_id = instances_.size();
        instances_.emplace_back();
    }

    auto& instance = instances_[instance_id];
    instance.entity_id = entity_id;
    instance.base_model_id = base_model_id;
    instance.current_lod_level = 0;
    instance.render_flags = 0;

    return instance_id;
}

void InstanceManager::destroy_instance(uint64_t instance_id) {
    if (instance_id < instances_.size()) {
        // Mark as free
        free_slots_.push_back(instance_id);
        instances_[instance_id] = ModelInstance{}; // Reset
    }
}

ModelInstance* InstanceManager::get_instance(uint64_t instance_id) {
    if (instance_id < instances_.size()) {
        return &instances_[instance_id];
    }
    return nullptr;
}

ModelInstance* InstanceManager::get_instance_by_entity(EntityID entity_id) {
    for (auto& instance : instances_) {
        if (instance.entity_id == entity_id && instance.entity_id != 0) {
            return &instance;
        }
    }
    return nullptr;
}

void InstanceManager::update_transform(uint64_t instance_id, const SIMDTransform& transform) {
    if (auto* instance = get_instance(instance_id)) {
        // Update the world transform and recompute world bounds from the current AABB.
        instance->world_transform = transform;

        float min_raw[4];
        float max_raw[4];
        _mm_storeu_ps(min_raw, instance->world_bounds.min_bounds);
        _mm_storeu_ps(max_raw, instance->world_bounds.max_bounds);

        glm::vec3 min_point(min_raw[0], min_raw[1], min_raw[2]);
        glm::vec3 max_point(max_raw[0], max_raw[1], max_raw[2]);

        glm::mat4 world_transform(1.0f);
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                world_transform[c][r] = transform.f32[r * 4 + c];
            }
        }

        glm::vec3 updated_min;
        glm::vec3 updated_max;
        transform_aabb(world_transform, min_point, max_point, updated_min, updated_max);

        instance->world_bounds.min_bounds = _mm_setr_ps(updated_min.x, updated_min.y, updated_min.z, 0.0f);
        instance->world_bounds.max_bounds = _mm_setr_ps(updated_max.x, updated_max.y, updated_max.z, 0.0f);
    }
}

std::vector<ModelInstance*> InstanceManager::get_visible_instances(const glm::mat4& view_proj_matrix) {
    std::vector<ModelInstance*> visible;

    // Extract frustum planes
    glm::vec4 planes[6];
    extract_frustum_planes(view_proj_matrix, planes);

    // Check each instance against frustum
    for (auto& instance : instances_) {
        if (instance.entity_id == 0) continue; // Inactive instance

        if (is_aabb_outside_frustum(instance.world_bounds, planes)) {
            // Mark as skipped
            instance.render_flags |= RENDER_FLAG_SKIP;
            continue;
        }

        // Clear skip flag and add to visible
        instance.render_flags &= ~RENDER_FLAG_SKIP;
        visible.push_back(&instance);
    }

    return visible;
}

void InstanceManager::extract_frustum_planes(const glm::mat4& m, glm::vec4 planes[6]) {
    // Extract frustum planes from view-projection matrix
    planes[0] = m[3] + m[0]; // left
    planes[1] = m[3] - m[0]; // right
    planes[2] = m[3] + m[1]; // bottom
    planes[3] = m[3] - m[1]; // top
    planes[4] = m[3] + m[2]; // near
    planes[5] = m[3] - m[2]; // far

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 0.0f) {
            planes[i] /= len;
        }
    }
}

bool InstanceManager::is_aabb_outside_frustum(const BoundingBox& aabb, const glm::vec4 planes[6]) {
    // Get AABB corners
    float min_x = _mm_cvtss_f32(aabb.min_bounds);
    float min_y = _mm_cvtss_f32(_mm_shuffle_ps(aabb.min_bounds, aabb.min_bounds, _MM_SHUFFLE(1,1,1,1)));
    float min_z = _mm_cvtss_f32(_mm_shuffle_ps(aabb.min_bounds, aabb.min_bounds, _MM_SHUFFLE(2,2,2,2)));
    glm::vec3 min_p(min_x, min_y, min_z);
    
    float max_x = _mm_cvtss_f32(aabb.max_bounds);
    float max_y = _mm_cvtss_f32(_mm_shuffle_ps(aabb.max_bounds, aabb.max_bounds, _MM_SHUFFLE(1,1,1,1)));
    float max_z = _mm_cvtss_f32(_mm_shuffle_ps(aabb.max_bounds, aabb.max_bounds, _MM_SHUFFLE(2,2,2,2)));
    glm::vec3 max_p(max_x, max_y, max_z);

    // Check against each frustum plane
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& plane = planes[i];
        glm::vec3 normal(plane.x, plane.y, plane.z);

        // Find the farthest point in the direction of the plane normal
        glm::vec3 p(
            normal.x > 0.0f ? max_p.x : min_p.x,
            normal.y > 0.0f ? max_p.y : min_p.y,
            normal.z > 0.0f ? max_p.z : min_p.z
        );

        // Distance to plane
        float dist = glm::dot(p, normal) + plane.w;

        // If the farthest point is behind the plane, AABB is outside
        if (dist < 0.0f) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// MODEL SYSTEM IMPLEMENTATION
// ============================================================================

ModelSystem::ModelSystem()
    : rendering_engine_(nullptr)
    , physics_core_(nullptr)
    , assembly_system_(nullptr) {
}

ModelSystem::~ModelSystem() {
    // Cleanup
}

void ModelSystem::initialize(
    RenderingEngine* rendering_engine,
    physics_core::PhysicsCore* physics_core,
    AssemblySystem* assembly_system
) {
    rendering_engine_ = rendering_engine;
    physics_core_ = physics_core;
    assembly_system_ = assembly_system;

    // Initialize LOD levels
    LODLevel lod0 = {0, 100.0f, 50000.0f, 0, 0, 0, false, 0.1f};
    LODLevel lod1 = {1, 500.0f, 10000.0f, 0, 0, 0, true, 0.05f};
    LODLevel lod2 = {2, 2000.0f, 1000.0f, 0, 0, 0, true, 0.01f};

    lod_manager_.add_lod_level(lod0);
    lod_manager_.add_lod_level(lod1);
    lod_manager_.add_lod_level(lod2);
}

bool ModelSystem::generate_vehicle_from_blueprint(
    const VehicleBlueprint& blueprint,
    const std::vector<MaterialOverride>& customization,
    ModelInstance& output_instance
) {
    if (!procedural_generator_.generate_vehicle_from_blueprint(
        blueprint, customization, output_instance)) {
        return false;
    }

    // Register the generated mesh with AssetManager
    if (!output_instance.visual_vertices.empty() && !output_instance.visual_indices.empty()) {
        AssetID mesh_id = resource_manager::ResourceManager::get_instance()
            .get_asset_manager()
            .register_mesh(output_instance.visual_vertices, output_instance.visual_indices);
        
        if (mesh_id != 0) {
            output_instance.base_model_id = mesh_id;
        } else {
            // Failed to register mesh
            return false;
        }
    }

    return true;
}

void ModelSystem::update_lod_selections(const glm::vec3& camera_position, float frame_time) {
    // If rendering engine can provide a main view-projection matrix, it should be plugged in here.
    // For now, use the identity projection and rely on object bounds for simple culling.
    visible_instances_ = instance_manager_.get_visible_instances(glm::mat4(1.0f));
    lod_manager_.update_lod_selections(visible_instances_, camera_position, frame_time);
}

void ModelSystem::prepare_instanced_rendering(VkCommandBuffer cmd_buffer, const Camera& camera) {
    if (!rendering_engine_) return;

    // Group visible instances by base_model_id and LOD
    struct GroupKey { uint64_t model_id; uint32_t lod; };
    struct GroupKeyHash { size_t operator()(GroupKey const& k) const noexcept { return std::hash<uint64_t>()(k.model_id) ^ (k.lod << 1); } };
    struct GroupKeyEq { bool operator()(GroupKey const& a, GroupKey const& b) const noexcept { return a.model_id == b.model_id && a.lod == b.lod; } };

    std::unordered_map<GroupKey, std::vector<RenderableObject>, GroupKeyHash, GroupKeyEq> groups;

    // Get visible instances using camera frustum
    visible_instances_ = instance_manager_.get_visible_instances(camera.view_projection_matrix);

    for (ModelInstance* inst : visible_instances_) {
        if (!inst) continue;
        AssetID active_mesh_id = inst->base_model_id;
        if (inst->is_deformable) {
            uint8_t index = inst->current_dynamic_mesh_index.load(std::memory_order_relaxed);
            if (inst->dynamic_mesh_asset_ids[index] != 0) {
                active_mesh_id = inst->dynamic_mesh_asset_ids[index];
            }
        }

        GroupKey key{ active_mesh_id, inst->current_lod_level };
        RenderableObject obj;
        obj.transform = glm::mat4(1.0f);
        // Copy 4x4 float matrix from SIMDTransform (row-major) into glm::mat4
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                obj.transform[c][r] = inst->world_transform.f32[r * 4 + c];
            }
        }
        obj.mesh_id = static_cast<uint32_t>(active_mesh_id);
        // Use material override if available, otherwise default material
        if (!inst->material_overrides.empty()) {
            obj.material_id = static_cast<uint32_t>(inst->material_overrides[0].material_id);
        } else {
            obj.material_id = static_cast<uint32_t>(
                resource_manager::ResourceManager::get_instance()
                .get_asset_manager()
                .get_or_create_default_material());
        }
        obj.entity_id = static_cast<uint32_t>(inst->entity_id);
        obj.cast_shadow = (inst->render_flags & RENDER_FLAG_SHADOWS) != 0;
        obj.is_dynamic = true;
        groups[key].push_back(obj);
    }

    // Submit grouped renderables to the rendering engine as individual objects (fallback when GPU instancing isn't wired here)
    for (auto& kv : groups) {
        rendering_engine_->queue_renderables(kv.second);
    }
}

void ModelSystem::sync_model_to_physics(const ModelInstance& instance, physics_core::PhysicsCore& physics) {
    if (instance.entity_id == 0) return;

    // Update body transform (position + orientation)
    physics_core::PhysicsBody* body = physics.get_body(static_cast<physics_core::EntityID>(instance.entity_id));
    if (body) {
        // Extract translation from SIMDTransform (row-major indices 12,13,14)
        body->position.x = static_cast<double>(instance.world_transform.f32[12]);
        body->position.y = static_cast<double>(instance.world_transform.f32[13]);
        body->position.z = static_cast<double>(instance.world_transform.f32[14]);

        // Extract 3x3 orientation matrix (row-major upper-left 3x3)
        physics_core::Mat3x3 ori;
        ori.data[0] = static_cast<double>(instance.world_transform.f32[0]);
        ori.data[1] = static_cast<double>(instance.world_transform.f32[1]);
        ori.data[2] = static_cast<double>(instance.world_transform.f32[2]);
        ori.data[3] = static_cast<double>(instance.world_transform.f32[4]);
        ori.data[4] = static_cast<double>(instance.world_transform.f32[5]);
        ori.data[5] = static_cast<double>(instance.world_transform.f32[6]);
        ori.data[6] = static_cast<double>(instance.world_transform.f32[8]);
        ori.data[7] = static_cast<double>(instance.world_transform.f32[9]);
        ori.data[8] = static_cast<double>(instance.world_transform.f32[10]);
        body->orientation = ori;
        body->is_sleeping = false;
    }

    // If any components are destroyed, update collision shapes for the entity to reflect removal
    bool has_destroyed = false;
    for (const auto& cs : instance.component_states) {
        if (cs.is_destroyed) { has_destroyed = true; break; }
    }

    if (has_destroyed) {
        // For simplicity, register simplified collision shapes per non-destroyed component
        // Use BVH leaf bounds to approximate component shapes
        for (const BVHNode& node : instance.collision_bvh.nodes) {
            if (node.component_index == static_cast<uint32_t>(-1)) continue;
            uint32_t idx = node.component_index;
            if (idx < instance.component_states.size() && instance.component_states[idx].is_destroyed) {
                // Skip destroyed components (do not register)
                continue;
            }

            // Read bounds
            float minb[4], maxb[4];
            _mm_storeu_ps(minb, node.bounds.min_bounds);
            _mm_storeu_ps(maxb, node.bounds.max_bounds);

            physics_core::CollisionShape shape = physics_core::CollisionShape::create_box(
                static_cast<physics_core::EntityID>(instance.entity_id),
                physics_core::Vec3((double)((minb[0] + maxb[0]) * 0.5f), (double)((minb[1] + maxb[1]) * 0.5f), (double)((minb[2] + maxb[2]) * 0.5f)),
                physics_core::Vec3((double)((maxb[0] - minb[0]) * 0.5f), (double)((maxb[1] - minb[1]) * 0.5f), (double)((maxb[2] - minb[2]) * 0.5f)),
                physics_core::Mat3x3::identity(),
                true
            );
            physics.register_collision_shape(std::move(shape));
        }
        physics.update_broad_phase();
    }
}

bool ModelSystem::create_model_from_assembly(const void* assembly, ModelInstance& output) {
    if (!assembly) return false;

    // Try treating pointer as a pre-built VehicleBlueprint
    const VehicleBlueprint* vb = reinterpret_cast<const VehicleBlueprint*>(assembly);
    if (vb && !vb->components.empty()) {
        return procedural_generator_.generate_vehicle_from_blueprint(*vb, {}, output);
    }

    // Could not interpret assembly data; integration with AssemblySystem not available here
    return false;
}

void ModelSystem::submit_to_renderer(const ModelInstance& instance, RenderingEngine& renderer) {
    RenderableObject object;
    object.transform = glm::mat4(1.0f);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            object.transform[c][r] = instance.world_transform.f32[r * 4 + c];
        }
    }
    AssetID active_mesh_id = instance.base_model_id;
    if (instance.is_deformable) {
        uint8_t index = instance.current_dynamic_mesh_index.load(std::memory_order_relaxed);
        if (instance.dynamic_mesh_asset_ids[index] != 0) {
            active_mesh_id = instance.dynamic_mesh_asset_ids[index];
        }
    }

    object.mesh_id = static_cast<uint32_t>(active_mesh_id);
    object.material_id = 0;
    object.entity_id = static_cast<uint32_t>(instance.entity_id);
    object.cast_shadow = (instance.render_flags & RENDER_FLAG_SHADOWS) != 0;
    object.is_dynamic = true;
    object.transform_buffer = VK_NULL_HANDLE;

    std::vector<RenderableObject> list;
    list.push_back(object);
    renderer.queue_renderables(list);
}

bool ModelSystem::initialize_deformable_instance(ModelInstance& instance) {
    if (instance.visual_vertices.empty() || instance.visual_indices.empty()) {
        return false;
    }

    auto& asset_manager = resource_manager::ResourceManager::get_instance().get_asset_manager();
    instance.dynamic_mesh_asset_ids[0] = asset_manager.register_mesh(instance.visual_vertices, instance.visual_indices);
    instance.dynamic_mesh_asset_ids[1] = asset_manager.register_mesh(instance.visual_vertices, instance.visual_indices);
    instance.is_deformable = (instance.dynamic_mesh_asset_ids[0] != 0 && instance.dynamic_mesh_asset_ids[1] != 0);
    instance.current_dynamic_mesh_index.store(0, std::memory_order_relaxed);
    return instance.is_deformable;
}

void ModelSystem::update_deformable_meshes(physics_core::PhysicsCore& physics) {
    if (!physics.is_initialized()) {
        return;
    }

    physics_core::SoftBodySystem* soft_body_system = physics.get_soft_body_system();
    if (!soft_body_system) {
        return;
    }

    auto& asset_manager = resource_manager::ResourceManager::get_instance().get_asset_manager();
    for (const auto& body : soft_body_system->get_soft_bodies()) {
        if (body.owner_entity == 0) {
            continue;
        }

        ModelInstance* instance = instance_manager_.get_instance_by_entity(body.owner_entity);
        if (!instance || !instance->is_deformable) {
            continue;
        }

        std::vector<Vertex> updated_vertices;
        updated_vertices.reserve(body.positions.size());

        for (const auto& pos : body.positions) {
            Vertex vertex{};
            vertex.position = glm::vec3(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.tangent = glm::vec3(0.0f);
            vertex.texcoord = glm::vec2(0.0f, 0.0f);
            vertex.color = 0xFFFFFFFF;
            updated_vertices.push_back(vertex);
        }

        uint8_t back_index = instance->current_dynamic_mesh_index.load(std::memory_order_relaxed) ^ 1;
        AssetID back_id = instance->dynamic_mesh_asset_ids[back_index];
        if (back_id == 0) {
            continue;
        }

        asset_manager.update_mesh_vertices(back_id, updated_vertices);
        instance->current_dynamic_mesh_index.store(back_index, std::memory_order_release);
    }
}

std::vector<ModelInstance*> ModelSystem::get_visible_instances(const glm::mat4& view_proj_matrix) {
    return instance_manager_.get_visible_instances(view_proj_matrix);
}

uint64_t ModelSystem::create_instance(EntityID entity_id, AssetID base_model_id) {
    return instance_manager_.create_instance(entity_id, base_model_id);
}

void ModelSystem::destroy_instance(uint64_t instance_id) {
    instance_manager_.destroy_instance(instance_id);
}

ModelInstance* ModelSystem::get_instance(uint64_t instance_id) {
    return instance_manager_.get_instance(instance_id);
}

void ModelSystem::apply_damage_visualization(
    EntityID vehicle_id,
    const void* damage_event,
    ModelInstance& instance
) {
    // Simple damage visualization: fade destroyed components and preserve base model colors.
    for (auto& component_state : instance.component_states) {
        if (component_state.is_destroyed) {
            for (auto& vertex : instance.visual_vertices) {
                uint32_t alpha = 0x80u;
                uint32_t color = vertex.color;
                uint32_t rgb = color & 0x00FFFFFFu;
                vertex.color = rgb | (alpha << 24u);
            }
            break;
        }
    }
}

}  // namespace rendering_engine