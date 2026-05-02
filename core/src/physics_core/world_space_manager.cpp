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
#include "physics_core/world_space_manager.h"
#include "physics_core/local_coordinate_frame.h"
#include <mutex>

namespace physics_core {

WorldSpaceManager::WorldSpaceManager() : next_frame_id_(1) {
}

WorldSpaceManager::~WorldSpaceManager() = default;

// ========== Frame Management ==========

EntityID WorldSpaceManager::create_local_frame(
    const Vec3& position,
    const Mat3x3& orientation,
    double scale,
    EntityID parent_id
) {
    std::unique_lock<std::shared_mutex> lock(frames_mutex_);
    
    EntityID frame_id = next_frame_id_++;
    
    auto frame = std::make_shared<LocalCoordinateFrame>(frame_id, position, orientation, scale);
    frames_[frame_id] = frame;
    parent_hierarchy_[frame_id] = parent_id;
    
    if (parent_id != 0 && frames_.count(parent_id)) {
        children_hierarchy_[parent_id].push_back(frame_id);
    }
    
    return frame_id;
}

void WorldSpaceManager::destroy_local_frame(EntityID frame_id) {
    std::unique_lock<std::shared_mutex> lock(frames_mutex_);
    
    auto it = frames_.find(frame_id);
    if (it == frames_.end()) {
        return;
    }
    
    // Remove from parent's children
    EntityID parent = parent_hierarchy_[frame_id];
    if (parent != 0) {
        auto& siblings = children_hierarchy_[parent];
        siblings.erase(std::remove(siblings.begin(), siblings.end(), frame_id), siblings.end());
    }
    
    // Delete all children recursively
    auto children_it = children_hierarchy_.find(frame_id);
    if (children_it != children_hierarchy_.end()) {
        for (EntityID child : children_it->second) {
            destroy_local_frame(child);
        }
    }
    
    frames_.erase(it);
    parent_hierarchy_.erase(frame_id);
    children_hierarchy_.erase(frame_id);
}

WorldSpaceManager::LocalFramePtr WorldSpaceManager::get_frame(EntityID frame_id) {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    
    auto it = frames_.find(frame_id);
    return (it != frames_.end()) ? it->second : nullptr;
}

// ========== Coordinate Transformations ==========

Vec3 WorldSpaceManager::world_to_local(const Vec3& world_point, EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return world_point;
    }
    return frame->transform_point_to_local(world_point);
}

Vec3 WorldSpaceManager::local_to_world(const Vec3& local_point, EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return local_point;
    }
    return frame->transform_point_to_world(local_point);
}

Vec3 WorldSpaceManager::world_vector_to_local(const Vec3& world_vector, EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return world_vector;
    }
    return frame->transform_vector_to_local(world_vector);
}

Mat4x4 WorldSpaceManager::get_world_to_local_matrix(EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return Mat4x4::identity();
    }
    return frame->get_world_to_local_transform();
}

Mat4x4 WorldSpaceManager::get_local_to_world_matrix(EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return Mat4x4::identity();
    }
    return frame->get_local_to_world_transform();
}

// ========== Frame Hierarchy ==========

void WorldSpaceManager::set_parent_frame(EntityID frame_id, EntityID new_parent_id) {
    std::unique_lock<std::shared_mutex> lock(frames_mutex_);
    
    auto frame_it = frames_.find(frame_id);
    auto parent_it = frames_.find(new_parent_id);
    
    if (frame_it == frames_.end() || (new_parent_id != 0 && parent_it == frames_.end())) {
        return;
    }
    
    // Remove from old parent's children
    EntityID old_parent = parent_hierarchy_[frame_id];
    if (old_parent != 0) {
        auto& old_siblings = children_hierarchy_[old_parent];
        old_siblings.erase(std::remove(old_siblings.begin(), old_siblings.end(), frame_id), old_siblings.end());
    }
    
    // Add to new parent's children
    parent_hierarchy_[frame_id] = new_parent_id;
    if (new_parent_id != 0) {
        children_hierarchy_[new_parent_id].push_back(frame_id);
    }
    
    // Invalidate transform cache for this frame and all children
    invalidate_transform_cache(frame_id);
}

EntityID WorldSpaceManager::get_parent_frame(EntityID frame_id) {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    
    auto it = parent_hierarchy_.find(frame_id);
    return (it != parent_hierarchy_.end()) ? it->second : 0;
}

std::vector<EntityID> WorldSpaceManager::get_children_frames(EntityID frame_id) {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    
    auto it = children_hierarchy_.find(frame_id);
    return (it != children_hierarchy_.end()) ? it->second : std::vector<EntityID>();
}

int WorldSpaceManager::get_frame_depth(EntityID frame_id) {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    
    int depth = 0;
    EntityID current = frame_id;
    
    while (true) {
        auto it = parent_hierarchy_.find(current);
        if (it == parent_hierarchy_.end() || it->second == 0) {
            break;
        }
        current = it->second;
        depth++;
    }
    
    return depth;
}

// ========== Statistics ==========

size_t WorldSpaceManager::get_frame_count() const {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    return frames_.size();
}

size_t WorldSpaceManager::get_memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(frames_mutex_);
    
    size_t memory = 0;
    memory += sizeof(WorldSpaceManager);
    memory += frames_.size() * sizeof(LocalCoordinateFrame);
    memory += parent_hierarchy_.size() * (sizeof(EntityID) * 2);
    memory += children_hierarchy_.size() * (sizeof(EntityID) + sizeof(std::vector<EntityID>));
    
    for (const auto& pair : children_hierarchy_) {
        memory += pair.second.capacity() * sizeof(EntityID);
    }
    
    return memory;
}

// ========== Private Helpers ==========

void WorldSpaceManager::invalidate_transform_cache(EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return;
    }
    
    frame->invalidate_transform_cache();
    
    // Recursively invalidate all children
    auto children_it = children_hierarchy_.find(frame_id);
    if (children_it != children_hierarchy_.end()) {
        for (EntityID child : children_it->second) {
            invalidate_transform_cache(child);
        }
    }
}

Mat4x4 WorldSpaceManager::compute_world_to_local_matrix_recursive(EntityID frame_id) {
    auto frame = get_frame(frame_id);
    if (!frame) {
        return Mat4x4::identity();
    }
    
    EntityID parent_id = get_parent_frame(frame_id);
    if (parent_id == 0) {
        return frame->get_world_to_local_transform();
    }
    
    // Compose parent's world-to-local with frame's world-to-local
    Mat4x4 parent_matrix = compute_world_to_local_matrix_recursive(parent_id);
    Mat4x4 frame_matrix = frame->get_world_to_local_transform();
    
    return frame_matrix * parent_matrix;
}

}  // namespace physics_core
