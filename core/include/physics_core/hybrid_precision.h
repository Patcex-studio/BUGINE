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
#include <cstdint>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <functional>
#include <queue>
#include <memory>
#include <shared_mutex>
#include <immintrin.h>

namespace physics_core {

// ============================================================================
// Event System for Hybrid Precision Synchronization
// ============================================================================

enum class PrecisionEventType : uint8_t {
    ORIGIN_SHIFTED,              // Origin updated (old_origin, new_origin)
    OBJECT_SYNC_REQUIRED,        // Object needs synchronization
    HIGH_PRECISION_NEEDED,       // High-speed object detected
    SYNC_COMPLETED,              // Full sync batch completed
    FALLBACK_TO_SCALAR,          // SIMD unavailable, using scalar
    PRECISION_WARNING,           // Precision degradation detected
};

struct PrecisionEvent {
    PrecisionEventType type;
    uint64_t entity_id;
    double old_origin[3];        // For ORIGIN_SHIFTED
    double new_origin[3];        // For ORIGIN_SHIFTED
    double velocity_magnitude;   // For HIGH_PRECISION_NEEDED
    uint64_t timestamp;          // Event creation time
};

// ============================================================================
// Global Object Representation (Storage Layer)
// ============================================================================

struct alignas(64) GlobalObject {
    EntityID id;                 // 64-bit unique identifier
    double global_pos[3];        // dvec3: X, Y, Z (±1e15 meters)
    float global_rot[4];         // quat: w, x, y, z
    uint32_t object_type;        // enum: TANK=0, AIRCRAFT=1, PROJECTILE=2
    mutable uint32_t update_counter;     // For synchronization
    mutable std::atomic<uint32_t> sync_generation;  // Generation counter for invalidation
    
    static constexpr size_t SIZE_BYTES = 48; // 8 + 24 + 16 = 48
    
    GlobalObject() : id(0), object_type(0), update_counter(0), sync_generation(0) {
        global_pos[0] = global_pos[1] = global_pos[2] = 0.0;
        global_rot[0] = 1.0f; global_rot[1] = global_rot[2] = global_rot[3] = 0.0f;
    }
    
    GlobalObject(EntityID eid, const double pos[3], const float rot[4], uint32_t type)
        : id(eid), object_type(type), update_counter(0), sync_generation(0) {
        global_pos[0] = pos[0]; global_pos[1] = pos[1]; global_pos[2] = pos[2];
        global_rot[0] = rot[0]; global_rot[1] = rot[1]; global_rot[2] = rot[2]; global_rot[3] = rot[3];
    }
    
    // Copy constructor for assignment
    GlobalObject(const GlobalObject& other) 
        : id(other.id), object_type(other.object_type), 
          update_counter(other.update_counter), sync_generation(other.sync_generation.load()) {
        global_pos[0] = other.global_pos[0];
        global_pos[1] = other.global_pos[1];
        global_pos[2] = other.global_pos[2];
        global_rot[0] = other.global_rot[0];
        global_rot[1] = other.global_rot[1];
        global_rot[2] = other.global_rot[2];
        global_rot[3] = other.global_rot[3];
    }
    
    // Assignment operator
    GlobalObject& operator=(const GlobalObject& other) {
        if (this != &other) {
            id = other.id;
            global_pos[0] = other.global_pos[0];
            global_pos[1] = other.global_pos[1];
            global_pos[2] = other.global_pos[2];
            global_rot[0] = other.global_rot[0];
            global_rot[1] = other.global_rot[1];
            global_rot[2] = other.global_rot[2];
            global_rot[3] = other.global_rot[3];
            object_type = other.object_type;
            update_counter = other.update_counter;
            sync_generation.store(other.sync_generation.load());
        }
        return *this;
    }
};

// ============================================================================
// Local Physics Representation (Computation Layer - SIMD optimized)
// ============================================================================

struct alignas(32) LocalPhysicsBody {
    __m256 pos_vec;              // xyz + padding (float32)
    __m256 vel_vec;              // xyz + padding (velocity)
    __m256 acc_vec;              // xyz + padding (acceleration)
    __m256 aux_data;             // xyz (angular_velocity) + mass_inv
    
    uint32_t entity_type;        // bitmask: SOLID=1, FLUID=2, GAS=4
    uint32_t sync_flags;         // dirty=0x01, fast_object=0x02, active=0x04
    uint64_t last_sync;          // timestamp counter
    uint32_t frame_generation;   // For invalidation tracking
};

static_assert(sizeof(LocalPhysicsBody) == 160, "LocalPhysicsBody must be exactly 160 bytes for four 256-bit SIMD lanes plus flags");
static_assert(alignof(LocalPhysicsBody) >= 32, "LocalPhysicsBody must be 32-byte aligned");

// LocalPhysicsBody helper functions
namespace local_body_utils {
    inline LocalPhysicsBody create(
        const float pos[3], const float vel[3], const float acc[3],
        float mass_inv = 1.0f, uint32_t type = 1
    ) {
        LocalPhysicsBody body;
        body.pos_vec = _mm256_setr_ps(pos[0], pos[1], pos[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        body.vel_vec = _mm256_setr_ps(vel[0], vel[1], vel[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        body.acc_vec = _mm256_setr_ps(acc[0], acc[1], acc[2], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        body.aux_data = _mm256_setr_ps(0.0f, 0.0f, 0.0f, mass_inv, 0.0f, 0.0f, 0.0f, 0.0f);
        body.entity_type = type;
        body.sync_flags = 4; // ACTIVE
        body.last_sync = 0;
        body.frame_generation = 0;
        return body;
    }
    
    inline void extract_position(const LocalPhysicsBody& body, float out[3]) {
        __m128 low = _mm256_castps256_ps128(body.pos_vec);
        out[0] = _mm_cvtss_f32(low);
        out[1] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(1, 1, 1, 1)));
        out[2] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(2, 2, 2, 2)));
    }
    
    inline void extract_velocity(const LocalPhysicsBody& body, float out[3]) {
        __m128 low = _mm256_castps256_ps128(body.vel_vec);
        out[0] = _mm_cvtss_f32(low);
        out[1] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(1, 1, 1, 1)));
        out[2] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(2, 2, 2, 2)));
    }
    
    inline void mark_synced(LocalPhysicsBody& body) { body.sync_flags &= ~0x01; }
    inline void mark_dirty(LocalPhysicsBody& body) { body.sync_flags |= 0x01; }
    inline bool is_dirty(const LocalPhysicsBody& body) { return (body.sync_flags & 0x01) != 0; }
    inline bool is_fast_object(const LocalPhysicsBody& body) { return (body.sync_flags & 0x02) != 0; }
    inline bool is_active(const LocalPhysicsBody& body) { return (body.sync_flags & 0x04) != 0; }
}

// ============================================================================
// Reference Frame System (Floating Origin Management)
// ============================================================================

struct ReferenceFrame {
    double origin[3];            // Current floating origin (double precision)
    uint64_t frame_generation;   // Generation counter for invalidation tracking
    std::atomic<bool> needs_resync;  // Dirty flag
    std::atomic<uint64_t> last_used;   // Last access timestamp
    
    // Grid properties
    static constexpr double GRID_CELL_SIZE = 100.0;  // 100m cells
    
    ReferenceFrame() : frame_generation(0), needs_resync(false), last_used(0) {
        origin[0] = origin[1] = origin[2] = 0.0;
    }
    
    ReferenceFrame(const double org[3]) : frame_generation(0), needs_resync(true), last_used(0) {
        origin[0] = org[0];
        origin[1] = org[1];
        origin[2] = org[2];
    }
    
    // Grid-snap coordinates to GRID_CELL_SIZE increments
    static void grid_snap(const double pos[3], double out[3]) {
        for (int i = 0; i < 3; ++i) {
            out[i] = std::floor(pos[i] / GRID_CELL_SIZE) * GRID_CELL_SIZE;
        }
    }
    
    // Calculate distance from origin
    double distance_from_origin(const double pos[3]) const {
        double dx = pos[0] - origin[0];
        double dy = pos[1] - origin[1];
        double dz = pos[2] - origin[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// ============================================================================
// Fast Projectile Handler (High-Precision Path for Fast Objects)
// ============================================================================

struct HighPrecisionProjectile {
    EntityID id;
    double start_pos[3];         // Launch coordinates (double)
    double velocity[3];          // High-precision velocity vector
    double elapsed_time;         // For trajectory calculation
    float mass;                  // Projectile mass
    float drag_coefficient;      // Aerodynamic drag
    uint64_t creation_time;      // When projectile was created
    
    HighPrecisionProjectile() : id(0), elapsed_time(0.0), mass(1.0f), 
                               drag_coefficient(0.1f), creation_time(0) {
        start_pos[0] = start_pos[1] = start_pos[2] = 0.0;
        velocity[0] = velocity[1] = velocity[2] = 0.0;
    }
    
    // Calculate current position using double-precision integration
    void compute_position(double out[3], double dt_step = 0.016667) const {
        // Simple Euler integration: pos = pos0 + vel * t
        out[0] = start_pos[0] + velocity[0] * elapsed_time;
        out[1] = start_pos[1] + velocity[1] * elapsed_time;
        out[2] = start_pos[2] + velocity[2] * elapsed_time;
    }
    
    // Get velocity magnitude
    float velocity_magnitude() const {
        double vx = velocity[0], vy = velocity[1], vz = velocity[2];
        return static_cast<float>(std::sqrt(vx*vx + vy*vy + vz*vz));
    }
};

// ============================================================================
// Main Hybrid Precision Management System
// ============================================================================

class HybridPrecisionSystem {
public:
    using EventCallback = std::function<void(const PrecisionEvent&)>;
    
    static constexpr double DEFAULT_ORIGIN_THRESHOLD = 500.0;    // meters
    static constexpr double FAST_OBJECT_VELOCITY_THRESHOLD = 100.0;  // m/s
    static constexpr size_t MAX_PROJECTILES = 10000;
    
    HybridPrecisionSystem();
    ~HybridPrecisionSystem();
    
    // ========== Configuration ==========
    
    /**
     * Set the threshold distance for origin shift
     * @param meters Distance in meters (default: 500m)
     */
    void set_simulation_threshold(double meters);
    
    /**
     * Enable/disable fast projectile mode
     * @param enabled If true, use high-precision for fast objects
     */
    void enable_fast_projectile_mode(bool enabled);
    
    /**
     * Get count of currently active objects
     * @return Number of active objects
     */
    size_t get_active_objects_count() const;
    
    /**
     * Get count of active projectiles
     * @return Number of projectiles
     */
    size_t get_active_projectiles_count() const;
    
    /**
     * Validate consistency between global and local coordinates
     * @param epsilon Tolerance for comparison
     * @return True if all objects are consistent
     */
    bool validate_consistency(float epsilon = 1e-4f) const;
    
    // ========== Global Storage Management ==========
    
    /**
     * Register a global object (storage layer)
     * @param obj Global object to register
     * @return Success status
     */
    bool register_global_object(const GlobalObject& obj);
    
    /**
     * Update global object position
     * @param id Object entity ID
     * @param pos New position
     */
    void update_global_position(EntityID id, const double pos[3]);
    
    /**
     * Get global object by ID
     * @param id Entity ID
     * @return Pointer to global object (nullptr if not found)
     */
    const GlobalObject* get_global_object(EntityID id) const;
    
    /**
     * Remove object from global storage
     * @param id Entity ID
     */
    void unregister_global_object(EntityID id);
    
    // ========== Reference Frame Management ==========
    
    /**
     * Update reference frame based on observer position
     * @param player_camera_pos Observer position (typically camera)
     * @param threshold Distance threshold for origin shift (optional)
     */
    void update_reference_frame(const double player_camera_pos[3], 
                               double threshold = DEFAULT_ORIGIN_THRESHOLD);
    
    /**
     * Get current reference frame
     * @return Current reference frame
     */
    const ReferenceFrame& get_reference_frame() const;
    
    /**
     * Manually set reference frame origin (for debugging/streaming)
     * @param origin New origin coordinates
     */
    void set_reference_frame_origin(const double origin[3]);

    /**
     * Force a full resynchronization of the reference frame.
     */
    void force_full_resync();

    /**
     * Request an origin shift to a new location.
     * @param new_origin New origin coordinates
     */
    void request_origin_shift(const Vec3& new_origin);

    /**
     * Apply a pending origin shift if one has been requested.
     */
    void apply_pending_shift();
    
    // ========== SIMD Synchronization (Global ↔ Local) ==========
    
    /**
     * Sync global objects to local physics bodies (double → float)
     * Uses SIMD (AVX2/AVX-512) for batch processing
     * 
     * @param globals Array of global objects
     * @param locals Output array of local physics bodies
     * @param count Number of objects to sync
     * @return Number of successfully synced objects
     */
    size_t sync_global_to_local(
        const GlobalObject* globals,
        LocalPhysicsBody* locals,
        size_t count
    );
    
    /**
     * Sync local physics bodies back to global storage (float → double)
     * Uses SIMD for batch processing
     * 
     * @param globals Output array of global objects
     * @param locals Array of local physics bodies
     * @param count Number of objects to sync
     * @return Number of successfully synced objects
     */
    size_t sync_local_to_global(
        GlobalObject* globals,
        const LocalPhysicsBody* locals,
        size_t count
    );
    
    /**
     * Sync single object (global → local)
     * @param global_obj Global object
     * @param local_body Output local body
     */
    void sync_single_global_to_local(const GlobalObject& global_obj, LocalPhysicsBody& local_body);
    
    /**
     * Sync single object (local → global)
     * @param local_body Local physics body
     * @param global_obj Output global object
     */
    void sync_single_local_to_global(const LocalPhysicsBody& local_body, GlobalObject& global_obj);
    
    // ========== Fast Projectile Handling ==========
    
    /**
     * Register a high-precision projectile
     * @param proj Projectile data
     * @return Success status
     */
    bool register_projectile(const HighPrecisionProjectile& proj);
    
    /**
     * Update projectile trajectory
     * @param id Projectile ID
     * @param dt Time delta
     */
    void update_projectile(EntityID id, double dt);
    
    /**
     * Get projectile current position (double precision)
     * @param id Projectile ID
     * @param out Output position
     * @return Success status
     */
    bool get_projectile_position(EntityID id, double out[3]) const;
    
    /**
     * Remove projectile from tracking
     * @param id Projectile ID
     */
    void unregister_projectile(EntityID id);
    
    // ========== Event System ==========
    
    /**
     * Subscribe to precision events
     * @param callback Function to call on event
     * @return Subscription ID (use to unsubscribe)
     */
    uint64_t subscribe_events(const EventCallback& callback);
    
    /**
     * Unsubscribe from events
     * @param subscription_id Subscription ID from subscribe_events
     */
    void unsubscribe_events(uint64_t subscription_id);
    
    /**
     * Publish event to all subscribers
     * @param event Event to publish
     */
    void publish_event(const PrecisionEvent& event);
    
    // ========== Statistics & Debugging ==========
    
    /**
     * Get memory usage statistics
     * @return Memory used in bytes
     */
    size_t get_memory_usage() const;
    
    /**
     * Get synchronization statistics
     * @return Struct with counts and performance metrics
     */
    struct SyncStats {
        size_t global_objects;
        size_t local_bodies;
        size_t projectiles;
        size_t events_published;
        double last_origin_shift_distance;
        uint64_t total_syncs;
    };
    
    SyncStats get_statistics() const;
    
    /**
     * Validate system consistency
     * @return True if all invariants are satisfied
     */
    bool validate_consistency() const;
    
private:
    // Origin shift buffers
    struct OriginBuffer {
        Vec3 origin;
        std::atomic<uint64_t> version{0};
        alignas(64) char padding[64 - sizeof(Vec3) - sizeof(std::atomic<uint64_t>)];
    };
    
    OriginBuffer active_origin_;
    OriginBuffer pending_origin_;
    
    // Storage
    std::unordered_map<EntityID, GlobalObject> global_objects_;
    std::unordered_map<EntityID, HighPrecisionProjectile> projectiles_;
    mutable std::shared_mutex global_objects_mutex_;
    mutable std::shared_mutex projectiles_mutex_;
    mutable std::shared_mutex local_objects_mutex_;
    std::unordered_map<EntityID, LocalPhysicsBody> local_object_cache_;
    
    // Reference frame
    ReferenceFrame current_frame_;
    mutable std::shared_mutex frame_mutex_;
    
    // Configuration
    double origin_threshold_;
    bool fast_projectile_enabled_;
    
    // Events
    struct EventSubscription {
        uint64_t id;
        EventCallback callback;
    };
    std::vector<EventSubscription> event_subscribers_;
    mutable std::shared_mutex event_mutex_;
    uint64_t next_subscription_id_;
    
    // Statistics
    std::atomic<uint64_t> total_syncs_;
    std::atomic<size_t> events_published_;
    
    // SIMD utilities
    struct SIMDState {
        bool has_simd;
        bool has_avx2;
        bool has_avx512;
    };
    SIMDState simd_state_;
    
    // Helper methods
    void detect_simd_capabilities();
    void shift_all_origins(const double old_origin[3], const double new_origin[3]);
    void emit_event(const PrecisionEvent& event);
    
    // SIMD batch operations (internal)
    size_t batch_sync_g2l_simd(const GlobalObject* globals, LocalPhysicsBody* locals, size_t count);
    size_t batch_sync_l2g_simd(GlobalObject* globals, const LocalPhysicsBody* locals, size_t count);
    size_t batch_sync_g2l_scalar(const GlobalObject* globals, LocalPhysicsBody* locals, size_t count);
    size_t batch_sync_l2g_scalar(GlobalObject* globals, const LocalPhysicsBody* locals, size_t count);
};

}  // namespace physics_core
