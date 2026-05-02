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
#include "local_coordinate_frame.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <shared_mutex>

namespace physics_core {

// ============================================================================
// World Coordinate System (WCS) Management
// ============================================================================

/**
 * @class WorldSpaceManager
 * @brief Manages global coordinate system and transformations between local frames
 * 
 * Criteria:
 * - Support coordinate range ±1e15 meters
 * - Transformation error < 1e-9 meters
 * - Support 10^6 local coordinate systems simultaneously
 */
class WorldSpaceManager {
public:
    using LocalFramePtr = std::shared_ptr<class LocalCoordinateFrame>;

    WorldSpaceManager();
    ~WorldSpaceManager();

    // ========== Frame Management ==========

    /**
     * Create a new local coordinate frame in world space
     * @param position Global position of the frame origin
     * @param orientation Rotation matrix (3x3)
     * @param scale Uniform scale factor
     * @param parent_id ID of parent frame (0 for world root)
     * @return ID of the created frame
     */
    EntityID create_local_frame(
        const Vec3& position,
        const Mat3x3& orientation = Mat3x3::identity(),
        double scale = 1.0,
        EntityID parent_id = 0
    );

    /**
     * Remove a local coordinate frame
     * @param frame_id ID of frame to remove
     */
    void destroy_local_frame(EntityID frame_id);

    /**
     * Get a local coordinate frame by ID
     * @param frame_id ID of the frame
     * @return Pointer to frame (nullptr if not found)
     */
    LocalFramePtr get_frame(EntityID frame_id);

    // ========== Coordinate Transformations ==========

    /**
     * Transform point from world space to local frame space
     * @param world_point Point in world coordinates
     * @param frame_id Target local frame ID (0 for world frame)
     * @return Point in local frame coordinates
     */
    Vec3 world_to_local(const Vec3& world_point, EntityID frame_id);

    /**
     * Transform point from local frame space to world space
     * @param local_point Point in local frame coordinates
     * @param frame_id Source local frame ID
     * @return Point in world coordinates
     */
    Vec3 local_to_world(const Vec3& local_point, EntityID frame_id);

    /**
     * Transform vector (direction) from world to local space
     * @param world_vector Vector in world coordinates
     * @param frame_id Target local frame ID
     * @return Vector in local frame coordinates
     */
    Vec3 world_vector_to_local(const Vec3& world_vector, EntityID frame_id);

    /**
     * Get the transformation matrix from world to local space
     * @param frame_id Target local frame ID
     * @return 4x4 transformation matrix
     */
    Mat4x4 get_world_to_local_matrix(EntityID frame_id);

    /**
     * Get the transformation matrix from local to world space
     * @param frame_id Source local frame ID
     * @return 4x4 transformation matrix
     */
    Mat4x4 get_local_to_world_matrix(EntityID frame_id);

    // ========== Frame Hierarchy ==========

    /**
     * Set parent frame for a frame
     * @param frame_id Frame to reparent
     * @param parent_id New parent frame ID
     */
    void set_parent_frame(EntityID frame_id, EntityID parent_id);

    /**
     * Get parent frame ID
     * @param frame_id Child frame ID
     * @return Parent frame ID (0 if root)
     */
    EntityID get_parent_frame(EntityID frame_id);

    /**
     * Get all children of a frame
     * @param frame_id Parent frame ID
     * @return Vector of child frame IDs
     */
    std::vector<EntityID> get_children_frames(EntityID frame_id);

    /**
     * Get frame depth in hierarchy (0 = root)
     * @param frame_id Frame to check
     * @return Depth level
     */
    int get_frame_depth(EntityID frame_id);

    // ========== Statistics ==========

    /**
     * Get number of active local frames
     * @return Number of frames
     */
    size_t get_frame_count() const;

    /**
     * Get memory usage in bytes
     * @return Memory usage
     */
    size_t get_memory_usage() const;

private:
    std::unordered_map<EntityID, LocalFramePtr> frames_;
    std::unordered_map<EntityID, EntityID> parent_hierarchy_;
    std::unordered_map<EntityID, std::vector<EntityID>> children_hierarchy_;
    EntityID next_frame_id_;
    mutable std::shared_mutex frames_mutex_;

    // Helper methods
    void invalidate_transform_cache(EntityID frame_id);
    Mat4x4 compute_world_to_local_matrix_recursive(EntityID frame_id);
};

}  // namespace physics_core
