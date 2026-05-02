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
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

// Simple Vec3
struct Vec3 {
    double x, y, z;
    Vec3(double x_ = 0, double y_ = 0, double z_ = 0) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator*(const Vec3& v) const { return Vec3(x * v.x, y * v.y, z * v.z); }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const { double l = length(); return l > 0 ? Vec3(x/l, y/l, z/l) : Vec3(0,0,0); }
    double dot(const Vec3& v) const { return x*v.x + y*v.y + z*v.z; }
};

// Simple quaternion approximation using Euler
struct SimpleQuat {
    float yaw, pitch, roll;
    
    static SimpleQuat identity() { return {0,0,0}; }
    static SimpleQuat from_euler(float y, float p, float r) { return {y,p,r}; }
    
    Vec3 rotate(const Vec3& v) const {
        return v; // Simplified
    }
    
    SimpleQuat operator*(const SimpleQuat& q) const {
        return {yaw + q.yaw, pitch + q.pitch, roll + q.roll};
    }
};

// Simple aircraft
struct SimpleAircraft {
    Vec3 position;
    Vec3 velocity;
    SimpleQuat orientation;
    Vec3 angular_velocity;
    
    float throttle = 0.5f;
    float pitch_input = 0.0f;
    float yaw_input = 0.0f;
    float roll_input = 0.0f;
    
    void update(float dt) {
        Vec3 acceleration = {0, -9.81, 0};
        
        Vec3 forward = orientation.rotate({0, 0, 1});
        acceleration = acceleration + forward * (throttle * 50.0);
        
        angular_velocity.x += roll_input * 2.0f - angular_velocity.x * 0.1f;
        angular_velocity.y += yaw_input * 1.5f - angular_velocity.y * 0.1f;
        angular_velocity.z += pitch_input * 1.5f - angular_velocity.z * 0.1f;
        
        float roll = angular_velocity.x * dt;
        float yaw = angular_velocity.y * dt;
        float pitch = angular_velocity.z * dt;
        
        orientation = SimpleQuat::from_euler(yaw, pitch, roll) * orientation;
        
        velocity = velocity + acceleration * dt;
        velocity = velocity * 0.99;
        position = position + velocity * dt;
        
        float aoa = calculate_aoa(velocity, forward);
        if (std::abs(aoa) > 15.0f * 3.14159f / 180.0f) {
            velocity = velocity * 0.95;
            std::cout << "STALL!" << std::endl;
        }
    }
    
    float calculate_aoa(const Vec3& vel, const Vec3& forward) {
        Vec3 nvel = vel.normalized();
        float dot = nvel.dot(forward);
        return std::acos(std::clamp(dot, -1.0f, 1.0f));
    }
};

int main() {
    std::cout << "Flight Simulation Demo" << std::endl;
    std::cout << "6DOF aircraft control demonstration" << std::endl;
    
    SimpleAircraft aircraft;
    aircraft.position = {0, 100, 0};
    aircraft.orientation = SimpleQuat::identity();
    
    auto start_time = std::chrono::steady_clock::now();
    float elapsed_time = 0.0f;
    
    while (elapsed_time < 30.0f) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - start_time).count();
        start_time = now;
        elapsed_time += dt;
        
        if (elapsed_time < 10.0f) {
            aircraft.throttle = 0.8f;
            aircraft.pitch_input = 0.2f;
        } else if (elapsed_time < 20.0f) {
            aircraft.pitch_input = 0.0f;
            aircraft.roll_input = 0.3f;
        } else {
            aircraft.throttle = 0.3f;
            aircraft.pitch_input = -0.1f;
        }
        
        aircraft.update(dt);
        
        if (static_cast<int>(elapsed_time) % 1 == 0 && static_cast<int>(elapsed_time) != static_cast<int>(elapsed_time - dt)) {
            std::cout << "Time: " << elapsed_time << "s" << std::endl;
            std::cout << "Pos: (" << aircraft.position.x << ", " << aircraft.position.y << ", " << aircraft.position.z << ")" << std::endl;
            std::cout << "Vel: (" << aircraft.velocity.x << ", " << aircraft.velocity.y << ", " << aircraft.velocity.z << ") mag: " << aircraft.velocity.length() << std::endl;
            std::cout << "Throttle: " << aircraft.throttle << ", Pitch: " << aircraft.pitch_input << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    std::cout << "Flight demo completed." << std::endl;
    return 0;
}