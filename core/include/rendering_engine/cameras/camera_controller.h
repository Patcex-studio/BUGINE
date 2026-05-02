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

#include "../types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <chrono>

// Forward declare GLFW window
struct GLFWwindow;

namespace rendering_engine {

/**
 * @brief Free camera controller with mouse/keyboard input
 *
 * Implements a standard FPS-style camera with:
 * - WASD + QE for movement (6 DoF)
 * - Mouse look for rotation
 * - Delta-time based motion for smooth frame-rate independence
 * - Vulkan-specific matrix conventions (RH_ZO, Y-down in NDC)
 *
 * Critical notes (from rendering hints):
 * 1. Uses glm::perspectiveRH_ZO for right-handed Z-forward
 * 2. Inverts projection[1][1] for Vulkan Y-down NDC
 * 3. Captures cursor with GLFW_CURSOR_DISABLED for input
 * 4. Uses std::chrono for accurate delta time
 * 5. Stores yaw/pitch separately, not quaternions
 */
class CameraController {
public:
    // Camera state
    Camera camera_;

    // Movement parameters
    float move_speed_ = 5.0f;        // units/second
    float mouse_sensitivity_ = 0.002f; // radians per pixel

    // Camera angles (in radians)
    float yaw_ = -glm::half_pi<float>();  // Starts looking along +Z
    float pitch_ = 0.0f;                    // Horizontal plane initially

    // Mouse state tracking
    glm::vec2 last_mouse_pos_{0.0f, 0.0f};
    bool first_mouse_ = true;        // Skip first frame to avoid jump
    bool cursor_captured_ = false;   // Tracks if cursor is disabled

    // Timing
    std::chrono::steady_clock::time_point last_frame_time_;

public:
    CameraController() = default;
    ~CameraController() = default;

    /**
     * Initialize camera controller
     * @param window GLFW window for cursor capture
     * @param initial_position Starting camera position
     * @param fov_degrees Vertical FOV in degrees
     * @param aspect_ratio Width/Height ratio
     */
    void Init(GLFWwindow* window,
              glm::vec3 initial_position = {0.0f, 0.0f, 5.0f},
              float fov_degrees = 45.0f,
              float aspect_ratio = 16.0f / 9.0f);

    /**
     * Update camera based on input and elapsed time
     * Must be called every frame AFTER glfwPollEvents()
     * @param window GLFW window for input polling
     * @param out_delta_time Optional output for frame delta time
     * @return Calculated delta time in seconds
     */
    float Update(GLFWwindow* window, float* out_delta_time = nullptr);

    /**
     * Toggle cursor capture (for UI/menu support)
     * @param window GLFW window
     */
    void ToggleCursorCapture(GLFWwindow* window);

    /**
     * Set camera position explicitly
     */
    void SetPosition(const glm::vec3& pos) {
        camera_.position = pos;
    }

    /**
     * Get camera reference
     */
    const Camera& GetCamera() const {
        return camera_;
    }

    /**
     * Get mutable camera reference
     */
    Camera& GetCamera() {
        return camera_;
    }

    /**
     * Set move speed (units/second)
     */
    void SetMoveSpeed(float speed) {
        move_speed_ = speed;
    }

    /**
     * Set mouse sensitivity (radians/pixel)
     */
    void SetMouseSensitivity(float sensitivity) {
        mouse_sensitivity_ = sensitivity;
    }

private:
    /**
     * Update camera direction vectors from yaw/pitch angles
     */
    void UpdateDirectionVectors();

    /**
     * Update view and projection matrices
     */
    void UpdateMatrices();

    /**
     * Handle keyboard input (WASD + QE)
     * @param window GLFW window
     * @param delta_time Frame delta time in seconds
     */
    void HandleKeyboardInput(GLFWwindow* window, float delta_time);

    /**
     * Handle mouse input for camera rotation
     * @param window GLFW window
     */
    void HandleMouseInput(GLFWwindow* window);
};

} // namespace rendering_engine
