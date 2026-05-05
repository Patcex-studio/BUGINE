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
#include "physics_core/hybrid_precision.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <mutex>

namespace physics_core {

// ============================================================================
// LocalPhysicsBody helper functions implementation
// ============================================================================

namespace physics_core {
namespace local_body_utils {

LocalPhysicsBody create(
    const float pos[3], const float vel[3], const float acc[3],
    float mass_inv, uint32_t type
) {
    LocalPhysicsBody body;
    
    // Create SIMD vectors with x, y, z + 0.0f padding
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

void extract_position(const LocalPhysicsBody& body, float out[3]) {
    // Extract xyz from pos_vec (stored in lower 128 bits)
    __m128 low = _mm256_castps256_ps128(body.pos_vec);
    out[0] = _mm_cvtss_f32(low);
    out[1] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(1, 1, 1, 1)));
    out[2] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(2, 2, 2, 2)));
}

void extract_velocity(const LocalPhysicsBody& body, float out[3]) {
    __m128 low = _mm256_castps256_ps128(body.vel_vec);
    out[0] = _mm_cvtss_f32(low);
    out[1] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(1, 1, 1, 1)));
    out[2] = _mm_cvtss_f32(_mm_shuffle_ps(low, low, _MM_SHUFFLE(2, 2, 2, 2)));
}

} // namespace local_body_utils
} // namespace physics_core

// ============================================================================
// HybridPrecisionSystem Implementation
// ============================================================================

HybridPrecisionSystem::HybridPrecisionSystem()
    : origin_threshold_(DEFAULT_ORIGIN_THRESHOLD),
      fast_projectile_enabled_(true),
      next_subscription_id_(1),
      total_syncs_(0),
      events_published_(0) {
    detect_simd_capabilities();
    current_frame_.frame_generation = 1;
    current_frame_.last_used.store(0);
    
    // Инициализируем origins
    active_origin_.origin = Vec3(0, 0, 0);
    active_origin_.version.store(0);
    pending_origin_.origin = Vec3(0, 0, 0);
    pending_origin_.version.store(0);
}

HybridPrecisionSystem::~HybridPrecisionSystem() = default;

// ========== Configuration ==========

void HybridPrecisionSystem::set_simulation_threshold(double meters) {
    origin_threshold_ = meters;
}

void HybridPrecisionSystem::enable_fast_projectile_mode(bool enabled) {
    fast_projectile_enabled_ = enabled;
}

size_t HybridPrecisionSystem::get_active_objects_count() const {
    std::shared_lock<std::shared_mutex> lock(global_objects_mutex_);
    return global_objects_.size();
}

size_t HybridPrecisionSystem::get_active_projectiles_count() const {
    std::shared_lock<std::shared_mutex> lock(projectiles_mutex_);
    return projectiles_.size();
}

void HybridPrecisionSystem::force_full_resync() {
    std::unique_lock<std::shared_mutex> lock(frame_mutex_);
    current_frame_.frame_generation++;
    current_frame_.needs_resync.store(true);
}

void HybridPrecisionSystem::request_origin_shift(const Vec3& new_origin) {
    // Атомарно устанавливаем pending origin
    pending_origin_.origin = {new_origin.x, new_origin.y, new_origin.z};
    pending_origin_.version.fetch_add(1, std::memory_order_release);
}

void HybridPrecisionSystem::apply_pending_shift() {
    uint64_t pending_ver = pending_origin_.version.load(std::memory_order_acquire);
    uint64_t active_ver = active_origin_.version.load(std::memory_order_relaxed);
    
    if (pending_ver != active_ver) {
<<<<<<< HEAD
        // Копируем origin
        active_origin_.origin = pending_origin_.origin;
        // Атомарно обновляем версию
        active_origin_.version.store(pending_ver, std::memory_order_release);
        
        // Здесь можно добавить двойную буферизацию локальных координат
        // Для простоты пропустим
=======
        // Store old origin for offset calculation
        Vec3 old_origin = active_origin_.origin;
        
        // Apply new origin
        active_origin_.origin = pending_origin_.origin;
        
        // Calculate offset to apply to all local bodies
        Vec3 offset = active_origin_.origin - old_origin;
        
        // Atomically update version
        active_origin_.version.store(pending_ver, std::memory_order_release);
        
        // Shift all global objects by offset
        {
            std::unique_lock<std::shared_mutex> lock(global_objects_mutex_);
            for (auto& [id, obj] : global_objects_) {
                // Update global position by subtracting offset (origin moved, so local coords appear shifted back)
                obj.position -= offset;
                
                // Mark as needing resync to local frame
                {
                    std::unique_lock<std::shared_mutex> local_lock(local_objects_mutex_);
                    if (local_object_cache_.count(id)) {
                        local_object_cache_[id].needs_update = true;
                    }
                }
            }
        }
        
        // Shift all projectiles
        {
            std::unique_lock<std::shared_mutex> lock(projectiles_mutex_);
            for (auto& proj : projectiles_) {
                proj.position -= offset;
            }
        }
        
        total_syncs_++;
>>>>>>> c308d63 (Helped the rabbits find a home)
    }
}

bool HybridPrecisionSystem::validate_consistency(float epsilon) const {
    std::shared_lock<std::shared_mutex> lock(global_objects_mutex_);
    bool consistent = true;
    
    for (const auto& [id, obj] : global_objects_) {
        // Предполагаем, что локальные координаты хранятся где-то
        // Заглушка: считаем consistent
    }
    
    return consistent;
}

// ========== Global Storage Management ==========

bool HybridPrecisionSystem::register_global_object(const GlobalObject& obj) {
    std::unique_lock<std::shared_mutex> lock(global_objects_mutex_);
    
    if (global_objects_.size() >= 1000000) {
        return false; // Exceed limit
    }
    
    global_objects_[obj.id] = obj;
    return true;
}

void HybridPrecisionSystem::update_global_position(EntityID id, const double pos[3]) {
    std::unique_lock<std::shared_mutex> lock(global_objects_mutex_);
    
    auto it = global_objects_.find(id);
    if (it != global_objects_.end()) {
        it->second.global_pos[0] = pos[0];
        it->second.global_pos[1] = pos[1];
        it->second.global_pos[2] = pos[2];
        it->second.sync_generation++;
    }
}

const GlobalObject* HybridPrecisionSystem::get_global_object(EntityID id) const {
    std::shared_lock<std::shared_mutex> lock(global_objects_mutex_);
    
    auto it = global_objects_.find(id);
    return (it != global_objects_.end()) ? &it->second : nullptr;
}

void HybridPrecisionSystem::unregister_global_object(EntityID id) {
    std::unique_lock<std::shared_mutex> lock(global_objects_mutex_);
    global_objects_.erase(id);
}

// ========== Reference Frame Management ==========

void HybridPrecisionSystem::update_reference_frame(
    const double player_camera_pos[3],
    double threshold
) {
    std::unique_lock<std::shared_mutex> lock(frame_mutex_);
    
    double distance = current_frame_.distance_from_origin(player_camera_pos);
    
    if (distance > threshold) {
        // Calculate grid-snapped origin
        double new_origin[3];
        ReferenceFrame::grid_snap(player_camera_pos, new_origin);
        
        // Store old origin for event
        double old_origin[3];
        old_origin[0] = current_frame_.origin[0];
        old_origin[1] = current_frame_.origin[1];
        old_origin[2] = current_frame_.origin[2];
        
        // Update origin and increment generation
        current_frame_.origin[0] = new_origin[0];
        current_frame_.origin[1] = new_origin[1];
        current_frame_.origin[2] = new_origin[2];
        current_frame_.frame_generation++;
        current_frame_.needs_resync.store(true);
        
        // Shift all global coordinates from old frame to new frame
        shift_all_origins(old_origin, new_origin);
        
        // Emit event
        PrecisionEvent evt;
        evt.type = PrecisionEventType::ORIGIN_SHIFTED;
        evt.old_origin[0] = old_origin[0];
        evt.old_origin[1] = old_origin[1];
        evt.old_origin[2] = old_origin[2];
        evt.new_origin[0] = new_origin[0];
        evt.new_origin[1] = new_origin[1];
        evt.new_origin[2] = new_origin[2];
        evt.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        
        emit_event(evt);
    }
    
    current_frame_.last_used.store(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

const ReferenceFrame& HybridPrecisionSystem::get_reference_frame() const {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    return current_frame_;
}

void HybridPrecisionSystem::set_reference_frame_origin(const double origin[3]) {
    std::unique_lock<std::shared_mutex> lock(frame_mutex_);
    
    double old_origin[3];
    old_origin[0] = current_frame_.origin[0];
    old_origin[1] = current_frame_.origin[1];
    old_origin[2] = current_frame_.origin[2];
    
    current_frame_.origin[0] = origin[0];
    current_frame_.origin[1] = origin[1];
    current_frame_.origin[2] = origin[2];
    current_frame_.frame_generation++;
    current_frame_.needs_resync.store(true);
    
    shift_all_origins(old_origin, origin);
    
    PrecisionEvent evt;
    evt.type = PrecisionEventType::ORIGIN_SHIFTED;
    evt.old_origin[0] = old_origin[0];
    evt.old_origin[1] = old_origin[1];
    evt.old_origin[2] = old_origin[2];
    evt.new_origin[0] = origin[0];
    evt.new_origin[1] = origin[1];
    evt.new_origin[2] = origin[2];
    evt.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    emit_event(evt);
}

// ========== SIMD Synchronization ==========

size_t HybridPrecisionSystem::sync_global_to_local(
    const GlobalObject* globals,
    LocalPhysicsBody* locals,
    size_t count
) {
    if (count == 0) return 0;
    
    total_syncs_++;
    
    // Use SIMD if available and count is large enough
    if (simd_state_.has_avx2 && count >= 4) {
        return batch_sync_g2l_simd(globals, locals, count);
    }

    PrecisionEvent evt;
    evt.type = PrecisionEventType::FALLBACK_TO_SCALAR;
    evt.entity_id = 0;
    evt.velocity_magnitude = 0.0;
    evt.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    publish_event(evt);
    return batch_sync_g2l_scalar(globals, locals, count);
}

size_t HybridPrecisionSystem::sync_local_to_global(
    GlobalObject* globals,
    const LocalPhysicsBody* locals,
    size_t count
) {
    if (count == 0) return 0;
    
    total_syncs_++;
    
    // Use SIMD if available and count is large enough
    if (simd_state_.has_avx2 && count >= 4) {
        return batch_sync_l2g_simd(globals, locals, count);
    }

    PrecisionEvent evt;
    evt.type = PrecisionEventType::FALLBACK_TO_SCALAR;
    evt.entity_id = 0;
    evt.velocity_magnitude = 0.0;
    evt.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    publish_event(evt);
    return batch_sync_l2g_scalar(globals, locals, count);
}

void HybridPrecisionSystem::sync_single_global_to_local(
    const GlobalObject& global_obj,
    LocalPhysicsBody& local_body
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    // Convert double position to local float coordinates
    double local_pos_d[3];
    local_pos_d[0] = global_obj.global_pos[0] - current_frame_.origin[0];
    local_pos_d[1] = global_obj.global_pos[1] - current_frame_.origin[1];
    local_pos_d[2] = global_obj.global_pos[2] - current_frame_.origin[2];
    
    // Convert double to float
    float local_pos_f[3];
    local_pos_f[0] = static_cast<float>(local_pos_d[0]);
    local_pos_f[1] = static_cast<float>(local_pos_d[1]);
    local_pos_f[2] = static_cast<float>(local_pos_d[2]);
    
    // Load into SIMD register
    local_body.pos_vec = _mm256_setr_ps(
        local_pos_f[0], local_pos_f[1], local_pos_f[2], 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    );
    
    local_body.frame_generation = current_frame_.frame_generation;
    local_body_utils::mark_synced(local_body);
}

void HybridPrecisionSystem::sync_single_local_to_global(
    const LocalPhysicsBody& local_body,
    GlobalObject& global_obj
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    // Extract float position from SIMD
    float local_pos_f[3];
    local_body_utils::extract_position(local_body, local_pos_f);
    
    // Convert float to double and add origin
    global_obj.global_pos[0] = static_cast<double>(local_pos_f[0]) + current_frame_.origin[0];
    global_obj.global_pos[1] = static_cast<double>(local_pos_f[1]) + current_frame_.origin[1];
    global_obj.global_pos[2] = static_cast<double>(local_pos_f[2]) + current_frame_.origin[2];
    
    global_obj.sync_generation++;
}

// ========== Fast Projectile Handling ==========

bool HybridPrecisionSystem::register_projectile(const HighPrecisionProjectile& proj) {
    if (!fast_projectile_enabled_) return false;
    
    std::unique_lock<std::shared_mutex> lock(projectiles_mutex_);
    
    if (projectiles_.size() >= MAX_PROJECTILES) {
        return false;
    }
    
    projectiles_[proj.id] = proj;
    projectiles_[proj.id].creation_time = 
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    float magnitude = projectiles_[proj.id].velocity_magnitude();
    if (magnitude >= FAST_OBJECT_VELOCITY_THRESHOLD) {
        PrecisionEvent proj_event;
        proj_event.type = PrecisionEventType::HIGH_PRECISION_NEEDED;
        proj_event.entity_id = proj.id;
        proj_event.velocity_magnitude = magnitude;
        proj_event.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        publish_event(proj_event);
    }
    
    return true;
}

void HybridPrecisionSystem::update_projectile(EntityID id, double dt) {
    std::unique_lock<std::shared_mutex> lock(projectiles_mutex_);
    
    auto it = projectiles_.find(id);
    if (it != projectiles_.end()) {
        it->second.elapsed_time += dt;
    }
}

bool HybridPrecisionSystem::get_projectile_position(EntityID id, double out[3]) const {
    std::shared_lock<std::shared_mutex> lock(projectiles_mutex_);
    
    auto it = projectiles_.find(id);
    if (it != projectiles_.end()) {
        it->second.compute_position(out);
        return true;
    }
    return false;
}

void HybridPrecisionSystem::unregister_projectile(EntityID id) {
    std::unique_lock<std::shared_mutex> lock(projectiles_mutex_);
    projectiles_.erase(id);
}

// ========== Event System ==========

uint64_t HybridPrecisionSystem::subscribe_events(const EventCallback& callback) {
    std::unique_lock<std::shared_mutex> lock(event_mutex_);
    
    uint64_t subscription_id = next_subscription_id_++;
    event_subscribers_.push_back({subscription_id, callback});
    
    return subscription_id;
}

void HybridPrecisionSystem::unsubscribe_events(uint64_t subscription_id) {
    std::unique_lock<std::shared_mutex> lock(event_mutex_);
    
    event_subscribers_.erase(
        std::remove_if(event_subscribers_.begin(), event_subscribers_.end(),
                      [subscription_id](const EventSubscription& s) { return s.id == subscription_id; }),
        event_subscribers_.end()
    );
}

void HybridPrecisionSystem::publish_event(const PrecisionEvent& event) {
    emit_event(event);
}

// ========== Statistics & Debugging ==========

size_t HybridPrecisionSystem::get_memory_usage() const {
    size_t total = 0;
    
    {
        std::shared_lock<std::shared_mutex> lock(global_objects_mutex_);
        total += global_objects_.size() * sizeof(GlobalObject);
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(projectiles_mutex_);
        total += projectiles_.size() * sizeof(HighPrecisionProjectile);
    }
    
    return total;
}

HybridPrecisionSystem::SyncStats HybridPrecisionSystem::get_statistics() const {
    SyncStats stats;
    
    {
        std::shared_lock<std::shared_mutex> lock(global_objects_mutex_);
        stats.global_objects = global_objects_.size();
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(projectiles_mutex_);
        stats.projectiles = projectiles_.size();
    }
    
    stats.local_bodies = 0; // Not tracked in this implementation
    stats.events_published = events_published_.load();
    stats.total_syncs = total_syncs_.load();
    stats.last_origin_shift_distance = 0.0;
    
    return stats;
}

bool HybridPrecisionSystem::validate_consistency() const {
    std::shared_lock<std::shared_mutex> glock(global_objects_mutex_);
    std::shared_lock<std::shared_mutex> flock(frame_mutex_);
    
    // Check invariants from specification
    for (const auto& [id, obj] : global_objects_) {
        double local_pos[3];
        local_pos[0] = obj.global_pos[0] - current_frame_.origin[0];
        local_pos[1] = obj.global_pos[1] - current_frame_.origin[1];
        local_pos[2] = obj.global_pos[2] - current_frame_.origin[2];
        
        // Invariant: local_pos must remain within [-1000, +1000] meters
        if (std::abs(local_pos[0]) > 1000.0 || std::abs(local_pos[1]) > 1000.0 || std::abs(local_pos[2]) > 1000.0) {
            return false;
        }
    }
    
    return true;
}

// ========== SIMD Batch Operations ==========

size_t HybridPrecisionSystem::batch_sync_g2l_simd(
    const GlobalObject* globals,
    LocalPhysicsBody* locals,
    size_t count
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    __m256d origin_x = _mm256_set1_pd(current_frame_.origin[0]);
    __m256d origin_y = _mm256_set1_pd(current_frame_.origin[1]);
    __m256d origin_z = _mm256_set1_pd(current_frame_.origin[2]);
    
    size_t processed = 0;
    size_t full_batches = count / 4;
    
    for (size_t batch = 0; batch < full_batches; ++batch) {
        const GlobalObject* g = &globals[batch * 4];
        LocalPhysicsBody* l = &locals[batch * 4];
        
        __m256d gx = _mm256_setr_pd(
            g[0].global_pos[0], g[1].global_pos[0],
            g[2].global_pos[0], g[3].global_pos[0]
        );
        __m256d gy = _mm256_setr_pd(
            g[0].global_pos[1], g[1].global_pos[1],
            g[2].global_pos[1], g[3].global_pos[1]
        );
        __m256d gz = _mm256_setr_pd(
            g[0].global_pos[2], g[1].global_pos[2],
            g[2].global_pos[2], g[3].global_pos[2]
        );
        
        __m128 fx = _mm256_cvtpd_ps(_mm256_sub_pd(gx, origin_x));
        __m128 fy = _mm256_cvtpd_ps(_mm256_sub_pd(gy, origin_y));
        __m128 fz = _mm256_cvtpd_ps(_mm256_sub_pd(gz, origin_z));
        
        float x_array[4];
        float y_array[4];
        float z_array[4];
        _mm_storeu_ps(x_array, fx);
        _mm_storeu_ps(y_array, fy);
        _mm_storeu_ps(z_array, fz);
        
        for (int j = 0; j < 4; ++j) {
            __m256 pos = _mm256_insertf128_ps(_mm256_setzero_ps(),
                _mm_setr_ps(x_array[j], y_array[j], z_array[j], 0.0f), 0);
            l[j].pos_vec = pos;
            l[j].frame_generation = current_frame_.frame_generation;
            local_body_utils::mark_synced(l[j]);
        }
        
        processed += 4;
    }
    
    for (size_t i = full_batches * 4; i < count; ++i) {
        double lx = globals[i].global_pos[0] - current_frame_.origin[0];
        double ly = globals[i].global_pos[1] - current_frame_.origin[1];
        double lz = globals[i].global_pos[2] - current_frame_.origin[2];
        
        __m256 pos = _mm256_setr_ps(
            static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz), 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        );
        locals[i].pos_vec = pos;
        locals[i].frame_generation = current_frame_.frame_generation;
        local_body_utils::mark_synced(locals[i]);
        
        processed++;
    }
    
    return processed;
}

size_t HybridPrecisionSystem::batch_sync_l2g_simd(
    GlobalObject* globals,
    const LocalPhysicsBody* locals,
    size_t count
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    size_t processed = 0;
    
    for (size_t i = 0; i < count; ++i) {
        __m128 pos_low = _mm256_castps256_ps128(locals[i].pos_vec);
        __m256d local_d = _mm256_cvtps_pd(pos_low);
        double local_pos[4];
        _mm256_storeu_pd(local_pos, local_d);
        
        globals[i].global_pos[0] = local_pos[0] + current_frame_.origin[0];
        globals[i].global_pos[1] = local_pos[1] + current_frame_.origin[1];
        globals[i].global_pos[2] = local_pos[2] + current_frame_.origin[2];
        
        globals[i].sync_generation++;
        processed++;
    }
    
    return processed;
}

size_t HybridPrecisionSystem::batch_sync_g2l_scalar(
    const GlobalObject* globals,
    LocalPhysicsBody* locals,
    size_t count
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    size_t processed = 0;
    
    for (size_t i = 0; i < count; ++i) {
        double lx = globals[i].global_pos[0] - current_frame_.origin[0];
        double ly = globals[i].global_pos[1] - current_frame_.origin[1];
        double lz = globals[i].global_pos[2] - current_frame_.origin[2];
        
        __m256 pos = _mm256_setr_ps(
            static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz), 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        );
        
        locals[i].pos_vec = pos;
        locals[i].frame_generation = current_frame_.frame_generation;
        local_body_utils::mark_synced(locals[i]);
        
        processed++;
    }
    
    return processed;
}

size_t HybridPrecisionSystem::batch_sync_l2g_scalar(
    GlobalObject* globals,
    const LocalPhysicsBody* locals,
    size_t count
) {
    std::shared_lock<std::shared_mutex> lock(frame_mutex_);
    
    size_t processed = 0;
    
    for (size_t i = 0; i < count; ++i) {
        float local_pos_f[3];
        local_body_utils::extract_position(locals[i], local_pos_f);
        
        globals[i].global_pos[0] = static_cast<double>(local_pos_f[0]) + current_frame_.origin[0];
        globals[i].global_pos[1] = static_cast<double>(local_pos_f[1]) + current_frame_.origin[1];
        globals[i].global_pos[2] = static_cast<double>(local_pos_f[2]) + current_frame_.origin[2];
        
        globals[i].sync_generation++;
        processed++;
    }
    
    return processed;
}

// ========== Private Helper Methods ==========

void HybridPrecisionSystem::detect_simd_capabilities() {
    unsigned int eax, ebx, ecx, edx;
    
    // Check for AVX2 support
    eax = 7;
    ecx = 0;
    
    __asm__ __volatile__(
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (eax), "c" (ecx)
    );
    
    simd_state_.has_simd = true;
    simd_state_.has_avx2 = (ebx & (1 << 5)) != 0; // AVX2 bit
    simd_state_.has_avx512 = (ebx & (1 << 16)) != 0; // AVX-512F bit
}

void HybridPrecisionSystem::shift_all_origins(const double old_origin[3], const double new_origin[3]) {
    std::unique_lock<std::shared_mutex> lock(global_objects_mutex_);
    
    double dx = new_origin[0] - old_origin[0];
    double dy = new_origin[1] - old_origin[1];
    double dz = new_origin[2] - old_origin[2];
    
    for (auto& [id, obj] : global_objects_) {
        obj.global_pos[0] -= dx;
        obj.global_pos[1] -= dy;
        obj.global_pos[2] -= dz;
        obj.sync_generation++;
    }
}

void HybridPrecisionSystem::emit_event(const PrecisionEvent& event) {
    std::shared_lock<std::shared_mutex> lock(event_mutex_);
    
    events_published_++;
    
    for (auto& sub : event_subscribers_) {
        try {
            sub.callback(event);
        } catch (...) {
            // Silently ignore callback exceptions
        }
    }
}

}  // namespace physics_core
