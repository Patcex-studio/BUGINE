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

#include <immintrin.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <unordered_map>

namespace physics_core {

class PhysicsCore;

// ============================================================================
// SPH Particle Types and Constants
// ============================================================================

/**
 * @enum FluidType
 * @brief Types of fluids supported by the SPH system
 */
enum class FluidType : uint32_t {
    WATER = 0,        // ρ=1000, μ=0.001
    MUD = 1,          // ρ=1200, μ=0.05 (viscous)
    BLOOD = 2,        // ρ=1060, μ=0.004 (special properties)
    OIL = 3,          // ρ=900,  μ=0.01 (low surface tension)
    FUEL = 4,         // ρ=750,  μ=0.0005 (volatile)
    CHEMICAL = 5,     // Custom properties
    GAS = 6           // Low-density gas
};

/**
 * @struct SPHParticle
 * @brief SIMD-optimized SPH particle structure
 *
 * Memory layout optimized for AVX2/AVX-512:
 * - 32-byte alignment for optimal SIMD loads
 * - SoA-friendly structure for cache efficiency
 * - Float32 precision for performance
 */
struct alignas(32) SPHParticle {
    // Position vector (xyz, padded to 4 floats for SIMD)
    __m128 position;        // float32 x,y,z,0

    // Velocity vector (xyz, padded)
    __m128 velocity;        // float32 x,y,z,0

    // Acceleration vector (xyz, padded)
    __m128 acceleration;    // float32 x,y,z,0

    // Scalar properties
    float density;          // Current density
    float pressure;         // Computed pressure
    float mass;             // Constant mass
    uint32_t fluid_type;    // FluidType enum
    uint32_t flags;         // Particle flags

    // Material properties
    float viscosity;        // Material-specific viscosity
    float temperature;      // Thermal effects

    // Constructor
    SPHParticle()
        : position(_mm_setzero_ps())
        , velocity(_mm_setzero_ps())
        , acceleration(_mm_setzero_ps())
        , density(1000.0f)
        , pressure(0.0f)
        , mass(1.0f)
        , fluid_type(static_cast<uint32_t>(FluidType::WATER))
        , flags(0)
        , viscosity(0.001f)
        , temperature(293.15f) // 20°C
    {}

    // Set position
    void set_position(float x, float y, float z) {
        position = _mm_set_ps(0.0f, z, y, x);
    }

    // Get position components
    float get_x() const { return _mm_cvtss_f32(position); }
    float get_y() const { return _mm_cvtss_f32(_mm_shuffle_ps(position, position, _MM_SHUFFLE(1,1,1,1))); }
    float get_z() const { return _mm_cvtss_f32(_mm_shuffle_ps(position, position, _MM_SHUFFLE(2,2,2,2))); }

    // Set velocity
    void set_velocity(float vx, float vy, float vz) {
        velocity = _mm_set_ps(0.0f, vz, vy, vx);
    }

    // Get velocity components
    float get_vx() const { return _mm_cvtss_f32(velocity); }
    float get_vy() const { return _mm_cvtss_f32(_mm_shuffle_ps(velocity, velocity, _MM_SHUFFLE(1,1,1,1))); }
    float get_vz() const { return _mm_cvtss_f32(_mm_shuffle_ps(velocity, velocity, _MM_SHUFFLE(2,2,2,2))); }

    // Set acceleration
    void set_acceleration(float ax, float ay, float az) {
        acceleration = _mm_set_ps(0.0f, az, ay, ax);
    }

    // Get acceleration components
    float get_ax() const { return _mm_cvtss_f32(acceleration); }
    float get_ay() const { return _mm_cvtss_f32(_mm_shuffle_ps(acceleration, acceleration, _MM_SHUFFLE(1,1,1,1))); }
    float get_az() const { return _mm_cvtss_f32(_mm_shuffle_ps(acceleration, acceleration, _MM_SHUFFLE(2,2,2,2))); }
};

// ============================================================================
// SPH Kernel Functions
// ============================================================================

/**
 * @struct SPHKernel
 * @brief Cubic spline kernel functions for SPH
 *
 * Implements the standard cubic spline kernel:
 * W(r,h) = (1/(πh³)) * { 1 - 1.5*q² + 0.75*q³ } for q < 1
 * W(r,h) = (1/(πh³)) * 0.25*(2-q)³ for 1 ≤ q < 2
 * W(r,h) = 0 for q ≥ 2
 *
 * Where q = r/h
 */
struct SPHKernel {
    float h;                    // Smoothing length
    float h_sq;                 // h² precomputed
    float poly6_coeff;          // W_poly6 normalization
    float spiky_grad_coeff;     // ∇W_spiky normalization
    float visc_laplace_coeff;   // ∇²W_viscosity normalization

    // Constructor
    SPHKernel(float smoothing_length = 0.01f)
        : h(smoothing_length)
        , h_sq(h * h)
    {
        // Precompute coefficients for cubic spline kernel
        const float pi_h3 = 1.0f / (3.141592653589793f * h * h * h);

        // Poly6 kernel coefficient: 315/(64*π*h⁹)
        poly6_coeff = 315.0f / (64.0f * 3.141592653589793f * h * h * h * h * h * h * h * h * h);

        // Spiky gradient coefficient: -45/(π*h⁶)
        spiky_grad_coeff = -45.0f / (3.141592653589793f * h * h * h * h * h * h);

        // Viscosity Laplacian coefficient: 45/(π*h⁶)
        visc_laplace_coeff = 45.0f / (3.141592653589793f * h * h * h * h * h * h);
    }

    /**
     * Evaluate Poly6 kernel for density computation
     * W_poly6(r,h) = (315/(64*π*h⁹)) * (h²-r²)³
     */
    float evaluate_poly6(float dist_sq) const {
        if (dist_sq >= h_sq * 4.0f) return 0.0f; // q >= 2

        float q = std::sqrt(dist_sq) / h;
        if (q >= 2.0f) return 0.0f;

        float h_sq_minus_r_sq = h_sq - dist_sq;
        return poly6_coeff * h_sq_minus_r_sq * h_sq_minus_r_sq * h_sq_minus_r_sq;
    }

    /**
     * Evaluate Spiky kernel gradient for pressure forces
     * ∇W_spiky(r,h) = (-45/(π*h⁶)) * (h-r)² * (r/|r|)
     */
    __m128 evaluate_spiky_gradient(__m128 r_vec, float dist) const {
        if (dist >= h * 2.0f || dist < 1e-6f) return _mm_setzero_ps();

        float q = dist / h;
        if (q >= 2.0f) return _mm_setzero_ps();

        // Compute (h-r)² / r
        float h_minus_r = h - dist;
        float coeff = spiky_grad_coeff * h_minus_r * h_minus_r / dist;

        // Normalize r_vec and multiply by coefficient
        __m128 dist_vec = _mm_set1_ps(dist);
        __m128 normalized_r = _mm_div_ps(r_vec, dist_vec);
        return _mm_mul_ps(normalized_r, _mm_set1_ps(coeff));
    }

    /**
     * Evaluate viscosity Laplacian
     * ∇²W_visc(r,h) = (45/(π*h⁶)) * (h-r)
     */
    float evaluate_viscosity_laplacian(float dist) const {
        if (dist >= h * 2.0f) return 0.0f;

        float q = dist / h;
        if (q >= 2.0f) return 0.0f;

        return visc_laplace_coeff * (h - dist);
    }

    /**
     * SIMD version: evaluate density kernel for 8 particles
     * Returns __m256 of kernel values
     */
    __m256 evaluate_density_kernel_simd(__m256 dist_sq_vec) const {
        // Load constants
        __m256 h_sq_vec = _mm256_set1_ps(h_sq * 4.0f);
        __m256 zero_vec = _mm256_setzero_ps();

        // Check if dist_sq >= 4*h² (q >= 2)
        __m256 mask = _mm256_cmp_ps(dist_sq_vec, h_sq_vec, _CMP_LT_OQ);
        if (_mm256_testz_ps(mask, mask)) return zero_vec; // All particles too far

        // Compute q = sqrt(dist_sq) / h
        __m256 dist_vec = _mm256_sqrt_ps(dist_sq_vec);
        __m256 h_vec = _mm256_set1_ps(h);
        __m256 q_vec = _mm256_div_ps(dist_vec, h_vec);

        // Check q < 2
        __m256 q_mask = _mm256_cmp_ps(q_vec, _mm256_set1_ps(2.0f), _CMP_LT_OQ);
        mask = _mm256_and_ps(mask, q_mask);

        // Compute (h² - r²)³
        __m256 h_sq_vec2 = _mm256_set1_ps(h_sq);
        __m256 diff_vec = _mm256_sub_ps(h_sq_vec2, dist_sq_vec);
        __m256 diff_sq_vec = _mm256_mul_ps(diff_vec, diff_vec);
        __m256 kernel_vec = _mm256_mul_ps(diff_sq_vec, diff_vec);

        // Apply coefficient
        __m256 coeff_vec = _mm256_set1_ps(poly6_coeff);
        kernel_vec = _mm256_mul_ps(kernel_vec, coeff_vec);

        // Apply mask
        return _mm256_and_ps(kernel_vec, mask);
    }
};

// ============================================================================
// Grid-Based Neighbor Search
// ============================================================================

/**
 * @struct FluidGridCell
 * @brief Grid cell for spatial hashing
 */
struct FluidGridCell {
    uint32_t particle_start;    // Start index in particle array
    uint32_t particle_count;    // Count in this cell
    std::array<uint32_t, 26> neighbor_cells; // 3D neighborhood indices

    FluidGridCell() : particle_start(0), particle_count(0) {
        neighbor_cells.fill(0);
    }
};

/**
 * @struct int3
 * @brief 3D integer vector for grid coordinates
 */
struct int3 {
    int x, y, z;

    int3() : x(0), y(0), z(0) {}
    int3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    bool operator==(const int3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    int3 operator+(const int3& other) const {
        return int3(x + other.x, y + other.y, z + other.z);
    }
};

/**
 * @class FluidGrid
 * @brief Spatial hashing grid for neighbor search
 */
class FluidGrid {
public:
    FluidGrid(float cell_size = 0.02f, int grid_size = 64);
    ~FluidGrid() = default;

    /**
     * Update grid with new particle positions
     */
    void update_grid(const std::vector<SPHParticle>& particles);

    /**
     * Get neighbors for a particle
     */
    std::vector<uint32_t> get_neighbors(uint32_t particle_idx, const std::vector<SPHParticle>& particles) const;

    /**
     * Get grid cell for position
     */
    int3 get_cell_coords(float x, float y, float z) const;

    /**
     * Get cell index from coordinates
     */
    uint32_t get_cell_index(const int3& coords) const;

    /**
     * Check if cell coordinates are valid
     */
    bool is_valid_cell(const int3& coords) const;

private:
    std::vector<uint32_t> sorted_particles_;  // Particles sorted by grid cell
    std::vector<FluidGridCell> cells_;        // Grid structure
    std::vector<uint32_t> cell_particle_map_; // Reverse mapping
    float cell_size_;                         // h * 2 (kernel radius)
    int3 grid_dims_;                          // Grid dimensions
    int total_cells_;                         // Total number of cells

    void build_neighbor_table();
};

/**
 * @class SPHSystem
 * @brief Complete SPH fluid simulation system
 */
class SPHSystem {
public:
    SPHSystem(size_t max_particles = 100000);
    ~SPHSystem();

    /**
     * Initialize with fluid type
     */
    void initialize_fluid(FluidType type, const std::vector<std::array<float, 3>>& positions);

    /**
     * Update simulation
     */
    void update(float dt);

    /**
     * Get particles (read-only)
     */
    const std::vector<SPHParticle>& get_particles() const { return particles_; }

    /**
     * Add particles
     */
    void add_particles(const std::vector<std::array<float, 3>>& positions, FluidType type);

    /**
     * Remove particles
     */
    void remove_particles(const std::vector<uint32_t>& indices);

    /**
     * Set fluid properties
     */
    void set_fluid_properties(FluidType type, float density, float viscosity);

    /**
     * Apply boundary forces from solid bodies (for two-way coupling)
     * @param boundary_particles Boundary particles from SPHBoundarySystem
     * @param physics Core physics interface for rigid body application
     * @param dt Time step
     */
    void apply_boundary_forces(class SPHBoundarySystem* boundary_system, PhysicsCore& physics, float dt);

    /**
     * Set artificial viscosity coefficient for shockwave stability
     * @param alpha Monaghan viscosity coefficient (typically 0.5-1.0)
     */
    void set_artificial_viscosity(float alpha) { artificial_viscosity_alpha_ = alpha; }

    /**
     * Get reference to particle temperature array for thermal effects
     */
    float* get_temperature_array() { return temp_particle_data_.empty() ? nullptr : temp_particle_data_.data(); }

    /**
     * Update particle temperatures (for heating/cooling effects)
     */
    void update_thermal_properties(const class ThermalSystem* thermal_system);

private:
    std::vector<SPHParticle> particles_;
    SPHKernel kernel_;
    FluidGrid grid_;
    size_t max_particles_;

    // SoA arrays for SIMD computation
    float* pos_x_ = nullptr;
    float* pos_y_ = nullptr;
    float* pos_z_ = nullptr;
    float* vel_x_ = nullptr;
    float* vel_y_ = nullptr;
    float* vel_z_ = nullptr;
    float* acc_x_ = nullptr;
    float* acc_y_ = nullptr;
    float* acc_z_ = nullptr;
    float* density_ = nullptr;
    float* pressure_ = nullptr;
    float* mass_ = nullptr;
    float* viscosity_ = nullptr;
    
    // Thermal data (aligned for SIMD)
    alignas(64) std::vector<float> temp_particle_data_;

    bool soa_dirty_ = true;

    // Fluid properties per type
    std::unordered_map<FluidType, std::pair<float, float>> fluid_properties_;

    // Новые параметры для улучшений
    float xsph_epsilon_;  // Коэффициент XSPH сглаживания (0.5)
    float artificial_viscosity_alpha_;  // Коэффициент искусственной вязкости (для ударных волн)

    // SIMD computation functions
    void compute_densities_simd();
    void compute_pressures_simd();
    void compute_forces_simd();
    void compute_boundary_forces_simd(class SPHBoundarySystem* boundary_system);
    void apply_xsph_smoothing();
    void integrate_particles_simd(float dt);

    // SoA helpers
    void allocate_soa_arrays();
    void free_soa_arrays();
    void sync_soa_from_aos();
    void sync_aos_from_soa();

    // Utility functions
    float get_rest_density(FluidType type) const;
    float get_viscosity(FluidType type) const;
};

} // namespace physics_core