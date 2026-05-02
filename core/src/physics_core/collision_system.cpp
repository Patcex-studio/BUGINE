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
#include "physics_core/collision_system.h"
#include "physics_core/async_bvh_builder.h"
#include "physics_core/profile_macros.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace physics_core {

// -----------------------------------------------------------------------------
// CollisionShape
// -----------------------------------------------------------------------------

CollisionShape CollisionShape::create_sphere(EntityID entity, const Vec3& center, float radius, bool dynamic) {
    CollisionShape shape;
    shape.type = CollisionShapeType::Sphere;
    shape.entity_id = entity;
    shape.center = center;
    shape.radius = radius;
    shape.half_extents = Vec3(0.0, 0.0, 0.0);
    shape.orientation = Mat3x3::identity();
    shape.dynamic = dynamic;
    return shape;
}

CollisionShape CollisionShape::create_box(EntityID entity, const Vec3& center, const Vec3& half_extents, const Mat3x3& orientation, bool dynamic) {
    CollisionShape shape;
    shape.type = CollisionShapeType::Box;
    shape.entity_id = entity;
    shape.center = center;
    shape.half_extents = half_extents;
    shape.orientation = orientation;
    shape.radius = 0.0f;
    shape.dynamic = dynamic;
    return shape;
}

CollisionShape CollisionShape::create_sdf(EntityID entity, const Vec3& center, size_t sdf_index, bool dynamic) {
    CollisionShape shape;
    shape.type = CollisionShapeType::SDF;
    shape.entity_id = entity;
    shape.center = center;
    shape.sdf_index = sdf_index;
    shape.half_extents = Vec3(0.0, 0.0, 0.0);
    shape.orientation = Mat3x3::identity();
    shape.radius = 0.0f;
    shape.dynamic = dynamic;
    return shape;
}

// -----------------------------------------------------------------------------
// BVHNode
// -----------------------------------------------------------------------------

BVHNode::BVHNode() {
    std::fill(std::begin(min_x), std::end(min_x), 0.0f);
    std::fill(std::begin(min_y), std::end(min_y), 0.0f);
    std::fill(std::begin(min_z), std::end(min_z), 0.0f);
    std::fill(std::begin(max_x), std::end(max_x), 0.0f);
    std::fill(std::begin(max_y), std::end(max_y), 0.0f);
    std::fill(std::begin(max_z), std::end(max_z), 0.0f);
    std::fill(std::begin(left_child), std::end(left_child), BVH_INVALID_NODE);
    std::fill(std::begin(right_child), std::end(right_child), BVH_INVALID_NODE);
    std::fill(std::begin(entity_id), std::end(entity_id), BVH_INVALID_NODE);
    std::fill(std::begin(flags), std::end(flags), 0u);
    std::fill(std::begin(is_deformable), std::end(is_deformable), false);
    std::fill(std::begin(vertex_data_ptr), std::end(vertex_data_ptr), nullptr);
    std::fill(std::begin(vertex_count), std::end(vertex_count), 0u);
}

// -----------------------------------------------------------------------------
// BVHCollisionSystem
// -----------------------------------------------------------------------------

BVHCollisionSystem::BVHCollisionSystem() = default;
BVHCollisionSystem::~BVHCollisionSystem() = default;

void BVHCollisionSystem::initialize_async_builder(PhysicsThreadPool* thread_pool) {
    if (!thread_pool) {
        return;  // Thread pool required for async building
    }
    async_builder_ = std::make_unique<AsyncBVHBuilder>(this, thread_pool);
}

void BVHCollisionSystem::schedule_async_update(
    const float* positions_x,
    const float* positions_y,
    const float* positions_z,
    const float* radii,
    const uint32_t* entity_ids,
    size_t count,
    bool force_rebuild
) {
    if (!async_builder_) {
        return;
    }
    async_builder_->schedule_update(
        positions_x,
        positions_y,
        positions_z,
        radii,
        entity_ids,
        count,
        force_rebuild
    );
}

static inline uint32_t expand_morton_bits(uint32_t value) {
    uint32_t x = value & 0x000003ffu;
    x = (x | (x << 16)) & 0x30000ffu;
    x = (x | (x << 8)) & 0x300f00fu;
    x = (x | (x << 4)) & 0x30c30c3u;
    x = (x | (x << 2)) & 0x9249249u;
    return x;
}

static inline uint32_t morton3D(uint32_t x, uint32_t y, uint32_t z) {
    return (expand_morton_bits(x) << 2) | (expand_morton_bits(y) << 1) | expand_morton_bits(z);
}

static inline float clampf(float v, float minv, float maxv) {
    return v < minv ? minv : (v > maxv ? maxv : v);
}

void BVHCollisionSystem::build_tree_simd(
    const float* positions_x,
    const float* positions_y,
    const float* positions_z,
    const float* radii,
    const uint32_t* entity_ids,
    size_t count
) {
    object_aabb_min_x_.assign(count, 0.0f);
    object_aabb_min_y_.assign(count, 0.0f);
    object_aabb_min_z_.assign(count, 0.0f);
    object_aabb_max_x_.assign(count, 0.0f);
    object_aabb_max_y_.assign(count, 0.0f);
    object_aabb_max_z_.assign(count, 0.0f);
    object_radii_.assign(count, 0.0f);
    object_entity_ids_.assign(count, 0u);
    leaf_indices_.clear();
    nodes_.clear();

    if (count == 0) {
        return;
    }

    const __m256 one = _mm256_set1_ps(1.0f);
    size_t i = 0;
    for (; i + BVH_BATCH_SIZE <= count; i += BVH_BATCH_SIZE) {
        __m256 px = _mm256_loadu_ps(positions_x + i);
        __m256 py = _mm256_loadu_ps(positions_y + i);
        __m256 pz = _mm256_loadu_ps(positions_z + i);
        __m256 rr = _mm256_loadu_ps(radii + i);

        __m256 minx = _mm256_sub_ps(px, rr);
        __m256 miny = _mm256_sub_ps(py, rr);
        __m256 minz = _mm256_sub_ps(pz, rr);
        __m256 maxx = _mm256_add_ps(px, rr);
        __m256 maxy = _mm256_add_ps(py, rr);
        __m256 maxz = _mm256_add_ps(pz, rr);

        _mm256_storeu_ps(object_aabb_min_x_.data() + i, minx);
        _mm256_storeu_ps(object_aabb_min_y_.data() + i, miny);
        _mm256_storeu_ps(object_aabb_min_z_.data() + i, minz);
        _mm256_storeu_ps(object_aabb_max_x_.data() + i, maxx);
        _mm256_storeu_ps(object_aabb_max_y_.data() + i, maxy);
        _mm256_storeu_ps(object_aabb_max_z_.data() + i, maxz);
        _mm256_storeu_ps(object_radii_.data() + i, rr);
    }

    for (; i < count; ++i) {
        float px = positions_x[i];
        float py = positions_y[i];
        float pz = positions_z[i];
        float r = radii[i];
        object_aabb_min_x_[i] = px - r;
        object_aabb_min_y_[i] = py - r;
        object_aabb_min_z_[i] = pz - r;
        object_aabb_max_x_[i] = px + r;
        object_aabb_max_y_[i] = py + r;
        object_aabb_max_z_[i] = pz + r;
        object_radii_[i] = r;
    }

    for (size_t idx = 0; idx < count; ++idx) {
        object_entity_ids_[idx] = entity_ids[idx];
    }

    float min_x = object_aabb_min_x_[0];
    float min_y = object_aabb_min_y_[0];
    float min_z = object_aabb_min_z_[0];
    float max_x = object_aabb_max_x_[0];
    float max_y = object_aabb_max_y_[0];
    float max_z = object_aabb_max_z_[0];

    for (size_t idx = 1; idx < count; ++idx) {
        min_x = std::min(min_x, object_aabb_min_x_[idx]);
        min_y = std::min(min_y, object_aabb_min_y_[idx]);
        min_z = std::min(min_z, object_aabb_min_z_[idx]);
        max_x = std::max(max_x, object_aabb_max_x_[idx]);
        max_y = std::max(max_y, object_aabb_max_y_[idx]);
        max_z = std::max(max_z, object_aabb_max_z_[idx]);
    }

    float inv_range_x = 1.0f / clampf(max_x - min_x, 1e-3f, 1e6f);
    float inv_range_y = 1.0f / clampf(max_y - min_y, 1e-3f, 1e6f);
    float inv_range_z = 1.0f / clampf(max_z - min_z, 1e-3f, 1e6f);

    std::vector<std::pair<uint32_t, uint32_t>> morton_index;
    morton_index.reserve(count);
    for (size_t idx = 0; idx < count; ++idx) {
        float cx = 0.5f * (object_aabb_min_x_[idx] + object_aabb_max_x_[idx]);
        float cy = 0.5f * (object_aabb_min_y_[idx] + object_aabb_max_y_[idx]);
        float cz = 0.5f * (object_aabb_min_z_[idx] + object_aabb_max_z_[idx]);

        uint32_t xi = static_cast<uint32_t>(clampf((cx - min_x) * inv_range_x, 0.0f, 0.999f) * 1023.0f);
        uint32_t yi = static_cast<uint32_t>(clampf((cy - min_y) * inv_range_y, 0.0f, 0.999f) * 1023.0f);
        uint32_t zi = static_cast<uint32_t>(clampf((cz - min_z) * inv_range_z, 0.0f, 0.999f) * 1023.0f);

        uint32_t code = morton3D(xi, yi, zi);
        morton_index.emplace_back(code, static_cast<uint32_t>(idx));
    }

    std::sort(morton_index.begin(), morton_index.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<uint32_t> sorted_indices;
    sorted_indices.reserve(count);
    for (const auto& entry : morton_index) {
        sorted_indices.push_back(entry.second);
    }

    build_leaf_level(sorted_indices, radii);
    build_internal_levels();
}

void BVHCollisionSystem::build_leaf_level(const std::vector<uint32_t>& sorted_indices, const float* radius_data) {
    const size_t count = sorted_indices.size();
    const size_t leaf_count = (count + BVH_BATCH_SIZE - 1) / BVH_BATCH_SIZE;
    nodes_.reserve(leaf_count * 2);
    leaf_indices_.clear();

    for (size_t batch = 0; batch < leaf_count; ++batch) {
        BVHNode node;
        size_t base = batch * BVH_BATCH_SIZE;
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            size_t object_index = base + lane;
            if (object_index < count) {
                uint32_t object_id = sorted_indices[object_index];
                float minx = object_aabb_min_x_[object_id];
                float miny = object_aabb_min_y_[object_id];
                float minz = object_aabb_min_z_[object_id];
                float maxx = object_aabb_max_x_[object_id];
                float maxy = object_aabb_max_y_[object_id];
                float maxz = object_aabb_max_z_[object_id];

                node.min_x[lane] = minx;
                node.min_y[lane] = miny;
                node.min_z[lane] = minz;
                node.max_x[lane] = maxx;
                node.max_y[lane] = maxy;
                node.max_z[lane] = maxz;
                node.entity_id[lane] = object_entity_ids_[object_id];
                node.flags[lane] = 0;
                node.left_child[lane] = BVH_INVALID_NODE;
                node.right_child[lane] = BVH_INVALID_NODE;
                leaf_indices_.push_back(object_entity_ids_[object_id]);
            } else {
                node.entity_id[lane] = BVH_INVALID_NODE;
            }
        }
        nodes_.push_back(node);
    }
}

void BVHCollisionSystem::build_internal_levels() {
    if (nodes_.empty()) {
        return;
    }

    std::vector<uint32_t> current_level;
    current_level.reserve(nodes_.size());
    for (uint32_t i = 0; i < nodes_.size(); ++i) {
        current_level.push_back(i);
    }

    while (current_level.size() > 1) {
        std::vector<uint32_t> next_level;
        next_level.reserve((current_level.size() + BVH_BATCH_SIZE - 1) / BVH_BATCH_SIZE);

        for (size_t group = 0; group < current_level.size(); group += BVH_BATCH_SIZE) {
            BVHNode node;
            for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
                size_t child_index = group + lane;
                if (child_index < current_level.size()) {
                    uint32_t child_node = current_level[child_index];
                    const BVHNode& child = nodes_[child_node];

                    float minx = child.min_x[0];
                    float miny = child.min_y[0];
                    float minz = child.min_z[0];
                    float maxx = child.max_x[0];
                    float maxy = child.max_y[0];
                    float maxz = child.max_z[0];
                    for (size_t k = 1; k < BVH_BATCH_SIZE; ++k) {
                        if (child.entity_id[k] == BVH_INVALID_NODE) {
                            continue;
                        }
                        minx = std::min(minx, child.min_x[k]);
                        miny = std::min(miny, child.min_y[k]);
                        minz = std::min(minz, child.min_z[k]);
                        maxx = std::max(maxx, child.max_x[k]);
                        maxy = std::max(maxy, child.max_y[k]);
                        maxz = std::max(maxz, child.max_z[k]);
                    }

                    node.min_x[lane] = minx;
                    node.min_y[lane] = miny;
                    node.min_z[lane] = minz;
                    node.max_x[lane] = maxx;
                    node.max_y[lane] = maxy;
                    node.max_z[lane] = maxz;
                    node.left_child[lane] = child_node;
                    node.right_child[lane] = BVH_INVALID_NODE;
                    node.entity_id[lane] = BVH_INVALID_NODE;
                    node.flags[lane] = 0;
                } else {
                    node.entity_id[lane] = BVH_INVALID_NODE;
                    node.left_child[lane] = BVH_INVALID_NODE;
                    node.right_child[lane] = BVH_INVALID_NODE;
                }
            }
            next_level.push_back(static_cast<uint32_t>(nodes_.size()));
            nodes_.push_back(node);
        }
        current_level.swap(next_level);
    }
}

void BVHCollisionSystem::snapshot_active_bvh(BVHData& out) const {
    out.nodes_ = nodes_;
    out.leaf_indices_ = leaf_indices_;
    out.object_aabb_min_x_ = object_aabb_min_x_;
    out.object_aabb_min_y_ = object_aabb_min_y_;
    out.object_aabb_min_z_ = object_aabb_min_z_;
    out.object_aabb_max_x_ = object_aabb_max_x_;
    out.object_aabb_max_y_ = object_aabb_max_y_;
    out.object_aabb_max_z_ = object_aabb_max_z_;
    out.object_radii_ = object_radii_;
    out.object_entity_ids_ = object_entity_ids_;
    out.node_count = nodes_.size();
    out.leaf_count = leaf_indices_.size();
    out.version = 0;
}

namespace {

static void build_leaf_level_into(BVHData& out, const std::vector<uint32_t>& sorted_indices) {
    const size_t count = sorted_indices.size();
    const size_t leaf_count = (count + BVH_BATCH_SIZE - 1) / BVH_BATCH_SIZE;
    out.nodes_.reserve(leaf_count * 2);
    out.leaf_indices_.clear();

    for (size_t batch = 0; batch < leaf_count; ++batch) {
        BVHNode node;
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            size_t object_index = batch * BVH_BATCH_SIZE + lane;
            if (object_index < count) {
                uint32_t object_id = sorted_indices[object_index];
                float minx = out.object_aabb_min_x_[object_id];
                float miny = out.object_aabb_min_y_[object_id];
                float minz = out.object_aabb_min_z_[object_id];
                float maxx = out.object_aabb_max_x_[object_id];
                float maxy = out.object_aabb_max_y_[object_id];
                float maxz = out.object_aabb_max_z_[object_id];

                node.min_x[lane] = minx;
                node.min_y[lane] = miny;
                node.min_z[lane] = minz;
                node.max_x[lane] = maxx;
                node.max_y[lane] = maxy;
                node.max_z[lane] = maxz;
                node.entity_id[lane] = out.object_entity_ids_[object_id];
                node.flags[lane] = 0;
                node.left_child[lane] = BVH_INVALID_NODE;
                node.right_child[lane] = BVH_INVALID_NODE;
                out.leaf_indices_.push_back(out.object_entity_ids_[object_id]);
            } else {
                node.entity_id[lane] = BVH_INVALID_NODE;
                node.flags[lane] = 0;
                node.left_child[lane] = BVH_INVALID_NODE;
                node.right_child[lane] = BVH_INVALID_NODE;
                node.min_x[lane] = 0.0f;
                node.min_y[lane] = 0.0f;
                node.min_z[lane] = 0.0f;
                node.max_x[lane] = 0.0f;
                node.max_y[lane] = 0.0f;
                node.max_z[lane] = 0.0f;
            }
        }
        out.nodes_.push_back(node);
    }
}

static void build_internal_levels_into(BVHData& out) {
    if (out.nodes_.empty()) {
        return;
    }

    std::vector<uint32_t> current_level;
    current_level.reserve(out.nodes_.size());
    for (uint32_t i = 0; i < out.nodes_.size(); ++i) {
        current_level.push_back(i);
    }

    while (current_level.size() > 1) {
        std::vector<uint32_t> next_level;
        next_level.reserve((current_level.size() + BVH_BATCH_SIZE - 1) / BVH_BATCH_SIZE);

        for (size_t group = 0; group < current_level.size(); group += BVH_BATCH_SIZE) {
            BVHNode node;
            for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
                size_t child_index = group + lane;
                if (child_index < current_level.size()) {
                    uint32_t child_node = current_level[child_index];
                    const BVHNode& child = out.nodes_[child_node];

                    float minx = child.min_x[0];
                    float miny = child.min_y[0];
                    float minz = child.min_z[0];
                    float maxx = child.max_x[0];
                    float maxy = child.max_y[0];
                    float maxz = child.max_z[0];
                    for (size_t k = 1; k < BVH_BATCH_SIZE; ++k) {
                        if (child.entity_id[k] == BVH_INVALID_NODE) {
                            continue;
                        }
                        minx = std::min(minx, child.min_x[k]);
                        miny = std::min(miny, child.min_y[k]);
                        minz = std::min(minz, child.min_z[k]);
                        maxx = std::max(maxx, child.max_x[k]);
                        maxy = std::max(maxy, child.max_y[k]);
                        maxz = std::max(maxz, child.max_z[k]);
                    }

                    node.min_x[lane] = minx;
                    node.min_y[lane] = miny;
                    node.min_z[lane] = minz;
                    node.max_x[lane] = maxx;
                    node.max_y[lane] = maxy;
                    node.max_z[lane] = maxz;
                    node.left_child[lane] = child_node;
                    node.right_child[lane] = BVH_INVALID_NODE;
                    node.entity_id[lane] = BVH_INVALID_NODE;
                    node.flags[lane] = 0;
                } else {
                    node.entity_id[lane] = BVH_INVALID_NODE;
                    node.flags[lane] = 0;
                    node.left_child[lane] = BVH_INVALID_NODE;
                    node.right_child[lane] = BVH_INVALID_NODE;
                    node.min_x[lane] = 0.0f;
                    node.min_y[lane] = 0.0f;
                    node.min_z[lane] = 0.0f;
                    node.max_x[lane] = 0.0f;
                    node.max_y[lane] = 0.0f;
                    node.max_z[lane] = 0.0f;
                }
            }
            next_level.push_back(static_cast<uint32_t>(out.nodes_.size()));
            out.nodes_.push_back(node);
        }
        current_level.swap(next_level);
    }
}

}

void BVHCollisionSystem::build_tree_simd_into(
    BVHData& out,
    const float* positions_x,
    const float* positions_y,
    const float* positions_z,
    const float* radii,
    const uint32_t* entity_ids,
    size_t count
) {
    out.object_aabb_min_x_.assign(count, 0.0f);
    out.object_aabb_min_y_.assign(count, 0.0f);
    out.object_aabb_min_z_.assign(count, 0.0f);
    out.object_aabb_max_x_.assign(count, 0.0f);
    out.object_aabb_max_y_.assign(count, 0.0f);
    out.object_aabb_max_z_.assign(count, 0.0f);
    out.object_radii_.assign(count, 0.0f);
    out.object_entity_ids_.assign(count, 0u);
    out.leaf_indices_.clear();
    out.nodes_.clear();

    if (count == 0) {
        out.node_count = 0;
        out.leaf_count = 0;
        return;
    }

    size_t i = 0;
    for (; i + BVH_BATCH_SIZE <= count; i += BVH_BATCH_SIZE) {
        __m256 px = _mm256_loadu_ps(positions_x + i);
        __m256 py = _mm256_loadu_ps(positions_y + i);
        __m256 pz = _mm256_loadu_ps(positions_z + i);
        __m256 rr = _mm256_loadu_ps(radii + i);

        __m256 minx = _mm256_sub_ps(px, rr);
        __m256 miny = _mm256_sub_ps(py, rr);
        __m256 minz = _mm256_sub_ps(pz, rr);
        __m256 maxx = _mm256_add_ps(px, rr);
        __m256 maxy = _mm256_add_ps(py, rr);
        __m256 maxz = _mm256_add_ps(pz, rr);

        _mm256_storeu_ps(out.object_aabb_min_x_.data() + i, minx);
        _mm256_storeu_ps(out.object_aabb_min_y_.data() + i, miny);
        _mm256_storeu_ps(out.object_aabb_min_z_.data() + i, minz);
        _mm256_storeu_ps(out.object_aabb_max_x_.data() + i, maxx);
        _mm256_storeu_ps(out.object_aabb_max_y_.data() + i, maxy);
        _mm256_storeu_ps(out.object_aabb_max_z_.data() + i, maxz);
        _mm256_storeu_ps(out.object_radii_.data() + i, rr);
    }

    for (; i < count; ++i) {
        float px = positions_x[i];
        float py = positions_y[i];
        float pz = positions_z[i];
        float r = radii[i];
        out.object_aabb_min_x_[i] = px - r;
        out.object_aabb_min_y_[i] = py - r;
        out.object_aabb_min_z_[i] = pz - r;
        out.object_aabb_max_x_[i] = px + r;
        out.object_aabb_max_y_[i] = py + r;
        out.object_aabb_max_z_[i] = pz + r;
        out.object_radii_[i] = r;
    }

    for (size_t idx = 0; idx < count; ++idx) {
        out.object_entity_ids_[idx] = entity_ids[idx];
    }

    float min_x = out.object_aabb_min_x_[0];
    float min_y = out.object_aabb_min_y_[0];
    float min_z = out.object_aabb_min_z_[0];
    float max_x = out.object_aabb_max_x_[0];
    float max_y = out.object_aabb_max_y_[0];
    float max_z = out.object_aabb_max_z_[0];

    for (size_t idx = 1; idx < count; ++idx) {
        min_x = std::min(min_x, out.object_aabb_min_x_[idx]);
        min_y = std::min(min_y, out.object_aabb_min_y_[idx]);
        min_z = std::min(min_z, out.object_aabb_min_z_[idx]);
        max_x = std::max(max_x, out.object_aabb_max_x_[idx]);
        max_y = std::max(max_y, out.object_aabb_max_y_[idx]);
        max_z = std::max(max_z, out.object_aabb_max_z_[idx]);
    }

    float inv_range_x = 1.0f / clampf(max_x - min_x, 1e-3f, 1e6f);
    float inv_range_y = 1.0f / clampf(max_y - min_y, 1e-3f, 1e6f);
    float inv_range_z = 1.0f / clampf(max_z - min_z, 1e-3f, 1e6f);

    std::vector<std::pair<uint32_t, uint32_t>> morton_index;
    morton_index.reserve(count);
    for (size_t idx = 0; idx < count; ++idx) {
        float cx = 0.5f * (out.object_aabb_min_x_[idx] + out.object_aabb_max_x_[idx]);
        float cy = 0.5f * (out.object_aabb_min_y_[idx] + out.object_aabb_max_y_[idx]);
        float cz = 0.5f * (out.object_aabb_min_z_[idx] + out.object_aabb_max_z_[idx]);

        uint32_t xi = static_cast<uint32_t>(clampf((cx - min_x) * inv_range_x, 0.0f, 0.999f) * 1023.0f);
        uint32_t yi = static_cast<uint32_t>(clampf((cy - min_y) * inv_range_y, 0.0f, 0.999f) * 1023.0f);
        uint32_t zi = static_cast<uint32_t>(clampf((cz - min_z) * inv_range_z, 0.0f, 0.999f) * 1023.0f);

        uint32_t code = morton3D(xi, yi, zi);
        morton_index.emplace_back(code, static_cast<uint32_t>(idx));
    }

    std::sort(morton_index.begin(), morton_index.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<uint32_t> sorted_indices;
    sorted_indices.reserve(count);
    for (const auto& entry : morton_index) {
        sorted_indices.push_back(entry.second);
    }

    build_leaf_level_into(out, sorted_indices);
    build_internal_levels_into(out);

    out.node_count = out.nodes_.size();
    out.leaf_count = out.leaf_indices_.size();
}

void BVHCollisionSystem::refit_bvh_into(
    BVHData& out,
    const BVHData& source,
    const float* positions_x,
    const float* positions_y,
    const float* positions_z,
    const float* radii,
    const uint32_t* entity_ids,
    size_t count
) {
    out = source;
    out.node_count = source.node_count;
    out.leaf_count = source.leaf_count;

    std::unordered_map<uint32_t, size_t> index_map;
    index_map.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        index_map[entity_ids[i]] = i;
    }

    for (BVHNode& node : out.nodes_) {
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            if (node.left_child[lane] == BVH_INVALID_NODE && node.entity_id[lane] != BVH_INVALID_NODE) {
                auto it = index_map.find(node.entity_id[lane]);
                if (it == index_map.end()) {
                    continue;
                }
                size_t input_idx = it->second;
                float r = radii[input_idx];
                float px = positions_x[input_idx];
                float py = positions_y[input_idx];
                float pz = positions_z[input_idx];
                node.min_x[lane] = px - r;
                node.min_y[lane] = py - r;
                node.min_z[lane] = pz - r;
                node.max_x[lane] = px + r;
                node.max_y[lane] = py + r;
                node.max_z[lane] = pz + r;
            }
        }
    }

    for (int node_idx = static_cast<int>(out.nodes_.size()) - 1; node_idx >= 0; --node_idx) {
        BVHNode& node = out.nodes_[node_idx];
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            uint32_t left = node.left_child[lane];
            uint32_t right = node.right_child[lane];
            if (left == BVH_INVALID_NODE) {
                continue;
            }

            float min_x = std::numeric_limits<float>::max();
            float min_y = std::numeric_limits<float>::max();
            float min_z = std::numeric_limits<float>::max();
            float max_x = std::numeric_limits<float>::lowest();
            float max_y = std::numeric_limits<float>::lowest();
            float max_z = std::numeric_limits<float>::lowest();

            if (left < out.nodes_.size()) {
                const BVHNode& left_node = out.nodes_[left];
                for (size_t i = 0; i < BVH_BATCH_SIZE; ++i) {
                    min_x = std::min(min_x, left_node.min_x[i]);
                    min_y = std::min(min_y, left_node.min_y[i]);
                    min_z = std::min(min_z, left_node.min_z[i]);
                    max_x = std::max(max_x, left_node.max_x[i]);
                    max_y = std::max(max_y, left_node.max_y[i]);
                    max_z = std::max(max_z, left_node.max_z[i]);
                }
            }
            if (right < out.nodes_.size()) {
                const BVHNode& right_node = out.nodes_[right];
                for (size_t i = 0; i < BVH_BATCH_SIZE; ++i) {
                    min_x = std::min(min_x, right_node.min_x[i]);
                    min_y = std::min(min_y, right_node.min_y[i]);
                    min_z = std::min(min_z, right_node.min_z[i]);
                    max_x = std::max(max_x, right_node.max_x[i]);
                    max_y = std::max(max_y, right_node.max_y[i]);
                    max_z = std::max(max_z, right_node.max_z[i]);
                }
            }

            node.min_x[lane] = min_x;
            node.min_y[lane] = min_y;
            node.min_z[lane] = min_z;
            node.max_x[lane] = max_x;
            node.max_y[lane] = max_y;
            node.max_z[lane] = max_z;
        }
    }
}

void BVHCollisionSystem::swap_internal_buffers(BVHData& back_data) noexcept {
    std::swap(nodes_, back_data.nodes_);
    std::swap(leaf_indices_, back_data.leaf_indices_);
    std::swap(object_aabb_min_x_, back_data.object_aabb_min_x_);
    std::swap(object_aabb_min_y_, back_data.object_aabb_min_y_);
    std::swap(object_aabb_min_z_, back_data.object_aabb_min_z_);
    std::swap(object_aabb_max_x_, back_data.object_aabb_max_x_);
    std::swap(object_aabb_max_y_, back_data.object_aabb_max_y_);
    std::swap(object_aabb_max_z_, back_data.object_aabb_max_z_);
    std::swap(object_radii_, back_data.object_radii_);
    std::swap(object_entity_ids_, back_data.object_entity_ids_);
}

bool BVHCollisionSystem::should_refit(const BVHData& snapshot, size_t count) const noexcept {
    if (snapshot.leaf_count == 0) {
        return false;
    }
    return snapshot.leaf_count == count;
}

bool BVHCollisionSystem::test_aabb_overlap(
    float aminx, float aminy, float aminz,
    float amaxx, float amaxy, float amaxz,
    float bminx, float bminy, float bminz,
    float bmaxx, float bmaxy, float bmaxz
) const {
    return !(amaxx < bminx || amaxy < bminy || amaxz < bminz ||
             bmaxx < aminx || bmaxy < aminy || bmaxz < aminz);
}

void BVHCollisionSystem::query_node_recursively(
    uint32_t node_index,
    EntityID origin_entity,
    float query_min_x,
    float query_min_y,
    float query_min_z,
    float query_max_x,
    float query_max_y,
    float query_max_z,
    CollisionPairs& output
) const {
    if (node_index >= nodes_.size()) {
        return;
    }

    const BVHNode& node = nodes_[node_index];
    for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
        if (node.entity_id[lane] == BVH_INVALID_NODE && node.left_child[lane] == BVH_INVALID_NODE) {
            continue;
        }

        float minx = node.min_x[lane];
        float miny = node.min_y[lane];
        float minz = node.min_z[lane];
        float maxx = node.max_x[lane];
        float maxy = node.max_y[lane];
        float maxz = node.max_z[lane];

        if (!test_aabb_overlap(query_min_x, query_min_y, query_min_z, query_max_x, query_max_y, query_max_z,
                               minx, miny, minz, maxx, maxy, maxz)) {
            continue;
        }

        if (node.left_child[lane] == BVH_INVALID_NODE) {
            // leaf entry
            if (node.entity_id[lane] != origin_entity) {
                output.push_back({origin_entity, node.entity_id[lane], 0.0f});
            }
        } else {
            query_node_recursively(node.left_child[lane], origin_entity, query_min_x, query_min_y, query_min_z, query_max_x, query_max_y, query_max_z, output);
        }
    }
}

void BVHCollisionSystem::ccd_query_simd(
    const MotionBounds* moving_objects,
    size_t count,
    CollisionPairs& output
) const {
    output.clear();
    if (nodes_.empty() || count == 0) {
        return;
    }

    for (size_t object_index = 0; object_index < count; ++object_index) {
        const MotionBounds& motion = moving_objects[object_index];

        float query_min_x = std::numeric_limits<float>::infinity();
        float query_min_y = std::numeric_limits<float>::infinity();
        float query_min_z = std::numeric_limits<float>::infinity();
        float query_max_x = -std::numeric_limits<float>::infinity();
        float query_max_y = -std::numeric_limits<float>::infinity();
        float query_max_z = -std::numeric_limits<float>::infinity();

        alignas(32) float tmp[8];
        _mm256_store_ps(tmp, motion.swept_aabb);
        query_min_x = tmp[0];
        query_min_y = tmp[1];
        query_min_z = tmp[2];
        query_max_x = tmp[3];
        query_max_y = tmp[4];
        query_max_z = tmp[5];

        CollisionPairs candidates;
        query_node_recursively(static_cast<uint32_t>(nodes_.size() - 1), motion.entity_id, query_min_x, query_min_y, query_min_z, query_max_x, query_max_y, query_max_z, candidates);

        for (const auto& candidate : candidates) {
            if (candidate.entity_a != INVALID_COLLISION_ENTITY && candidate.entity_b != INVALID_COLLISION_ENTITY) {
                CollisionPair pair = candidate;
                pair.toi = motion.movement_time;
                output.push_back(pair);
            }
        }
    }
}

// Новый метод для деформируемых тел
void BVHCollisionSystem::refit_deformable_subtree(uint32_t node_idx) {
    if (node_idx >= nodes_.size()) return;

    BVHNode& node = nodes_[node_idx];

    // Для каждого из 8 слотов в узле
    for (size_t i = 0; i < BVH_BATCH_SIZE; ++i) {
        if (!node.is_deformable[i] || node.vertex_data_ptr[i] == nullptr || node.vertex_count[i] == 0) continue;

        // Вычисление нового AABB на основе вершин
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float min_z = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        float max_z = std::numeric_limits<float>::lowest();

        const Vec3* vertices = node.vertex_data_ptr[i];
        for (uint32_t j = 0; j < node.vertex_count[i]; ++j) {
            const Vec3& v = vertices[j];
            min_x = std::min(min_x, static_cast<float>(v.x));
            min_y = std::min(min_y, static_cast<float>(v.y));
            min_z = std::min(min_z, static_cast<float>(v.z));
            max_x = std::max(max_x, static_cast<float>(v.x));
            max_y = std::max(max_y, static_cast<float>(v.y));
            max_z = std::max(max_z, static_cast<float>(v.z));
        }

        node.min_x[i] = min_x;
        node.min_y[i] = min_y;
        node.min_z[i] = min_z;
        node.max_x[i] = max_x;
        node.max_y[i] = max_y;
        node.max_z[i] = max_z;
    }

    // Рекурсивно обновить дочерние узлы
    for (size_t i = 0; i < BVH_BATCH_SIZE; ++i) {
        if (node.left_child[i] != BVH_INVALID_NODE) {
            refit_deformable_subtree(node.left_child[i]);
        }
        if (node.right_child[i] != BVH_INVALID_NODE) {
            refit_deformable_subtree(node.right_child[i]);
        }
    }
}

// BVH Refitting and Treelet Optimization
void BVHCollisionSystem::refit_bvh(const float* positions_x, const float* positions_y, const float* positions_z, const float* radii, size_t count) {
    if (nodes_.empty() || count == 0) return;

    // Update object AABBs first
    object_aabb_min_x_.resize(count);
    object_aabb_min_y_.resize(count);
    object_aabb_min_z_.resize(count);
    object_aabb_max_x_.resize(count);
    object_aabb_max_y_.resize(count);
    object_aabb_max_z_.resize(count);
    object_radii_.resize(count);

    // Recompute AABBs from current positions
    const __m256 one = _mm256_set1_ps(1.0f);
    size_t i = 0;
    for (; i + BVH_BATCH_SIZE <= count; i += BVH_BATCH_SIZE) {
        __m256 px = _mm256_loadu_ps(positions_x + i);
        __m256 py = _mm256_loadu_ps(positions_y + i);
        __m256 pz = _mm256_loadu_ps(positions_z + i);
        __m256 rr = _mm256_loadu_ps(radii + i);

        __m256 minx = _mm256_sub_ps(px, rr);
        __m256 miny = _mm256_sub_ps(py, rr);
        __m256 minz = _mm256_sub_ps(pz, rr);
        __m256 maxx = _mm256_add_ps(px, rr);
        __m256 maxy = _mm256_add_ps(py, rr);
        __m256 maxz = _mm256_add_ps(pz, rr);

        _mm256_storeu_ps(object_aabb_min_x_.data() + i, minx);
        _mm256_storeu_ps(object_aabb_min_y_.data() + i, miny);
        _mm256_storeu_ps(object_aabb_min_z_.data() + i, minz);
        _mm256_storeu_ps(object_aabb_max_x_.data() + i, maxx);
        _mm256_storeu_ps(object_aabb_max_y_.data() + i, maxy);
        _mm256_storeu_ps(object_aabb_max_z_.data() + i, maxz);
        _mm256_storeu_ps(object_radii_.data() + i, rr);
    }

    for (; i < count; ++i) {
        float px = positions_x[i];
        float py = positions_y[i];
        float pz = positions_z[i];
        float r = radii[i];
        object_aabb_min_x_[i] = px - r;
        object_aabb_min_y_[i] = py - r;
        object_aabb_min_z_[i] = pz - r;
        object_aabb_max_x_[i] = px + r;
        object_aabb_max_y_[i] = py + r;
        object_aabb_max_z_[i] = pz + r;
        object_radii_[i] = r;
    }

    // Update leaf nodes with new AABBs
    size_t leaf_start = nodes_.size() / 2;
    size_t object_idx = 0;
    
    for (size_t node_idx = leaf_start; node_idx < nodes_.size(); ++node_idx) {
        BVHNode& node = nodes_[node_idx];
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            if (node.entity_id[lane] != BVH_INVALID_NODE && object_idx < count) {
                // Find the corresponding object in our updated AABBs
                // Since entity_ids may not be in order, we need to find the right object
                bool found = false;
                for (size_t obj = 0; obj < count; ++obj) {
                    if (object_entity_ids_[obj] == node.entity_id[lane]) {
                        node.min_x[lane] = object_aabb_min_x_[obj];
                        node.min_y[lane] = object_aabb_min_y_[obj];
                        node.min_z[lane] = object_aabb_min_z_[obj];
                        node.max_x[lane] = object_aabb_max_x_[obj];
                        node.max_y[lane] = object_aabb_max_y_[obj];
                        node.max_z[lane] = object_aabb_max_z_[obj];
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Fallback: use the next available object (shouldn't happen in normal operation)
                    node.min_x[lane] = object_aabb_min_x_[object_idx];
                    node.min_y[lane] = object_aabb_min_y_[object_idx];
                    node.min_z[lane] = object_aabb_min_z_[object_idx];
                    node.max_x[lane] = object_aabb_max_x_[object_idx];
                    node.max_y[lane] = object_aabb_max_y_[object_idx];
                    node.max_z[lane] = object_aabb_max_z_[object_idx];
                }
            }
            object_idx++;
            if (object_idx >= count) break;
        }
        if (object_idx >= count) break;
    }

    // Propagate AABBs upwards from leaves to root
    for (int i = static_cast<int>(leaf_start) - 1; i >= 0; --i) {
        BVHNode& node = nodes_[i];
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            if (node.left_child[lane] != BVH_INVALID_NODE && node.right_child[lane] != BVH_INVALID_NODE) {
                const BVHNode& left = nodes_[node.left_child[lane]];
                const BVHNode& right = nodes_[node.right_child[lane]];
                
                // Merge AABBs from children
                node.min_x[lane] = std::min(left.min_x[lane], right.min_x[lane]);
                node.min_y[lane] = std::min(left.min_y[lane], right.min_y[lane]);
                node.min_z[lane] = std::min(left.min_z[lane], right.min_z[lane]);
                node.max_x[lane] = std::max(left.max_x[lane], right.max_x[lane]);
                node.max_y[lane] = std::max(left.max_y[lane], right.max_y[lane]);
                node.max_z[lane] = std::max(left.max_z[lane], right.max_z[lane]);
            }
        }
    }

    // Run treelet optimization periodically
    optimize_treelets();
}

void BVHCollisionSystem::optimize_treelets() {
    if (nodes_.size() < 7) return;  // Need minimum treelet size

    static int frame_counter = 0;
    frame_counter++;
    
    // Run optimization every 100 frames or when tree quality degrades significantly
    if (frame_counter % 100 != 0) return;
    
    // For each internal node, try small rotations
    for (size_t node_idx = 0; node_idx < nodes_.size() / 2; ++node_idx) {
        BVHNode& node = nodes_[node_idx];
        
        for (size_t lane = 0; lane < BVH_BATCH_SIZE; ++lane) {
            if (node.left_child[lane] == BVH_INVALID_NODE || node.right_child[lane] == BVH_INVALID_NODE) 
                continue;
                
            // Calculate current SAH cost
            float current_cost = calculate_sah_cost(node, lane);
            
            // Try swapping children
            uint32_t temp = node.left_child[lane];
            node.left_child[lane] = node.right_child[lane];
            node.right_child[lane] = temp;
            
            // Recalculate AABB and cost
            refit_single_node(node_idx, lane);
            float new_cost = calculate_sah_cost(node, lane);
            
            // Keep swap if it improves cost
            if (new_cost >= current_cost) {
                // Swap back
                temp = node.left_child[lane];
                node.left_child[lane] = node.right_child[lane];
                node.right_child[lane] = temp;
                refit_single_node(node_idx, lane);
            }
        }
    }
}

float BVHCollisionSystem::calculate_sah_cost(const BVHNode& node, size_t lane) const {
    // Simplified SAH calculation
    float surface_area = (node.max_x[lane] - node.min_x[lane]) * 
                        (node.max_y[lane] - node.min_y[lane]) * 
                        (node.max_z[lane] - node.min_z[lane]);
    return surface_area;  // Simplified - real SAH would consider traversal costs
}

void BVHCollisionSystem::refit_single_node(size_t node_idx, size_t lane) {
    BVHNode& node = nodes_[node_idx];
    if (node.left_child[lane] != BVH_INVALID_NODE && node.right_child[lane] != BVH_INVALID_NODE) {
        const BVHNode& left = nodes_[node.left_child[lane]];
        const BVHNode& right = nodes_[node.right_child[lane]];
        
        node.min_x[lane] = std::min(left.min_x[lane], right.min_x[lane]);
        node.min_y[lane] = std::min(left.min_y[lane], right.min_y[lane]);
        node.min_z[lane] = std::min(left.min_z[lane], right.min_z[lane]);
        node.max_x[lane] = std::max(left.max_x[lane], right.max_x[lane]);
        node.max_y[lane] = std::max(left.max_y[lane], right.max_y[lane]);
        node.max_z[lane] = std::max(left.max_z[lane], right.max_z[lane]);
    }
}

// -----------------------------------------------------------------------------
// GJKSolver
// -----------------------------------------------------------------------------

GJKSolver::GJKSolver() {
    #if defined(__AVX2__)
    support_func_ = support_simd;
    #else
    support_func_ = support_scalar;
    #endif
}

GJKSolver::~GJKSolver() = default;

Vec3 GJKSolver::support_scalar(const ConvexShape& shape, const Vec3& direction) {
    return shape.support(direction);
}

Vec3 GJKSolver::support_simd(const ConvexShape& shape, const Vec3& direction) {
    // Для простоты используем скалярную версию, SIMD можно добавить позже
    return shape.support(direction);
}

Vec3 GJKSolver::support(const ConvexShape& shape, const Vec3& direction) const {
    return support_func_(shape, direction);
}

Vec3 GJKSolver::triple_product(const Vec3& a, const Vec3& b, const Vec3& c) const {
    return b * c.dot(a) - a * c.dot(b);
}

bool GJKSolver::contains_origin(std::array<Vec3, 4>& simplex, int& size, Vec3& direction) const {
    static constexpr float EPSILON = 1e-6f;
    
    if (size == 2) {
        // Линия
        Vec3 ab = simplex[1] - simplex[0];
        Vec3 ao = -simplex[0];
        Vec3 ab_perp = triple_product(ab, ao, ab);
        direction = ab_perp;
        return false;
    } else if (size == 3) {
        // Треугольник
        Vec3 ab = simplex[1] - simplex[0];
        Vec3 ac = simplex[2] - simplex[0];
        Vec3 ao = -simplex[0];
        
        Vec3 abc = ab.cross(ac);
        Vec3 ab_perp = triple_product(ac, ab, ab);
        Vec3 ac_perp = triple_product(ab, ac, ac);
        
        if (ab_perp.dot(ao) > 0) {
            simplex[2] = simplex[1];
            simplex[1] = simplex[0];
            size = 2;
            direction = ab_perp;
            return false;
        }
        
        if (ac_perp.dot(ao) > 0) {
            simplex[1] = simplex[0];
            size = 2;
            direction = ac_perp;
            return false;
        }
        
        if (abc.dot(ao) > 0) {
            direction = abc;
            return false;
        } else {
            direction = -abc;
            return false;
        }
    } else if (size == 4) {
        // Тетраэдр
        Vec3 ab = simplex[1] - simplex[0];
        Vec3 ac = simplex[2] - simplex[0];
        Vec3 ad = simplex[3] - simplex[0];
        Vec3 ao = -simplex[0];
        
        Vec3 abc = ab.cross(ac);
        Vec3 acd = ac.cross(ad);
        Vec3 adb = ad.cross(ab);
        
        if (abc.dot(ao) > 0) {
            simplex[3] = simplex[2];
            simplex[2] = simplex[1];
            simplex[1] = simplex[0];
            size = 3;
            direction = abc;
            return false;
        }
        
        if (acd.dot(ao) > 0) {
            simplex[1] = simplex[0];
            simplex[2] = simplex[3];
            size = 3;
            direction = acd;
            return false;
        }
        
        if (adb.dot(ao) > 0) {
            simplex[2] = simplex[1];
            simplex[1] = simplex[3];
            size = 3;
            direction = adb;
            return false;
        }
        
        return true; // Проникновение
    }
    
    return false;
}

bool GJKSolver::intersect(const ConvexShape& A, const ConvexShape& B, 
                          GJKResult& out_result, uint64_t frame_seed) {
    std::array<Vec3, 4> simplex;
    int simplex_size = 0;
    
    // Начальное направление
    Vec3 direction = (B.center - A.center).normalized();
    if (direction.magnitude() < 1e-6f) direction = Vec3(1, 0, 0);
    
    const int MAX_ITERATIONS = 64;
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        Vec3 support_A = support(A, direction);
        Vec3 support_B = support(B, -direction);
        Vec3 new_point = support_A - support_B;
        
        if (new_point.dot(direction) < 1e-6f) {
            out_result.simplex = simplex;
            out_result.iterations = iter;
            out_result.converged = true;
            return false; // Разделение
        }
        
        simplex[simplex_size++] = new_point;
        
        if (contains_origin(simplex, simplex_size, direction)) {
            out_result.simplex = simplex;
            out_result.iterations = iter;
            out_result.converged = true;
            return true; // Пересечение
        }
    }
    
    out_result.converged = false;
    return true; // Консервативно считаем пересечением
}

// Старые методы для совместимости
__m256 GJKSolver::support_function_simd(const __m256 shape_a, const __m256 direction) const {
    return _mm256_mul_ps(_mm256_set1_ps(0.5f), direction);
}

bool GJKSolver::gjk_distance_simd(GJKSimplex& simplex, const __m256 a_pos, const __m256 b_pos) const {
    __m256 d = _mm256_sub_ps(a_pos, b_pos);
    __m256 abs_d = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), d);
    __m256 sum_xy = _mm256_add_ps(abs_d, _mm256_permute_ps(abs_d, 0b10110001));
    __m256 dist = _mm256_sqrt_ps(_mm256_add_ps(sum_xy, _mm256_permute_ps(abs_d, 0b01001110)));
    float values[8];
    _mm256_store_ps(values, dist);
    for (int i = 0; i < 8; ++i) {
        if (values[i] < 1e-6f) {
            return true;
        }
    }
    return false;
}

bool GJKSolver::gjk_penetration_simd(GJKSimplex& simplex, __m256& penetration_dir, float& depth) const {
    depth = 0.0f;
    penetration_dir = _mm256_setzero_ps();
    return simplex.point_count > 0;
}

// -----------------------------------------------------------------------------
// EPASolver
// -----------------------------------------------------------------------------

EPASolver::EPASolver() = default;
EPASolver::~EPASolver() = default;

bool EPASolver::expand_simplex_simd(EPASimplex& simplex, const GJKSimplex& initial) {
    // Простая реализация EPA
    // Инициализируем тетраэдр из симплекса GJK
    if (initial.point_count != 4) return false;
    
    simplex.vertex_count = 4;
    simplex.face_count = 4;
    
    // Грань 0: 0,1,2
    simplex.faces[0][0] = 0; simplex.faces[0][1] = 1; simplex.faces[0][2] = 2;
    // Грань 1: 0,1,3
    simplex.faces[1][0] = 0; simplex.faces[1][1] = 1; simplex.faces[1][2] = 3;
    // Грань 2: 0,2,3
    simplex.faces[2][0] = 0; simplex.faces[2][1] = 2; simplex.faces[2][2] = 3;
    // Грань 3: 1,2,3
    simplex.faces[3][0] = 1; simplex.faces[3][1] = 2; simplex.faces[3][2] = 3;
    
    // Копируем вершины (упрощённо, без SIMD)
    for (int i = 0; i < 4; ++i) {
        float vals[8] = {0};
        _mm256_storeu_ps(vals, initial.points[i]);
        // Предполагаем, что точки в первых 3 компонентах
        // simplex.vertices[i] = ... упрощённо
    }
    
    simplex.closest_distance = 0.1f; // Заглушка
    return true;
}

void EPASolver::extract_contact_points_simd(EPASimplex& simplex, ContactManifold& output) const {
    if (simplex.face_count == 0) return;
    
    // Находим ближайшую грань
    int closest_face = 0;
    float min_dist = std::numeric_limits<float>::max();
    
    for (int i = 0; i < simplex.face_count; ++i) {
        // Вычисляем расстояние до грани (упрощённо)
        float dist = 0.1f; // Заглушка
        if (dist < min_dist) {
            min_dist = dist;
            closest_face = i;
        }
    }
    
    // Создаём контактную точку
    output.points[0].position = Vec3(0, 0, 0); // Заглушка
    output.points[0].normal = Vec3(0, 1, 0);
    output.points[0].penetration = min_dist;
    output.points[0].entity_a = 0; // Нужно передать
    output.points[0].entity_b = 0;
    output.point_count = 1;
}

// -----------------------------------------------------------------------------
// CCDSolver
// -----------------------------------------------------------------------------

CCDSolver::CCDSolver() = default;
CCDSolver::~CCDSolver() = default;

float CCDSolver::compute_swept_radius(const PhysicsBody& body, float velocity) {
    float base_radius = body.bounding_radius;
    // Уменьшаем радиус для быстрых маленьких объектов (пули)
    if (velocity > 100.0f && base_radius < 0.1f) {
        return base_radius * 0.5f;  // меньше ложных срабатываний
    }
    return base_radius;
}

std::optional<float> CCDSolver::compute_toi_conservative(
    const PhysicsBody& a, const PhysicsBody& b, float dt) {
    // Упрощённый TOI тест для сфер
    Vec3 relative_velocity = b.velocity - a.velocity;
    Vec3 delta_pos = b.position - a.position;
    float dist = delta_pos.magnitude();
    float sum_radii = a.bounding_radius + b.bounding_radius;
    
    if (dist <= sum_radii) return 0.0f; // Уже пересекаются
    
    float relative_speed = relative_velocity.magnitude();
    if (relative_speed < 1e-6f) return std::nullopt; // Не движутся
    
    float toi = (dist - sum_radii) / relative_speed;
    if (toi < 0 || toi > dt) return std::nullopt;
    
    return toi;
}

Vec3 CCDSolver::compute_contact_normal(const PhysicsBody& a, const PhysicsBody& b) {
    Vec3 delta = b.position - a.position;
    return delta.normalized();
}

Vec3 CCDSolver::compute_contact_point(const PhysicsBody& a, const PhysicsBody& b, float toi) {
    // Интерполируем позицию
    Vec3 pos_a = a.position + a.velocity * toi;
    Vec3 pos_b = b.position + b.velocity * toi;
    return (pos_a + pos_b) * 0.5f;
}

std::vector<CCDContact> CCDSolver::query_ccd(
    std::span<const PhysicsBody> fast_bodies,
    const BVH& world_bvh,
    float dt,
    uint64_t frame_seed) 
{
    std::vector<CCDContact> contacts;
    contacts.reserve(fast_bodies.size() * 4);  // preallocate
    
    // Обработка в детерминированном порядке (по EntityId)
    std::vector<size_t> sorted_indices(fast_bodies.size());
    for (size_t i = 0; i < fast_bodies.size(); ++i) {
        sorted_indices[i] = i;
    }
    std::sort(sorted_indices.begin(), sorted_indices.end(), 
        [&](size_t a, size_t b) {
            return fast_bodies[a].entity_id < fast_bodies[b].entity_id;
        });
    
    for (size_t idx : sorted_indices) {
        const auto& body = fast_bodies[idx];
        if (body.velocity.magnitude() < 100.0f) continue;
        
        // Построение swept-капсулы
        float radius = compute_swept_radius(body, body.velocity.magnitude());
        // SweptVolume swept{
        //     .start = body.position,
        //     .end = body.position + body.velocity * dt,
        //     .radius = radius
        // };
        
        // BVH запрос (упрощённо, без реального BVH)
        // auto candidates = world_bvh.query_swept_deterministic(swept, frame_seed);
        std::vector<EntityID> candidates = {1, 2}; // Заглушка
        
        for (EntityID candidate_id : candidates) {
            if (candidate_id == body.entity_id) continue;
            
            // Точный TOI тест
            PhysicsBody dummy_b; // Заглушка
            dummy_b.position = Vec3(0, 0, 0);
            dummy_b.velocity = Vec3(0, 0, 0);
            dummy_b.bounding_radius = 1.0f;
            dummy_b.entity_id = candidate_id;
            
            if (auto toi = compute_toi_conservative(body, dummy_b, dt)) {
                contacts.push_back(CCDContact{
                    .body_a = body.entity_id,
                    .body_b = candidate_id,
                    .toi = toi.value(),
                    .normal = compute_contact_normal(body, dummy_b),
                    .point = compute_contact_point(body, dummy_b, toi.value()),
                    .penetration = 0.0f
                });
            }
        }
    }
    
    // Сортировка контактов по приоритету (детерминированно)
    std::sort(contacts.begin(), contacts.end());
    
    return contacts;
}

// -----------------------------------------------------------------------------
// CollisionSystem
// -----------------------------------------------------------------------------

CollisionSystem::CollisionSystem() = default;
CollisionSystem::~CollisionSystem() = default;

size_t CollisionSystem::register_collision_shape(CollisionShape&& shape) {
    shapes_.push_back(std::move(shape));
    return shapes_.size() - 1;
}

size_t CollisionSystem::register_sdf_shape(SDFCollisionShape&& sdf_shape) {
    sdf_shapes_.push_back(std::move(sdf_shape));
    return sdf_shapes_.size() - 1;
}

void CollisionSystem::initialize_async_builder(PhysicsThreadPool* thread_pool) {
    if (broad_phase_.has_async_builder()) {
        return;
    }
    broad_phase_.initialize_async_builder(thread_pool);
}

void CollisionSystem::build_shape_aabbs() {
    const size_t count = shapes_.size();
    positions_x_.assign(count, 0.0f);
    positions_y_.assign(count, 0.0f);
    positions_z_.assign(count, 0.0f);
    radii_.assign(count, 0.0f);
    entity_ids_.assign(count, INVALID_COLLISION_ENTITY);

    for (size_t i = 0; i < count; ++i) {
        const CollisionShape& shape = shapes_[i];
        positions_x_[i] = static_cast<float>(shape.center.x);
        positions_y_[i] = static_cast<float>(shape.center.y);
        positions_z_[i] = static_cast<float>(shape.center.z);
        entity_ids_[i] = static_cast<uint32_t>(shape.entity_id);
        if (shape.type == CollisionShapeType::Sphere) {
            radii_[i] = shape.radius;
        } else {
            radii_[i] = static_cast<float>(std::max({shape.half_extents.x, shape.half_extents.y, shape.half_extents.z}));
        }
    }
}

void CollisionSystem::update_broad_phase() {
    PHYSICS_PROFILE_SCOPE("BroadPhase");
    build_shape_aabbs();
    if (!positions_x_.empty()) {
        if (broad_phase_.has_async_builder()) {
            broad_phase_.schedule_async_update(
                positions_x_.data(),
                positions_y_.data(),
                positions_z_.data(),
                radii_.data(),
                entity_ids_.data(),
                positions_x_.size(),
                false
            );
        } else if (broad_phase_.get_node_count() == 0) {
            broad_phase_.build_tree_simd(
                positions_x_.data(),
                positions_y_.data(),
                positions_z_.data(),
                radii_.data(),
                entity_ids_.data(),
                positions_x_.size()
            );
        } else {
            broad_phase_.refit_bvh(
                positions_x_.data(),
                positions_y_.data(),
                positions_z_.data(),
                radii_.data(),
                positions_x_.size()
            );
        }
    }
}

void CollisionSystem::collect_broadphase_pairs() {
    broad_pairs_.clear();
    if (shapes_.empty()) {
        return;
    }

    std::vector<MotionBounds> motions;
    motions.reserve(shapes_.size());
    for (const CollisionShape& shape : shapes_) {
        MotionBounds motion;
        motion.entity_id = shape.entity_id;
        motion.start_pos = _mm256_set_ps(0.0f, static_cast<float>(shape.center.z), static_cast<float>(shape.center.y), static_cast<float>(shape.center.x), 0.0f, static_cast<float>(shape.center.z), static_cast<float>(shape.center.y), static_cast<float>(shape.center.x));
        motion.end_pos = motion.start_pos;
        float r = shape.type == CollisionShapeType::Sphere ? shape.radius : static_cast<float>(std::max({shape.half_extents.x, shape.half_extents.y, shape.half_extents.z}));
        motion.swept_aabb = _mm256_set_ps(0.0f,
            static_cast<float>(shape.center.z + r),
            static_cast<float>(shape.center.y + r),
            static_cast<float>(shape.center.x + r),
            static_cast<float>(shape.center.z - r),
            static_cast<float>(shape.center.y - r),
            static_cast<float>(shape.center.x - r),
            0.0f
        );
        motion.movement_time = 0.0f;
        motions.push_back(motion);
    }

    broad_phase_.ccd_query_simd(motions.data(), motions.size(), broad_pairs_);
}

void CollisionSystem::solve_narrow_phase(float dt) {
    PHYSICS_PROFILE_SCOPE("NarrowPhase");
    broad_pairs_.clear();
    manifolds_.clear();
    collect_broadphase_pairs();
    generate_contacts();
}

void CollisionSystem::generate_contacts() {
    manifolds_.reserve(broad_pairs_.size());
    for (const CollisionPair& pair : broad_pairs_) {
        if (pair.entity_a == INVALID_COLLISION_ENTITY || pair.entity_b == INVALID_COLLISION_ENTITY) {
            continue;
        }

        const CollisionShape* a = nullptr;
        const CollisionShape* b = nullptr;
        for (const CollisionShape& shape : shapes_) {
            if (shape.entity_id == pair.entity_a) {
                a = &shape;
            } else if (shape.entity_id == pair.entity_b) {
                b = &shape;
            }
        }
        if (!a || !b) {
            continue;
        }

        ContactManifold manifold;
        manifold.point_count = 0;

        if (a->type == CollisionShapeType::Sphere && b->type == CollisionShapeType::Sphere) {
            Vec3 delta = b->center - a->center;
            float dist = static_cast<float>(delta.magnitude());
            float radius_sum = a->radius + b->radius;
            if (dist < radius_sum && dist > 1e-6f) {
                Vec3 normal = delta / dist;
                Vec3 contact_point = a->center + normal * (a->radius - 0.5f * (radius_sum - dist));
                float penetration = radius_sum - dist;
                manifold.points[0] = {a->entity_id, b->entity_id, contact_point, normal, penetration};
                manifold.point_count = 1;
                manifolds_.push_back(manifold);
            }
        } else {
            GJKSimplex simplex;
            simplex.point_count = 0;
            __m256 pa = _mm256_set1_ps(0.0f);
            __m256 pb = _mm256_set1_ps(0.0f);
            float depth = 0.0f;
            __m256 penetration_dir;
            if (gjk_solver_.gjk_penetration_simd(simplex, penetration_dir, depth)) {
                ContactPoint contact;
                contact.entity_a = a->entity_id;
                contact.entity_b = b->entity_id;
                contact.normal = Vec3(0.0, 1.0, 0.0);
                contact.position = a->center;
                contact.penetration = depth;
                manifold.points[0] = contact;
                manifold.point_count = 1;
                manifolds_.push_back(manifold);
            }
        }
    }
}

void CollisionSystem::resolve_contacts_simd(ContactManifolds& contacts) {
    contacts = manifolds_;
}

bool CollisionSystem::raycast_simd(const Vec3& origin, const Vec3& direction, float max_distance, EntityID& hit_entity, Vec3& hit_point) const {
    hit_entity = INVALID_COLLISION_ENTITY;
    float best_distance = max_distance;
    for (const CollisionShape& shape : shapes_) {
        if (shape.type != CollisionShapeType::Sphere) {
            continue;
        }
        Vec3 to_center = shape.center - origin;
        float projection = static_cast<float>(to_center.dot(direction));
        if (projection < 0 || projection > best_distance) {
            continue;
        }
        Vec3 closest = origin + direction * projection;
        Vec3 delta = shape.center - closest;
        float distance_sq = static_cast<float>(delta.dot(delta));
        if (distance_sq <= shape.radius * shape.radius) {
            best_distance = projection;
            hit_entity = shape.entity_id;
            hit_point = closest;
        }
    }
    return hit_entity != INVALID_COLLISION_ENTITY;
}

const CollisionShape* CollisionSystem::find_shape(EntityID entity_id) const {
    for (const CollisionShape& shape : shapes_) {
        if (shape.entity_id == entity_id) {
            return &shape;
        }
    }
    return nullptr;
}

void CollisionSystem::sphere_query_simd(const Vec3& center, float radius, std::vector<EntityID>& results) const {
    results.clear();
    float radius_sq = radius * radius;
    for (const CollisionShape& shape : shapes_) {
        Vec3 delta = shape.center - center;
        float distance_sq = static_cast<float>(delta.dot(delta));
        if (distance_sq <= radius_sq + 1e-6f) {
            results.push_back(shape.entity_id);
        }
    }
}

}  // namespace physics_core
