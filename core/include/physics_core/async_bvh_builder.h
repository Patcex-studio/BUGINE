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
#include <atomic>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

namespace physics_core {

// ============================================================================
// AsyncBVHBuilder: Manages double-buffered BVH with asynchronous updates
// ============================================================================

// Forward declaration
class BVHCollisionSystem;
class PhysicsThreadPool;

/**
 * BVHData: Container for a complete BVH tree instance
 * Used for double-buffering: one instance is active (front), 
 * another is being built in background (back)
 */
struct BVHData {
    std::vector<class BVHNode> nodes_;           // BVH tree nodes
    std::vector<uint32_t> leaf_indices_;         // Mapping to entity IDs
    std::vector<float> object_aabb_min_x_, object_aabb_min_y_, object_aabb_min_z_;
    std::vector<float> object_aabb_max_x_, object_aabb_max_y_, object_aabb_max_z_;
    std::vector<float> object_radii_;
    std::vector<uint32_t> object_entity_ids_;   // Input entity IDs for refit lookup
    
    size_t node_count = 0;
    size_t leaf_count = 0;
    uint64_t version = 0;  // For detecting stale data
    
    // Light-weight copy of only structure (not leaf data)
    void copy_structure(const BVHData& src) noexcept {
        nodes_ = src.nodes_;
        leaf_indices_ = src.leaf_indices_;
        node_count = src.node_count;
        leaf_count = src.leaf_count;
    }
    
    void clear_but_keep_capacity() noexcept {
        nodes_.clear();
        leaf_indices_.clear();
        // AABBs cleared separately
        object_aabb_min_x_.clear();
        object_aabb_min_y_.clear();
        object_aabb_min_z_.clear();
        object_aabb_max_x_.clear();
        object_aabb_max_y_.clear();
        object_aabb_max_z_.clear();
        object_radii_.clear();
        object_entity_ids_.clear();
        node_count = 0;
        leaf_count = 0;
    }
};

/**
 * BuildParams: Parameters captured for background build task
 * Minimal copy to avoid holding references to mutable data
 */
struct BuildParams {
    std::vector<float> positions_x, positions_y, positions_z;
    std::vector<float> radii;
    std::vector<uint32_t> entity_ids;
    std::unordered_map<uint32_t, size_t> entity_to_input_index;
    BVHData base_bvh;
    uint64_t old_version = 0;
    bool force_rebuild = false;
};

class AsyncBVHBuilder {
public:
    AsyncBVHBuilder(BVHCollisionSystem* collision_system, PhysicsThreadPool* thread_pool);
    ~AsyncBVHBuilder();

    // ========================================================================
    // Main API: Called from PhysicsCore::update()
    // ========================================================================

    /**
     * schedule_update: Check if previous async build is done, and if so,
     * swap buffers. Then schedule a new background build if needed.
     * 
     * This is called EVERY FRAME from the main thread and is lock-free.
     * 
     * @return true if swap occurred (BVH was updated this frame)
     */
    bool schedule_update(
        const float* positions_x,
        const float* positions_y,
        const float* positions_z,
        const float* radii,
        const uint32_t* entity_ids,
        size_t count,
        bool force_rebuild = false
    ) noexcept;

    /**
     * get_active_bvh: Returns the current active (front) BVH data.
     * Safe to call from any thread, returns consistent snapshot.
     */
    const BVHData& get_active_bvh() const noexcept { 
        return front_; 
    }

    /**
     * get_bvh_version: Returns current version number.
     * Can be used to detect if BVH changed since last query.
     */
    uint64_t get_bvh_version() const noexcept {
        return front_version_.load(std::memory_order_acquire);
    }

    /**
     * wait_for_build_completion: Waits for any in-progress background build.
     * Useful before shutdown or when you absolutely need synchronized state.
     */
    void wait_for_build_completion() noexcept;

    /**
     * is_build_in_progress: Check without waiting if background task is active.
     */
    bool is_build_in_progress() const noexcept {
        return build_in_progress_.load(std::memory_order_relaxed);
    }

    // ========================================================================
    // Statistics and Diagnostics
    // ========================================================================

    struct Stats {
        size_t total_buffer_swaps = 0;
        size_t total_async_builds = 0;
        size_t total_refits = 0;
        double last_swap_time_ms = 0.0;
    };

    Stats get_stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = {}; }

private:
    friend class BVHCollisionSystem;

    // Reference to the collision system for building
    BVHCollisionSystem* collision_system_;
    PhysicsThreadPool* thread_pool_;

    // ========================================================================
    // Double-Buffered BVH Data
    // ========================================================================
    
    // Active BVH (read-only from main thread)
    BVHData front_;
    
    // Being built in background
    BVHData back_;

    // ========================================================================
    // Synchronization (Atomic, Lock-Free)
    // ========================================================================
    
    // Signals that back_ is ready for swapping
    std::atomic<bool> back_ready_{false};
    
    // Signals that a build task is currently in flight
    std::atomic<bool> build_in_progress_{false};
    
    // Version counter (incremented on each swap)
    std::atomic<uint64_t> front_version_{0};
    
    // Future for tracking background task
    std::future<void> build_future_;

    // ========================================================================
    // Configuration
    // ========================================================================
    
    struct Config {
        float rebuild_threshold = 0.3f;  // >30% change → full rebuild
        size_t min_objects_for_async = 50;  // <50 objects → sync build
    } config_;

    // ========================================================================
    // Private Methods
    // ========================================================================
    
    /**
     * swap_buffers_internal: Atomic swap of front_ and back_.
     * MUST be called from main thread only.
     */
    void swap_buffers_internal() noexcept;

    /**
     * launch_async_build: Start background task to build/refit back_.
     * Called after copying input data to BuildParams.
     */
    void launch_async_build(const BuildParams& params) noexcept;

    /**
     * rebuild_back_tree_async: Worker function for background thread.
     * Builds or refits back_ depending on change magnitude.
     */
    void rebuild_back_tree_async(const BuildParams& params) noexcept;

    /**
     * should_full_rebuild: Heuristic to decide between full rebuild or refit.
     */
    bool should_full_rebuild(const BuildParams& params) const noexcept;

    /**
     * refit_internal_nodes: Parallel bottom-up refit of internal BVH nodes.
     */
    void refit_internal_nodes(BVHData& target, size_t leaf_count) noexcept;

    // Statistics
    Stats stats_;
};

}  // namespace physics_core
