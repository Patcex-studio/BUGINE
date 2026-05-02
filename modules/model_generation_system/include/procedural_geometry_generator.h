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

#include <vector>
#include <memory>
#include "blueprint_engine.h"

namespace model_generation {

// Forward declarations
struct Mesh;
struct BoxParams;
struct CylinderParams;
struct SphereParams;
struct ExtrusionParams;
struct ParametricSurface;
struct DeformationField;

class ProceduralGeometryGenerator {
public:
    ProceduralGeometryGenerator();
    ~ProceduralGeometryGenerator();

    // Primitive generators
    std::unique_ptr<Mesh> generate_box_primitive(const BoxParams& params);
    std::unique_ptr<Mesh> generate_cylinder_primitive(const CylinderParams& params);
    std::unique_ptr<Mesh> generate_sphere_primitive(const SphereParams& params);
    std::unique_ptr<Mesh> generate_extrusion_primitive(const ExtrusionParams& params);
    
    // Composite operations
    std::unique_ptr<Mesh> boolean_union(const Mesh& a, const Mesh& b);
    std::unique_ptr<Mesh> boolean_difference(const Mesh& a, const Mesh& b);
    std::unique_ptr<Mesh> boolean_intersection(const Mesh& a, const Mesh& b);
    
    // Parametric modeling
    std::unique_ptr<Mesh> generate_parametric_surface(const ParametricSurface& surface);
    std::unique_ptr<Mesh> apply_deformation_field(const Mesh& base, const DeformationField& field);
    
    // LOD generation
    std::unique_ptr<Mesh> generate_lod_mesh(const Mesh& high_detail, uint32_t target_triangles);

private:
    // Internal implementation details
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Parameter structures
struct BoxParams {
    __m128 dimensions;  // width, height, depth
    uint32_t subdivisions;
};

struct CylinderParams {
    float radius;
    float height;
    uint32_t radial_segments;
    uint32_t height_segments;
    bool capped;
};

struct SphereParams {
    float radius;
    uint32_t width_segments;
    uint32_t height_segments;
};

struct ExtrusionParams {
    std::vector<__m128> profile_points;  // 2D profile
    float depth;
    uint32_t segments;
};

// Simplified Mesh structure (integrate with rendering engine)
struct Mesh {
    std::vector<__m128> vertices;    // SIMD positions
    std::vector<__m128> normals;     // SIMD normals
    std::vector<__m128> uvs;         // SIMD UVs
    std::vector<uint32_t> indices;
    BoundingBox bounds;
};

} // namespace model_generation