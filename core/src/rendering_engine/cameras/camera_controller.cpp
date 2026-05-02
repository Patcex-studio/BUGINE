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
#include "rendering_engine/cameras/camera_controller.h"
#ifdef RENDERING_ENGINE_WITH_GLFW
#include <GLFW/glfw3.h>
#endif
#include <cmath>

namespace rendering_engine {

void CameraController::Init(GLFWwindow* window,
                            glm::vec3 initial_position,
                            float fov_degrees,
                            float aspect_ratio) {
    // Initialize camera structure
    camera_.position = initial_position;
    camera_.fov = glm::radians(fov_degrees);
    camera_.aspect_ratio = aspect_ratio;
    camera_.near_plane = 0.1f;
    camera_.far_plane = 1000.0f;

    // Set up initial direction (looking along +Z)
    yaw_ = -glm::half_pi<float>();
    pitch_ = 0.0f;
    UpdateDirectionVectors();

    // Capture cursor for FPS-style controls if GLFW is available
#ifdef RENDERING_ENGINE_WITH_GLFW
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    cursor_captured_ = true;
#else
    cursor_captured_ = false;
#endif
    first_mouse_ = true;

    // Initialize timing
    last_frame_time_ = std::chrono::steady_clock::now();

    // Calculate matrices
    UpdateMatrices();
}

float CameraController::Update(GLFWwindow* window, float* out_delta_time) {
    // Calculate delta time using steady_clock for precision
    auto now = std::chrono::steady_clock::now();
    float delta_time =
        std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;

    // Clamp delta time to a reasonable maximum (in case of frame drops)
    delta_time = glm::min(delta_time, 0.1f); // Cap at 100ms

    if (out_delta_time) {
        *out_delta_time = delta_time;
    }

    // Handle input
    HandleMouseInput(window);
    HandleKeyboardInput(window, delta_time);

    // Update matrices based on new camera state
    UpdateMatrices();

    return delta_time;
}

void CameraController::HandleMouseInput(GLFWwindow* window) {
#ifdef RENDERING_ENGINE_WITH_GLFW
    if (!cursor_captured_) {
        return;
    }

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (first_mouse_) {
        last_mouse_pos_ = {static_cast<float>(xpos), static_cast<float>(ypos)};
        first_mouse_ = false;
        return;
    }

    // Calculate mouse delta
    float dx = static_cast<float>(xpos - last_mouse_pos_.x) * mouse_sensitivity_;
    float dy = static_cast<float>(ypos - last_mouse_pos_.y) * mouse_sensitivity_;

    last_mouse_pos_ = {static_cast<float>(xpos), static_cast<float>(ypos)};

    // Update angles
    yaw_ += dx;
    pitch_ = glm::clamp(pitch_ - dy,                                    // Move down = look down = negative pitch
                        -glm::half_pi<float>() + 0.01f,   // Clamp to avoid gimbal lock
                        glm::half_pi<float>() - 0.01f);

    // Recalculate direction vectors from new angles
    UpdateDirectionVectors();
#else
    (void)window;
#endif
}

void CameraController::HandleKeyboardInput(GLFWwindow* window,
                                          float delta_time) {
#ifdef RENDERING_ENGINE_WITH_GLFW
    const float speed = move_speed_ * delta_time;

    // Forward/backward (W/S)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera_.position += camera_.forward * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera_.position -= camera_.forward * speed;
    }

    // Left/right (A/D)
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera_.position -= camera_.right * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera_.position += camera_.right * speed;
    }

    // Up/down (Q/E)
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        camera_.position -= camera_.up * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        camera_.position += camera_.up * speed;
    }

    // Toggle cursor capture with ESC
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        ToggleCursorCapture(window);
    }
#else
    (void)window;
    (void)delta_time;
#endif
}

void CameraController::UpdateDirectionVectors() {
    // Right-handed coordinate system: +Z forward, Y up, X right
    // Compute forward direction from yaw and pitch
    camera_.forward.x = std::cos(yaw_) * std::cos(pitch_);
    camera_.forward.y = std::sin(pitch_);
    camera_.forward.z = std::sin(yaw_) * std::cos(pitch_);
    camera_.forward = glm::normalize(camera_.forward);

    // Right vector: perpendicular to forward and world up
    camera_.right =
        glm::normalize(glm::cross(camera_.forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Up vector: perpendicular to forward and right
    camera_.up = glm::normalize(glm::cross(camera_.right, camera_.forward));
}

void CameraController::UpdateMatrices() {
    // View matrix: standard lookAt
    camera_.view_matrix =
        glm::lookAt(camera_.position, camera_.position + camera_.forward,
                    camera_.up);

    // Projection matrix for Vulkan (right-handed, Z-forward, Y-down in NDC)
    // ============================================================
    // CRITICAL: glm::perspective creates left-handed matrix by default.
    // We need glm::perspectiveRH_ZO (right-handed, zero-to-one depth)
    //
    // If using older glm, define these macros BEFORE including glm:
    //   #define GLM_FORCE_DEPTH_ZERO_TO_ONE
    //   #define GLM_FORCE_RIGHT_HANDED
    camera_.projection_matrix = glm::perspectiveRH_ZO(
        camera_.fov, camera_.aspect_ratio, camera_.near_plane,
        camera_.far_plane);

    // CRITICAL: Vulkan NDC has Y pointing down, but glm assumes Y up.
    // Invert the projection matrix's Y component.
    camera_.projection_matrix[1][1] *= -1.0f;

    // Combined matrix
    camera_.view_projection_matrix =
        camera_.projection_matrix * camera_.view_matrix;

    // Inverse matrices for shaders (frustum culling, etc.)
    camera_.inverse_view_matrix = glm::inverse(camera_.view_matrix);
    camera_.inverse_projection_matrix =
        glm::inverse(camera_.projection_matrix);
    camera_.inverse_view_projection_matrix =
        glm::inverse(camera_.view_projection_matrix);

    // Compute frustum planes for culling (optional, but useful)
    // Planes stored as (nx, ny, nz, d) for plane equation: n·p + d = 0
    auto row = [&](int r) {
        return glm::vec4(
            camera_.view_projection_matrix[0][r],
            camera_.view_projection_matrix[1][r],
            camera_.view_projection_matrix[2][r],
            camera_.view_projection_matrix[3][r]
        );
    };

    const glm::vec4 col1 = row(0);
    const glm::vec4 col2 = row(1);
    const glm::vec4 col3 = row(2);
    const glm::vec4 col4 = row(3);

    // Frustum planes (normalized as-is; ideally normalize before use)
    camera_.frustum_planes[0] = col4 + col1; // Left
    camera_.frustum_planes[1] = col4 - col1; // Right
    camera_.frustum_planes[2] = col4 + col2; // Bottom (flipped due to Y-down)
    camera_.frustum_planes[3] = col4 - col2; // Top
    camera_.frustum_planes[4] = col4 + col3; // Near
    camera_.frustum_planes[5] = col4 - col3; // Far
}

void CameraController::ToggleCursorCapture(GLFWwindow* window) {
#ifdef RENDERING_ENGINE_WITH_GLFW
    cursor_captured_ = !cursor_captured_;
    glfwSetInputMode(window, GLFW_CURSOR,
                     cursor_captured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    first_mouse_ = true; // Prevent jump when re-enabling
#else
    (void)window;
#endif
}

} // namespace rendering_engine
