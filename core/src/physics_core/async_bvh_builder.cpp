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
#include "physics_core/async_bvh_builder.h"
#include "physics_core/collision_system.h"
#include "physics_core/physics_thread_pool.h"
#include "physics_core/profile_macros.h"
#include <algorithm>
#include <limits>
#include <future>

namespace physics_core {

AsyncBVHBuilder::AsyncBVHBuilder(BVHCollisionSystem* collision_system,
                                 PhysicsThreadPool* thread_pool)
    : collision_system_(collision_system), thread_pool_(thread_pool) {
}

AsyncBVHBuilder::~AsyncBVHBuilder() {
    wait_for_build_completion();
}

bool AsyncBVHBuilder::schedule_update(
    const float* positions_x,
    const float* positions_y,
    const float* positions_z,
    const float* radii,
    const uint32_t* entity_ids,
    size_t count,
    bool force_rebuild) noexcept {

    PHYSICS_PROFILE_FUNCTION();

    bool swapped = false;

    if (back_ready_.load(std::memory_order_acquire)) {
        swap_buffers_internal();
        swapped = true;
    }

    if (!build_in_progress_.load(std::memory_order_relaxed)) {
        BuildParams params;
        params.positions_x.assign(positions_x, positions_x + count);
        params.positions_y.assign(positions_y, positions_y + count);
        params.positions_z.assign(positions_z, positions_z + count);
        params.radii.assign(radii, radii + count);
        params.entity_ids.assign(entity_ids, entity_ids + count);
        params.old_version = front_version_.load(std::memory_order_relaxed);
        params.force_rebuild = force_rebuild;
        params.entity_to_input_index.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            params.entity_to_input_index[params.entity_ids[i]] = i;
        }

        if (collision_system_) {
            collision_system_->snapshot_active_bvh(params.base_bvh);
        }

        launch_async_build(std::move(params));
    }

    return swapped;
}

void AsyncBVHBuilder::wait_for_build_completion() noexcept {
    if (build_future_.valid()) {
        build_future_.wait();
    }
}

void AsyncBVHBuilder::swap_buffers_internal() noexcept {
    PHYSICS_PROFILE_SCOPE("BVH.SwapBuffers");

    back_ready_.store(false, std::memory_order_release);
    if (collision_system_) {
        collision_system_->swap_internal_buffers(back_);
    }
    front_version_.fetch_add(1, std::memory_order_release);
    stats_.total_buffer_swaps++;
    back_.clear_but_keep_capacity();
}

void AsyncBVHBuilder::launch_async_build(const BuildParams& params) noexcept {
    PHYSICS_PROFILE_SCOPE("BVH.LaunchAsync");

    if (build_in_progress_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    auto promise = std::make_shared<std::promise<void>>();
    build_future_ = promise->get_future();

    thread_pool_->submit_task([this, params = std::move(params), promise]() mutable {
        rebuild_back_tree_async(std::move(params));
        promise->set_value();
    });

    stats_.total_async_builds++;
}

void AsyncBVHBuilder::rebuild_back_tree_async(const BuildParams& params) noexcept {
    PHYSICS_PROFILE_FUNCTION();

    if (params.force_rebuild || should_full_rebuild(params)) {
        PHYSICS_PROFILE_SCOPE("BVH.FullRebuild");
        collision_system_->build_tree_simd_into(
            back_,
            params.positions_x.data(),
            params.positions_y.data(),
            params.positions_z.data(),
            params.radii.data(),
            params.entity_ids.data(),
            params.positions_x.size()
        );
    } else {
        PHYSICS_PROFILE_SCOPE("BVH.Refit");
        collision_system_->refit_bvh_into(
            back_,
            params.base_bvh,
            params.positions_x.data(),
            params.positions_y.data(),
            params.positions_z.data(),
            params.radii.data(),
            params.entity_ids.data(),
            params.positions_x.size()
        );
    }

    back_.version = params.old_version + 1;
    back_ready_.store(true, std::memory_order_release);
    build_in_progress_.store(false, std::memory_order_release);
}

bool AsyncBVHBuilder::should_full_rebuild(const BuildParams& params) const noexcept {
    if (params.base_bvh.leaf_count == 0) {
        return true;
    }
    if (params.base_bvh.leaf_count != params.positions_x.size()) {
        return true;
    }
    return false;
}

void AsyncBVHBuilder::refit_internal_nodes(BVHData& target, size_t leaf_count) noexcept {
    PHYSICS_PROFILE_SCOPE("BVH.RefitInternal");

    if (target.nodes_.empty()) {
        return;
    }

    for (int node_idx = static_cast<int>(target.nodes_.size()) - 1; node_idx >= 0; --node_idx) {
        BVHNode& node = target.nodes_[node_idx];
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

            if (left < target.nodes_.size()) {
                const BVHNode& left_node = target.nodes_[left];
                for (size_t i = 0; i < BVH_BATCH_SIZE; ++i) {
                    min_x = std::min(min_x, left_node.min_x[i]);
                    min_y = std::min(min_y, left_node.min_y[i]);
                    min_z = std::min(min_z, left_node.min_z[i]);
                    max_x = std::max(max_x, left_node.max_x[i]);
                    max_y = std::max(max_y, left_node.max_y[i]);
                    max_z = std::max(max_z, left_node.max_z[i]);
                }
            }

            if (right < target.nodes_.size()) {
                const BVHNode& right_node = target.nodes_[right];
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

    stats_.total_refits++;
}

}  // namespace physics_core
