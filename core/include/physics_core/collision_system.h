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

#include "types.h"
#include "physics_core/physics_body.h"
#include "physics_core/sdf_collision_shape.h"
#include <array>
#include <cstdint>
#include <immintrin.h>
#include <optional>
#include <span>
#include <vector>
#include <memory>
<<<<<<< HEAD
=======
#include <variant>
>>>>>>> c308d63 (Helped the rabbits find a home)

namespace physics_core {

class PhysicsThreadPool;
class AsyncBVHBuilder;
struct BVHData;

static constexpr uint32_t BVH_INVALID_NODE = ~0u;
static constexpr EntityID INVALID_COLLISION_ENTITY = 0;
static constexpr size_t BVH_BATCH_SIZE = 8;
static constexpr size_t CONTACT_MANIFOLD_MAX_POINTS = 8;

enum class CollisionShapeType : uint32_t {
    Sphere = 0,
    Box = 1,
    SDF = 2  // Новое
};

struct CollisionShape {
    CollisionShapeType type;
    EntityID entity_id;
    Vec3 center;
    Vec3 half_extents;
    Mat3x3 orientation;
    float radius;
    size_t sdf_index;  // Индекс в sdf_shapes_
    bool dynamic;

    CollisionShape() = default;

    static CollisionShape create_sphere(EntityID entity, const Vec3& center, float radius, bool dynamic = true);
    static CollisionShape create_box(EntityID entity, const Vec3& center, const Vec3& half_extents, const Mat3x3& orientation, bool dynamic = true);
    static CollisionShape create_sdf(EntityID entity, const Vec3& center, size_t sdf_index, bool dynamic = false);  // Новое
};

struct CollisionPair {
    EntityID entity_a;
    EntityID entity_b;
    float toi;
};

using CollisionPairs = std::vector<CollisionPair>;

struct ContactPoint {
    EntityID entity_a;
    EntityID entity_b;
    Vec3 position;
    Vec3 normal;
    float penetration;
};

struct ContactManifold {
    std::array<ContactPoint, CONTACT_MANIFOLD_MAX_POINTS> points;
    int point_count;
};

using ContactManifolds = std::vector<ContactManifold>;

struct MotionBounds {
    EntityID entity_id;
    __m256 start_pos;    // Packed 8 float positions: [x0,y0,z0,0,x1,y1,z1,0]
    __m256 end_pos;      // Packed 8 float positions
    __m256 swept_aabb;   // Packed [min_x, min_y, min_z, max_x, max_y, max_z, 0, 0]
    float movement_time;
};

struct BVHNode {
    alignas(32) float min_x[BVH_BATCH_SIZE];
    alignas(32) float min_y[BVH_BATCH_SIZE];
    alignas(32) float min_z[BVH_BATCH_SIZE];
    alignas(32) float max_x[BVH_BATCH_SIZE];
    alignas(32) float max_y[BVH_BATCH_SIZE];
    alignas(32) float max_z[BVH_BATCH_SIZE];
    alignas(32) uint32_t left_child[BVH_BATCH_SIZE];
    alignas(32) uint32_t right_child[BVH_BATCH_SIZE];
    alignas(32) uint32_t entity_id[BVH_BATCH_SIZE];
    alignas(32) uint32_t flags[BVH_BATCH_SIZE];

<<<<<<< HEAD
    // Новые поля для деформируемых тел
    alignas(32) bool is_deformable[BVH_BATCH_SIZE];
    alignas(32) const Vec3* vertex_data_ptr[BVH_BATCH_SIZE];
    alignas(32) uint32_t vertex_count[BVH_BATCH_SIZE];

=======
>>>>>>> c308d63 (Helped the rabbits find a home)
    BVHNode();
};

class BVHCollisionSystem {
public:
    BVHCollisionSystem();
    ~BVHCollisionSystem();

    void build_tree_simd(
        const float* positions_x,
        const float* positions_y,
        const float* positions_z,
        const float* radii,
        const uint32_t* entity_ids,
        size_t count
    );

    void ccd_query_simd(
        const MotionBounds* moving_objects,
        size_t count,
        CollisionPairs& output
    ) const;

    // BVH Refitting and optimization
    void refit_bvh(const float* positions_x, const float* positions_y, const float* positions_z, const float* radii, size_t count);
    void optimize_treelets();

    // Новый метод для деформируемых тел
    void refit_deformable_subtree(uint32_t node_idx);

    size_t get_node_count() const { return nodes_.size(); }
    const std::vector<BVHNode>& get_nodes() const { return nodes_; }
    const std::vector<uint32_t>& get_leaf_indices() const { return leaf_indices_; }

    // Async BVH initialization (call from PhysicsCore with thread pool)
    void initialize_async_builder(PhysicsThreadPool* thread_pool);
    void schedule_async_update(const float* positions_x,
        const float* positions_y,
        const float* positions_z,
        const float* radii,
        const uint32_t* entity_ids,
        size_t count,
        bool force_rebuild = false);

    // Check async builder status
    bool has_async_builder() const { return async_builder_ != nullptr; }

private:
    friend class AsyncBVHBuilder;

    // Background BVH helpers used by AsyncBVHBuilder
    void snapshot_active_bvh(BVHData& out) const;
    void build_tree_simd_into(BVHData& out,
        const float* positions_x,
        const float* positions_y,
        const float* positions_z,
        const float* radii,
        const uint32_t* entity_ids,
        size_t count);
    void refit_bvh_into(BVHData& out, const BVHData& source,
        const float* positions_x,
        const float* positions_y,
        const float* positions_z,
        const float* radii,
        const uint32_t* entity_ids,
        size_t count);

    void swap_internal_buffers(BVHData& back_data) noexcept;
    bool should_refit(const BVHData& snapshot, size_t count) const noexcept;

    void build_leaf_level(const std::vector<uint32_t>& sorted_indices, const float* radius_data);
    void build_internal_levels();
    bool test_aabb_overlap(
        float aminx, float aminy, float aminz,
        float amaxx, float amaxy, float amaxz,
        float bminx, float bminy, float bminz,
        float bmaxx, float bmaxy, float bmaxz
    ) const;
    void query_node_recursively(
        uint32_t node_index,
        EntityID origin_entity,
        float query_min_x,
        float query_min_y,
        float query_min_z,
        float query_max_x,
        float query_max_y,
        float query_max_z,
        CollisionPairs& output
    ) const;

    // Treelet optimization helpers
    float calculate_sah_cost(const BVHNode& node, size_t lane) const;
    void refit_single_node(size_t node_idx, size_t lane);

    std::vector<BVHNode> nodes_;
    std::vector<uint32_t> leaf_indices_;
    std::vector<float> object_aabb_min_x_;
    std::vector<float> object_aabb_min_y_;
    std::vector<float> object_aabb_min_z_;
    std::vector<float> object_aabb_max_x_;
    std::vector<float> object_aabb_max_y_;
    std::vector<float> object_aabb_max_z_;
    std::vector<float> object_radii_;
    std::vector<uint32_t> object_entity_ids_;
    
    // Async BVH builder (optional, created at runtime)
    std::unique_ptr<class AsyncBVHBuilder> async_builder_;
};

struct GJKSimplex {
    __m256 points[4];     // 4 support vertices packed as 8-wide float vectors
    int point_count;
    float barycentrics[4];
};

// Convex shape for GJK
struct ConvexShape {
    enum class Type { Sphere, Box, Capsule, ConvexHull };
<<<<<<< HEAD
    Type type;
    Vec3 center;
    union {
        struct { float radius; } sphere;
        struct { Vec3 half_extents; Mat3x3 orientation; } box;
        struct { float radius; float half_height; Vec3 axis; } capsule;
        struct { std::vector<Vec3> vertices; } hull;
    };
    
    Vec3 support(const Vec3& direction) const {
        switch (type) {
            case Type::Sphere: return center + direction.normalized() * sphere.radius;
            case Type::Box: {
                Vec3 d = box.orientation * direction;
                Vec3 support = Vec3(
                    d.x > 0 ? box.half_extents.x : -box.half_extents.x,
                    d.y > 0 ? box.half_extents.y : -box.half_extents.y,
                    d.z > 0 ? box.half_extents.z : -box.half_extents.z
                );
                // Compute the part manually instead of using transpose()
                Vec3 result = Vec3();
                result.x = support.x * box.orientation(0,0) + support.y * box.orientation(1,0) + support.z * box.orientation(2,0);
                result.y = support.x * box.orientation(0,1) + support.y * box.orientation(1,1) + support.z * box.orientation(2,1);
                result.z = support.x * box.orientation(0,2) + support.y * box.orientation(1,2) + support.z * box.orientation(2,2);
                return center + result;
            }
            case Type::Capsule: {
                Vec3 dir = direction.normalized();
                float proj = dir.dot(capsule.axis.normalized());
                Vec3 point = capsule.axis * (proj > 0 ? capsule.half_height : -capsule.half_height);
                return center + point + dir * capsule.radius;
            }
            case Type::ConvexHull: {
                Vec3 best = hull.vertices[0];
                float max_dot = best.dot(direction);
                for (const auto& v : hull.vertices) {
=======
    struct SphereShape { float radius; };
    struct BoxShape { Vec3 half_extents; Mat3x3 orientation; };
    struct CapsuleShape { float radius; float half_height; Vec3 axis; };
    using Hull = std::vector<Vec3>;

    Type type;
    Vec3 center;
    std::variant<SphereShape, BoxShape, CapsuleShape, Hull> data;

    ConvexShape() = default;

    bool is_sphere() const { return std::holds_alternative<SphereShape>(data); }
    bool is_box() const { return std::holds_alternative<BoxShape>(data); }
    bool is_capsule() const { return std::holds_alternative<CapsuleShape>(data); }
    bool is_hull() const { return std::holds_alternative<Hull>(data); }

    const SphereShape& sphere() const { return std::get<SphereShape>(data); }
    const BoxShape& box() const { return std::get<BoxShape>(data); }
    const CapsuleShape& capsule() const { return std::get<CapsuleShape>(data); }
    const Hull& hull() const { return std::get<Hull>(data); }

    Vec3 support(const Vec3& direction) const {
        switch (type) {
            case Type::Sphere: return center + direction.normalized() * std::get<SphereShape>(data).radius;
            case Type::Box: {
                const BoxShape& box_shape = std::get<BoxShape>(data);
                Vec3 d = box_shape.orientation * direction;
                Vec3 support = Vec3(
                    d.x > 0 ? box_shape.half_extents.x : -box_shape.half_extents.x,
                    d.y > 0 ? box_shape.half_extents.y : -box_shape.half_extents.y,
                    d.z > 0 ? box_shape.half_extents.z : -box_shape.half_extents.z
                );
                Vec3 result;
                result.x = support.x * box_shape.orientation(0,0) + support.y * box_shape.orientation(1,0) + support.z * box_shape.orientation(2,0);
                result.y = support.x * box_shape.orientation(0,1) + support.y * box_shape.orientation(1,1) + support.z * box_shape.orientation(2,1);
                result.z = support.x * box_shape.orientation(0,2) + support.y * box_shape.orientation(1,2) + support.z * box_shape.orientation(2,2);
                return center + result;
            }
            case Type::Capsule: {
                const CapsuleShape& capsule_shape = std::get<CapsuleShape>(data);
                Vec3 dir = direction.normalized();
                float proj = dir.dot(capsule_shape.axis.normalized());
                Vec3 point = capsule_shape.axis * (proj > 0 ? capsule_shape.half_height : -capsule_shape.half_height);
                return center + point + dir * capsule_shape.radius;
            }
            case Type::ConvexHull: {
                const Hull& vertices = std::get<Hull>(data);
                Vec3 best = vertices[0];
                float max_dot = best.dot(direction);
                for (const auto& v : vertices) {
>>>>>>> c308d63 (Helped the rabbits find a home)
                    float dot = v.dot(direction);
                    if (dot > max_dot) {
                        max_dot = dot;
                        best = v;
                    }
                }
                return center + best;
            }
        }
        return center;
    }
};

struct GJKResult {
    std::array<Vec3, 4> simplex;
    int iterations;
    bool converged;
};

class GJKSolver {
public:
    GJKSolver();
    ~GJKSolver();

    // Новый интерфейс согласно ТЗ
    bool intersect(const ConvexShape& A, const ConvexShape& B, 
                  GJKResult& out_result, uint64_t frame_seed = 0);
    
    // Старый SIMD интерфейс для совместимости
    __m256 support_function_simd(const __m256 shape_a, const __m256 direction) const;
    bool gjk_distance_simd(GJKSimplex& simplex, const __m256 a_pos, const __m256 b_pos) const;
    bool gjk_penetration_simd(GJKSimplex& simplex, __m256& penetration_dir, float& depth) const;

private:
    // Реализация GJK
    Vec3 support(const ConvexShape& shape, const Vec3& direction) const;
    bool contains_origin(std::array<Vec3, 4>& simplex, int& size, Vec3& direction) const;
    Vec3 triple_product(const Vec3& a, const Vec3& b, const Vec3& c) const;
    
    // Runtime dispatch для SIMD
    using SupportFunc = Vec3(*)(const ConvexShape&, const Vec3& direction);
    SupportFunc support_func_;
    
    static Vec3 support_scalar(const ConvexShape& shape, const Vec3& direction);
    static Vec3 support_simd(const ConvexShape& shape, const Vec3& direction);
};

struct EPASimplex {
    __m256 vertices[32];
    uint32_t faces[64][3];
    int vertex_count;
    int face_count;
    float closest_distance;
};

struct ContactManifoldOutput {
    std::array<Vec3, CONTACT_MANIFOLD_MAX_POINTS> positions;
    std::array<Vec3, CONTACT_MANIFOLD_MAX_POINTS> normals;
    std::array<float, CONTACT_MANIFOLD_MAX_POINTS> depths;
    int contact_count;
};

class EPASolver {
public:
    EPASolver();
    ~EPASolver();

    bool expand_simplex_simd(EPASimplex& simplex, const GJKSimplex& initial);
    void extract_contact_points_simd(EPASimplex& simplex, ContactManifold& output) const;
};

// CCD Solver
// Forward declarations
struct GJKResult;
class BVH;

struct CCDContact {
    EntityID body_a, body_b;
    float toi;                    // время удара [0, 1]
    Vec3 normal;             // нормаль контакта
    Vec3 point;              // точка контакта
    float penetration;            // глубина
    
    // Приоритет: сначала меньший TOI, потом детерминированный порядок ID
    bool operator<(const CCDContact& other) const {
        if (std::abs(toi - other.toi) > 1e-5f) return toi < other.toi;
        // Детерминированный порядок по ID (не по указателям!)
        return std::min(body_a, body_b) < std::min(other.body_a, other.body_b) ||
               (body_a == other.body_a && body_b < other.body_b);
    }
};

class CCDSolver {
public:
    CCDSolver();
    ~CCDSolver();
    
    // Детерминированный запрос CCD
    std::vector<CCDContact> query_ccd(
        std::span<const PhysicsBody> fast_bodies,
<<<<<<< HEAD
        const BVH& world_bvh,
=======
>>>>>>> c308d63 (Helped the rabbits find a home)
        float dt,
        uint64_t frame_seed
    );
    
private:
    // Адаптивный радиус капсулы (меньше для маленьких объектов)
    static float compute_swept_radius(const PhysicsBody& body, float velocity);
    
    // TOI тест
    static std::optional<float> compute_toi_conservative(
        const PhysicsBody& a, const PhysicsBody& b, float dt);
    
    static Vec3 compute_contact_normal(const PhysicsBody& a, const PhysicsBody& b);
    static Vec3 compute_contact_point(const PhysicsBody& a, const PhysicsBody& b, float toi);
};

class CollisionSystem {
public:
    CollisionSystem();
    ~CollisionSystem();

    size_t register_collision_shape(CollisionShape&& shape);
    size_t register_sdf_shape(SDFCollisionShape&& sdf_shape);  // Новый метод
    void update_broad_phase();
    void solve_narrow_phase(float dt);
    void resolve_contacts_simd(ContactManifolds& contacts);

    void initialize_async_builder(PhysicsThreadPool* thread_pool);

    bool raycast_simd(const Vec3& origin, const Vec3& direction, float max_distance, EntityID& hit_entity, Vec3& hit_point) const;
    void sphere_query_simd(const Vec3& center, float radius, std::vector<EntityID>& results) const;
    const CollisionShape* find_shape(EntityID entity_id) const;

    const ContactManifolds& get_contact_manifolds() const { return manifolds_; }

private:
    void build_shape_aabbs();
    void collect_broadphase_pairs();
    void generate_contacts();

    BVHCollisionSystem broad_phase_;
    GJKSolver gjk_solver_;
    EPASolver epa_solver_;
    std::vector<CollisionShape> shapes_;
    std::vector<SDFCollisionShape> sdf_shapes_;  // Новое поле для SDF
    std::vector<float> positions_x_;
    std::vector<float> positions_y_;
    std::vector<float> positions_z_;
    std::vector<float> radii_;
    std::vector<uint32_t> entity_ids_;
    CollisionPairs broad_pairs_;
    ContactManifolds manifolds_;
};

}  // namespace physics_core
