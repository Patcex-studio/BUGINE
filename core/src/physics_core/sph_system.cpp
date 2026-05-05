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
#include "physics_core/sph_system.h"
#include "physics_core/sph_boundary_system.h"
#include "physics_core/physics_core.h"
#include "physics_core/thermal_system.h"
#include <algorithm>
#include <unordered_map>

namespace physics_core {

static constexpr size_t SPH_BATCH_SIZE = 8;

static inline float horizontal_add(__m256 value) {
    __m128 lo = _mm256_castps256_ps128(value);
    __m128 hi = _mm256_extractf128_ps(value, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

// ============================================================================
// FluidGrid Implementation
// ============================================================================

FluidGrid::FluidGrid(float cell_size, int grid_size)
    : cell_size_(cell_size)
    , grid_dims_(grid_size, grid_size, grid_size)
    , total_cells_(grid_size * grid_size * grid_size)
{
    cells_.resize(total_cells_);
    build_neighbor_table();
}

void FluidGrid::build_neighbor_table() {
    // Build 3x3x3 neighborhood offsets (26 neighbors)
    std::vector<int3> offsets;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue; // Skip self
                offsets.emplace_back(dx, dy, dz);
            }
        }
    }

    // For each cell, compute neighbor indices
    for (int z = 0; z < grid_dims_.z; ++z) {
        for (int y = 0; y < grid_dims_.y; ++y) {
            for (int x = 0; x < grid_dims_.x; ++x) {
                int3 cell_coords(x, y, z);
                uint32_t cell_idx = get_cell_index(cell_coords);
                auto& cell = cells_[cell_idx];

                for (size_t i = 0; i < offsets.size(); ++i) {
                    int3 neighbor_coords = cell_coords + offsets[i];
                    if (is_valid_cell(neighbor_coords)) {
                        cell.neighbor_cells[i] = get_cell_index(neighbor_coords);
                    } else {
                        cell.neighbor_cells[i] = total_cells_; // Invalid marker
                    }
                }
            }
        }
    }
}

void FluidGrid::update_grid(const std::vector<SPHParticle>& particles) {
    // Clear previous data
    for (auto& cell : cells_) {
        cell.particle_count = 0;
    }

    // Count particles per cell
    std::vector<uint32_t> cell_counts(total_cells_, 0);
    for (const auto& particle : particles) {
        int3 cell_coords = get_cell_coords(particle.get_x(), particle.get_y(), particle.get_z());
        if (is_valid_cell(cell_coords)) {
            uint32_t cell_idx = get_cell_index(cell_coords);
            cell_counts[cell_idx]++;
        }
    }

    // Compute cell start indices
    uint32_t current_start = 0;
    for (uint32_t i = 0; i < total_cells_; ++i) {
        cells_[i].particle_start = current_start;
        cells_[i].particle_count = cell_counts[i];
        current_start += cell_counts[i];
    }

    // Sort particles into cells
    sorted_particles_.resize(particles.size());
    cell_particle_map_.resize(particles.size());
    std::vector<uint32_t> cell_offsets = cell_counts; // Copy for offsets

    for (uint32_t i = 0; i < particles.size(); ++i) {
        int3 cell_coords = get_cell_coords(particles[i].get_x(), particles[i].get_y(), particles[i].get_z());
        if (is_valid_cell(cell_coords)) {
            uint32_t cell_idx = get_cell_index(cell_coords);
            uint32_t local_idx = cells_[cell_idx].particle_start + (--cell_offsets[cell_idx]);
            sorted_particles_[local_idx] = i; // Store original particle index
            cell_particle_map_[i] = local_idx; // Reverse mapping
        }
    }
}

std::vector<uint32_t> FluidGrid::get_neighbors(uint32_t particle_idx, const std::vector<SPHParticle>& particles) const {
    std::vector<uint32_t> neighbors;

    // Get particle's cell
    const auto& particle = particles[particle_idx];
    int3 cell_coords = get_cell_coords(particle.get_x(), particle.get_y(), particle.get_z());

    if (!is_valid_cell(cell_coords)) return neighbors;

    uint32_t cell_idx = get_cell_index(cell_coords);
    const auto& cell = cells_[cell_idx];

    // Add particles from current cell
    for (uint32_t i = 0; i < cell.particle_count; ++i) {
        uint32_t neighbor_idx = sorted_particles_[cell.particle_start + i];
        if (neighbor_idx != particle_idx) {
            neighbors.push_back(neighbor_idx);
        }
    }

    // Add particles from neighbor cells
    for (uint32_t neighbor_cell_idx : cell.neighbor_cells) {
        if (neighbor_cell_idx >= total_cells_) continue; // Invalid cell

        const auto& neighbor_cell = cells_[neighbor_cell_idx];
        for (uint32_t i = 0; i < neighbor_cell.particle_count; ++i) {
            neighbors.push_back(sorted_particles_[neighbor_cell.particle_start + i]);
        }
    }

    return neighbors;
}

int3 FluidGrid::get_cell_coords(float x, float y, float z) const {
    return int3(
        static_cast<int>(std::floor(x / cell_size_)),
        static_cast<int>(std::floor(y / cell_size_)),
        static_cast<int>(std::floor(z / cell_size_))
    );
}

uint32_t FluidGrid::get_cell_index(const int3& coords) const {
    return static_cast<uint32_t>(
        coords.x +
        coords.y * grid_dims_.x +
        coords.z * grid_dims_.x * grid_dims_.y
    );
}

bool FluidGrid::is_valid_cell(const int3& coords) const {
    return coords.x >= 0 && coords.x < grid_dims_.x &&
           coords.y >= 0 && coords.y < grid_dims_.y &&
           coords.z >= 0 && coords.z < grid_dims_.z;
}

// ============================================================================
// SPH System Implementation
// ============================================================================

SPHSystem::SPHSystem(size_t max_particles)
    : particles_()
    , kernel_(0.01f) // 1cm smoothing length
    , grid_(0.02f, 128) // 2cm cells, 128x128x128 grid
    , max_particles_(max_particles)
    , xsph_epsilon_(0.5f)
    , artificial_viscosity_alpha_(0.1f)
{
    particles_.reserve(max_particles_);
    allocate_soa_arrays();

    // Initialize fluid properties
    fluid_properties_[FluidType::WATER] = {1000.0f, 0.001f};
    fluid_properties_[FluidType::MUD] = {1200.0f, 0.05f};
    fluid_properties_[FluidType::BLOOD] = {1060.0f, 0.004f};
    fluid_properties_[FluidType::OIL] = {900.0f, 0.01f};
    fluid_properties_[FluidType::FUEL] = {750.0f, 0.0005f};
    fluid_properties_[FluidType::CHEMICAL] = {1000.0f, 0.001f};
}

SPHSystem::~SPHSystem() {
    free_soa_arrays();
}

void SPHSystem::allocate_soa_arrays() {
    auto alloc = [&](size_t count) {
        return static_cast<float*>(_mm_malloc(count * sizeof(float), 64));
    };

    pos_x_ = alloc(max_particles_);
    pos_y_ = alloc(max_particles_);
    pos_z_ = alloc(max_particles_);
    vel_x_ = alloc(max_particles_);
    vel_y_ = alloc(max_particles_);
    vel_z_ = alloc(max_particles_);
    acc_x_ = alloc(max_particles_);
    acc_y_ = alloc(max_particles_);
    acc_z_ = alloc(max_particles_);
    density_ = alloc(max_particles_);
    pressure_ = alloc(max_particles_);
    mass_ = alloc(max_particles_);
    viscosity_ = alloc(max_particles_);
    
    // Allocate thermal data
    temp_particle_data_.resize(max_particles_);
    std::fill(temp_particle_data_.begin(), temp_particle_data_.end(), 293.15f);

    std::fill_n(pos_x_, max_particles_, 0.0f);
    std::fill_n(pos_y_, max_particles_, 0.0f);
    std::fill_n(pos_z_, max_particles_, 0.0f);
    std::fill_n(vel_x_, max_particles_, 0.0f);
    std::fill_n(vel_y_, max_particles_, 0.0f);
    std::fill_n(vel_z_, max_particles_, 0.0f);
    std::fill_n(acc_x_, max_particles_, 0.0f);
    std::fill_n(acc_y_, max_particles_, 0.0f);
    std::fill_n(acc_z_, max_particles_, 0.0f);
    std::fill_n(density_, max_particles_, 0.0f);
    std::fill_n(pressure_, max_particles_, 0.0f);
    std::fill_n(mass_, max_particles_, 0.0f);
    std::fill_n(viscosity_, max_particles_, 0.0f);
}

void SPHSystem::free_soa_arrays() {
    if (pos_x_) _mm_free(pos_x_);
    if (pos_y_) _mm_free(pos_y_);
    if (pos_z_) _mm_free(pos_z_);
    if (vel_x_) _mm_free(vel_x_);
    if (vel_y_) _mm_free(vel_y_);
    if (vel_z_) _mm_free(vel_z_);
    if (acc_x_) _mm_free(acc_x_);
    if (acc_y_) _mm_free(acc_y_);
    if (acc_z_) _mm_free(acc_z_);
    if (density_) _mm_free(density_);
    if (pressure_) _mm_free(pressure_);
    if (mass_) _mm_free(mass_);
    if (viscosity_) _mm_free(viscosity_);

    pos_x_ = pos_y_ = pos_z_ = nullptr;
    vel_x_ = vel_y_ = vel_z_ = nullptr;
    acc_x_ = acc_y_ = acc_z_ = nullptr;
    density_ = pressure_ = mass_ = viscosity_ = nullptr;
}

void SPHSystem::sync_soa_from_aos() {
    const size_t count = particles_.size();
    for (size_t i = 0; i < count; ++i) {
        const auto& p = particles_[i];
        pos_x_[i] = p.get_x();
        pos_y_[i] = p.get_y();
        pos_z_[i] = p.get_z();
        vel_x_[i] = p.get_vx();
        vel_y_[i] = p.get_vy();
        vel_z_[i] = p.get_vz();
        acc_x_[i] = p.get_ax();
        acc_y_[i] = p.get_ay();
        acc_z_[i] = p.get_az();
        density_[i] = p.density;
        pressure_[i] = p.pressure;
        mass_[i] = p.mass;
        viscosity_[i] = p.viscosity;
    }
    soa_dirty_ = false;
}

void SPHSystem::sync_aos_from_soa() {
    const size_t count = particles_.size();
    for (size_t i = 0; i < count; ++i) {
        auto& p = particles_[i];
        p.set_position(pos_x_[i], pos_y_[i], pos_z_[i]);
        p.set_velocity(vel_x_[i], vel_y_[i], vel_z_[i]);
        p.set_acceleration(acc_x_[i], acc_y_[i], acc_z_[i]);
        p.density = density_[i];
        p.pressure = pressure_[i];
        p.mass = mass_[i];
        p.viscosity = viscosity_[i];
    }
}

void SPHSystem::initialize_fluid(FluidType type, const std::vector<std::array<float, 3>>& positions) {
    particles_.clear();
    add_particles(positions, type);
    soa_dirty_ = true;
}

void SPHSystem::add_particles(const std::vector<std::array<float, 3>>& positions, FluidType type) {
    auto [density, viscosity] = fluid_properties_[type];

    for (const auto& pos : positions) {
        if (particles_.size() >= max_particles_) break;

        SPHParticle particle;
        particle.set_position(pos[0], pos[1], pos[2]);
        particle.density = density;
        particle.mass = 1.0f; // Assume unit mass
        particle.fluid_type = static_cast<uint32_t>(type);
        particle.viscosity = viscosity;
        particle.temperature = 293.15f;

        particles_.push_back(particle);
    }
    soa_dirty_ = true;
}

void SPHSystem::remove_particles(const std::vector<uint32_t>& indices) {
    if (indices.empty()) return;

    std::vector<char> remove_mask(particles_.size(), 0);
    for (uint32_t index : indices) {
        if (index < particles_.size()) remove_mask[index] = 1;
    }

    size_t write = 0;
    for (size_t read = 0; read < particles_.size(); ++read) {
        if (!remove_mask[read]) {
            if (write != read) particles_[write] = particles_[read];
            ++write;
        }
    }
    particles_.resize(write);
    soa_dirty_ = true;
}

void SPHSystem::set_fluid_properties(FluidType type, float density, float viscosity) {
    fluid_properties_[type] = {density, viscosity};
}

void SPHSystem::update(float dt) {
    if (particles_.empty()) return;

    if (soa_dirty_) {
        sync_soa_from_aos();
    }

    // Update grid using AoS for compatibility, then compute using SoA internals.
    grid_.update_grid(particles_);

    // SPH pipeline
    compute_densities_simd();
    compute_pressures_simd();
    compute_forces_simd();
    
    // Apply boundary forces for two-way interaction with rigid bodies/containers
    if (boundary_system_) {
        compute_boundary_forces_simd(boundary_system_);
    }
    
    apply_xsph_smoothing();
    integrate_particles_simd(dt);

    sync_aos_from_soa();
}

void SPHSystem::compute_densities_simd() {
    const size_t count = particles_.size();
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        auto neighbors = grid_.get_neighbors(static_cast<uint32_t>(i), particles_);
        __m256 rho_accum = _mm256_setzero_ps();
        float target_x = pos_x_[i];
        float target_y = pos_y_[i];
        float target_z = pos_z_[i];

        size_t n = 0;
        for (; n + SPH_BATCH_SIZE <= neighbors.size(); n += SPH_BATCH_SIZE) {
            __m256i indices = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(neighbors.data() + n));
            __m256 neigh_x = _mm256_i32gather_ps(pos_x_, indices, 4);
            __m256 neigh_y = _mm256_i32gather_ps(pos_y_, indices, 4);
            __m256 neigh_z = _mm256_i32gather_ps(pos_z_, indices, 4);
            __m256 neigh_mass = _mm256_i32gather_ps(mass_, indices, 4);

            __m256 target_xv = _mm256_set1_ps(target_x);
            __m256 target_yv = _mm256_set1_ps(target_y);
            __m256 target_zv = _mm256_set1_ps(target_z);

            __m256 dx = _mm256_sub_ps(neigh_x, target_xv);
            __m256 dy = _mm256_sub_ps(neigh_y, target_yv);
            __m256 dz = _mm256_sub_ps(neigh_z, target_zv);

            __m256 dist_sq = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));
            __m256 kernel_values = kernel_.evaluate_density_kernel_simd(dist_sq);
            rho_accum = _mm256_add_ps(rho_accum, _mm256_mul_ps(neigh_mass, kernel_values));
        }

        float density = horizontal_add(rho_accum);
        for (; n < neighbors.size(); ++n) {
            uint32_t neighbor_idx = neighbors[n];
            float dx = pos_x_[neighbor_idx] - target_x;
            float dy = pos_y_[neighbor_idx] - target_y;
            float dz = pos_z_[neighbor_idx] - target_z;
            float dist_sq = dx*dx + dy*dy + dz*dz;
            density += mass_[neighbor_idx] * kernel_.evaluate_poly6(dist_sq);
        }

        density_[i] = std::max(density, 100.0f);
    }
}

void SPHSystem::compute_pressures_simd() {
    const size_t count = particles_.size();
    if (count == 0) return;

    const __m256 K_vec = _mm256_set1_ps(1000.0f);
    const __m256 one_vec = _mm256_set1_ps(1.0f);

    size_t i = 0;
    for (; i + SPH_BATCH_SIZE <= count; i += SPH_BATCH_SIZE) {
        __m256 rho = _mm256_loadu_ps(density_ + i);

        alignas(32) float rest_density[SPH_BATCH_SIZE];
        for (size_t lane = 0; lane < SPH_BATCH_SIZE; ++lane) {
            rest_density[lane] = get_rest_density(static_cast<FluidType>(particles_[i + lane].fluid_type));
        }
        __m256 rest_rho = _mm256_load_ps(rest_density);

        __m256 ratio = _mm256_div_ps(rho, rest_rho);
        __m256 ratio2 = _mm256_mul_ps(ratio, ratio);
        __m256 ratio4 = _mm256_mul_ps(ratio2, ratio2);
        __m256 ratio7 = _mm256_mul_ps(ratio, _mm256_mul_ps(ratio2, ratio4));

        __m256 pressure = _mm256_mul_ps(K_vec, _mm256_sub_ps(ratio7, one_vec));
        _mm256_storeu_ps(pressure_ + i, pressure);
    }

    for (; i < count; ++i) {
        float rest_density = get_rest_density(static_cast<FluidType>(particles_[i].fluid_type));
        float ratio = density_[i] / rest_density;
        float ratio7 = ratio * ratio * ratio * ratio * ratio * ratio * ratio;
        pressure_[i] = 1000.0f * (ratio7 - 1.0f);
    }
}

void SPHSystem::compute_forces_simd() {
    const size_t count = particles_.size();
    if (count == 0) return;

    std::fill_n(acc_x_, count, 0.0f);
    std::fill_n(acc_y_, count, 0.0f);
    std::fill_n(acc_z_, count, 0.0f);

    const __m256 one_vec = _mm256_set1_ps(1.0f);
    const __m256 min_dist = _mm256_set1_ps(1e-6f);
    const __m256 h_vec = _mm256_set1_ps(kernel_.h);
    const __m256 grad_coeff = _mm256_set1_ps(kernel_.spiky_grad_coeff);
    const __m256 neg_one = _mm256_set1_ps(-1.0f);

    for (size_t i = 0; i < count; ++i) {
        auto neighbors = grid_.get_neighbors(static_cast<uint32_t>(i), particles_);
        __m256 total_fx = _mm256_setzero_ps();
        __m256 total_fy = _mm256_setzero_ps();
        __m256 total_fz = _mm256_setzero_ps();

        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        float vx = vel_x_[i];
        float vy = vel_y_[i];
        float vz = vel_z_[i];
        float pi_pressure = pressure_[i];
        float pi_density = density_[i];
        float inv_rho_i2 = 1.0f / (pi_density * pi_density);

        __m256 target_px = _mm256_set1_ps(px);
        __m256 target_py = _mm256_set1_ps(py);
        __m256 target_pz = _mm256_set1_ps(pz);
        __m256 target_vx = _mm256_set1_ps(vx);
        __m256 target_vy = _mm256_set1_ps(vy);
        __m256 target_vz = _mm256_set1_ps(vz);
        __m256 target_pressure = _mm256_set1_ps(pi_pressure);
        __m256 target_inv_rho_i2 = _mm256_set1_ps(inv_rho_i2);
        __m256 target_viscosity = _mm256_set1_ps(viscosity_[i]);

        size_t n = 0;
        for (; n + SPH_BATCH_SIZE <= neighbors.size(); n += SPH_BATCH_SIZE) {
            __m256i indices = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(neighbors.data() + n));
            __m256 nx = _mm256_i32gather_ps(pos_x_, indices, 4);
            __m256 ny = _mm256_i32gather_ps(pos_y_, indices, 4);
            __m256 nz = _mm256_i32gather_ps(pos_z_, indices, 4);
            __m256 nvx = _mm256_i32gather_ps(vel_x_, indices, 4);
            __m256 nvy = _mm256_i32gather_ps(vel_y_, indices, 4);
            __m256 nvz = _mm256_i32gather_ps(vel_z_, indices, 4);
            __m256 nmass = _mm256_i32gather_ps(mass_, indices, 4);
            __m256 npressure = _mm256_i32gather_ps(pressure_, indices, 4);
            __m256 ndensity = _mm256_i32gather_ps(density_, indices, 4);
            __m256 nviscosity = _mm256_i32gather_ps(viscosity_, indices, 4);

            __m256 dx = _mm256_sub_ps(nx, target_px);
            __m256 dy = _mm256_sub_ps(ny, target_py);
            __m256 dz = _mm256_sub_ps(nz, target_pz);
            __m256 dist_sq = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));
            __m256 dist = _mm256_sqrt_ps(dist_sq);
            __m256 valid = _mm256_and_ps(_mm256_cmp_ps(dist, min_dist, _CMP_GT_OQ), _mm256_cmp_ps(dist, _mm256_mul_ps(h_vec, _mm256_set1_ps(2.0f)), _CMP_LT_OQ));
            __m256 inv_dist = _mm256_div_ps(one_vec, _mm256_max_ps(dist, min_dist));

            __m256 h_minus_r = _mm256_sub_ps(h_vec, dist);
            __m256 coeff = _mm256_mul_ps(grad_coeff, _mm256_mul_ps(h_minus_r, h_minus_r));
            coeff = _mm256_mul_ps(coeff, inv_dist);
            coeff = _mm256_and_ps(coeff, valid);

            __m256 grad_x = _mm256_mul_ps(dx, coeff);
            __m256 grad_y = _mm256_mul_ps(dy, coeff);
            __m256 grad_z = _mm256_mul_ps(dz, coeff);

            __m256 pressure_term = _mm256_add_ps(_mm256_mul_ps(target_pressure, target_inv_rho_i2), _mm256_mul_ps(npressure, _mm256_div_ps(one_vec, _mm256_mul_ps(ndensity, ndensity))));
            pressure_term = _mm256_mul_ps(pressure_term, nmass);
            __m256 pressure_fx = _mm256_mul_ps(grad_x, _mm256_mul_ps(pressure_term, neg_one));
            __m256 pressure_fy = _mm256_mul_ps(grad_y, _mm256_mul_ps(pressure_term, neg_one));
            __m256 pressure_fz = _mm256_mul_ps(grad_z, _mm256_mul_ps(pressure_term, neg_one));

            __m256 vel_dx = _mm256_sub_ps(nvx, target_vx);
            __m256 vel_dy = _mm256_sub_ps(nvy, target_vy);
            __m256 vel_dz = _mm256_sub_ps(nvz, target_vz);
            __m256 viscosity_avg = _mm256_mul_ps(_mm256_add_ps(target_viscosity, nviscosity), _mm256_set1_ps(0.5f));
            __m256 visc_term = _mm256_mul_ps(nmass, _mm256_div_ps(viscosity_avg, ndensity));
            __m256 laplacian = _mm256_mul_ps(_mm256_set1_ps(kernel_.visc_laplace_coeff), _mm256_max_ps(_mm256_sub_ps(h_vec, dist), _mm256_setzero_ps()));
            __m256 viscosity_fx = _mm256_mul_ps(vel_dx, _mm256_mul_ps(visc_term, laplacian));
            __m256 viscosity_fy = _mm256_mul_ps(vel_dy, _mm256_mul_ps(visc_term, laplacian));
            __m256 viscosity_fz = _mm256_mul_ps(vel_dz, _mm256_mul_ps(visc_term, laplacian));

            total_fx = _mm256_add_ps(total_fx, _mm256_add_ps(pressure_fx, viscosity_fx));
            total_fy = _mm256_add_ps(total_fy, _mm256_add_ps(pressure_fy, viscosity_fy));
            total_fz = _mm256_add_ps(total_fz, _mm256_add_ps(pressure_fz, viscosity_fz));
        }

        float fx = horizontal_add(total_fx);
        float fy = horizontal_add(total_fy);
        float fz = horizontal_add(total_fz);

        for (; n < neighbors.size(); ++n) {
            uint32_t j = neighbors[n];
            float dx = pos_x_[j] - px;
            float dy = pos_y_[j] - py;
            float dz = pos_z_[j] - pz;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < 1e-6f || dist >= kernel_.h * 2.0f) continue;

            float pressure_term = mass_[j] * (pi_pressure * inv_rho_i2 + pressure_[j] / (density_[j] * density_[j]));
            float coeff = kernel_.spiky_grad_coeff * (kernel_.h - dist) * (kernel_.h - dist) / dist;
            fx += -pressure_term * dx * coeff;
            fy += -pressure_term * dy * coeff;
            fz += -pressure_term * dz * coeff;

            float viscosity_ij = 0.5f * (viscosity_[i] + viscosity_[j]);
            float laplacian = kernel_.visc_laplace_coeff * (kernel_.h - dist);
            float visc_term = mass_[j] * viscosity_ij / density_[j] * laplacian;
            fx += visc_term * (vel_x_[j] - vx);
            fy += visc_term * (vel_y_[j] - vy);
            fz += visc_term * (vel_z_[j] - vz);
        }

        acc_x_[i] = fx;
        acc_y_[i] = fy;
        acc_z_[i] = fz;
    }
}

void SPHSystem::apply_xsph_smoothing() {
    const size_t count = particles_.size();
    if (count == 0) return;

    const __m256 one_vec = _mm256_set1_ps(1.0f);

    for (size_t i = 0; i < count; ++i) {
        auto neighbors = grid_.get_neighbors(static_cast<uint32_t>(i), particles_);
        __m256 correction_x = _mm256_setzero_ps();
        __m256 correction_y = _mm256_setzero_ps();
        __m256 correction_z = _mm256_setzero_ps();

        float vx = vel_x_[i];
        float vy = vel_y_[i];
        float vz = vel_z_[i];

        size_t n = 0;
        for (; n + SPH_BATCH_SIZE <= neighbors.size(); n += SPH_BATCH_SIZE) {
            __m256i indices = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(neighbors.data() + n));
            __m256 nx = _mm256_i32gather_ps(pos_x_, indices, 4);
            __m256 ny = _mm256_i32gather_ps(pos_y_, indices, 4);
            __m256 nz = _mm256_i32gather_ps(pos_z_, indices, 4);
            __m256 nvx = _mm256_i32gather_ps(vel_x_, indices, 4);
            __m256 nvy = _mm256_i32gather_ps(vel_y_, indices, 4);
            __m256 nvz = _mm256_i32gather_ps(vel_z_, indices, 4);
            __m256 nmass = _mm256_i32gather_ps(mass_, indices, 4);
            __m256 ndensity = _mm256_i32gather_ps(density_, indices, 4);

            __m256 target_vx = _mm256_set1_ps(vx);
            __m256 target_vy = _mm256_set1_ps(vy);
            __m256 target_vz = _mm256_set1_ps(vz);
            __m256 target_x = _mm256_set1_ps(pos_x_[i]);
            __m256 target_y = _mm256_set1_ps(pos_y_[i]);
            __m256 target_z = _mm256_set1_ps(pos_z_[i]);

            __m256 dx = _mm256_sub_ps(nx, target_x);
            __m256 dy = _mm256_sub_ps(ny, target_y);
            __m256 dz = _mm256_sub_ps(nz, target_z);
            __m256 dist_sq = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));
            __m256 w = kernel_.evaluate_density_kernel_simd(dist_sq);
            __m256 inv_density = _mm256_div_ps(one_vec, ndensity);

            __m256 factor = _mm256_mul_ps(nmass, _mm256_mul_ps(inv_density, w));
            __m256 vel_dx = _mm256_sub_ps(nvx, target_vx);
            __m256 vel_dy = _mm256_sub_ps(nvy, target_vy);
            __m256 vel_dz = _mm256_sub_ps(nvz, target_vz);

            correction_x = _mm256_add_ps(correction_x, _mm256_mul_ps(factor, vel_dx));
            correction_y = _mm256_add_ps(correction_y, _mm256_mul_ps(factor, vel_dy));
            correction_z = _mm256_add_ps(correction_z, _mm256_mul_ps(factor, vel_dz));
        }

        float corr_x = horizontal_add(correction_x);
        float corr_y = horizontal_add(correction_y);
        float corr_z = horizontal_add(correction_z);

        for (; n < neighbors.size(); ++n) {
            uint32_t j = neighbors[n];
            float dx = pos_x_[j] - pos_x_[i];
            float dy = pos_y_[j] - pos_y_[i];
            float dz = pos_z_[j] - pos_z_[i];
            float dist_sq = dx*dx + dy*dy + dz*dz;
            float w = kernel_.evaluate_poly6(dist_sq);
            float mass_over_rho = mass_[j] / density_[j];
            corr_x += mass_over_rho * (vel_x_[j] - vx) * w;
            corr_y += mass_over_rho * (vel_y_[j] - vy) * w;
            corr_z += mass_over_rho * (vel_z_[j] - vz) * w;
        }

        vel_x_[i] += xsph_epsilon_ * corr_x;
        vel_y_[i] += xsph_epsilon_ * corr_y;
        vel_z_[i] += xsph_epsilon_ * corr_z;
    }
}

void SPHSystem::integrate_particles_simd(float dt) {
    const size_t count = particles_.size();
    if (count == 0) return;

    __m256 dt_vec = _mm256_set1_ps(dt);
    size_t i = 0;
    for (; i + SPH_BATCH_SIZE <= count; i += SPH_BATCH_SIZE) {
        __m256 vx = _mm256_load_ps(vel_x_ + i);
        __m256 vy = _mm256_load_ps(vel_y_ + i);
        __m256 vz = _mm256_load_ps(vel_z_ + i);
        __m256 ax = _mm256_load_ps(acc_x_ + i);
        __m256 ay = _mm256_load_ps(acc_y_ + i);
        __m256 az = _mm256_load_ps(acc_z_ + i);

        __m256 new_vx = _mm256_add_ps(vx, _mm256_mul_ps(ax, dt_vec));
        __m256 new_vy = _mm256_add_ps(vy, _mm256_mul_ps(ay, dt_vec));
        __m256 new_vz = _mm256_add_ps(vz, _mm256_mul_ps(az, dt_vec));

        _mm256_store_ps(vel_x_ + i, new_vx);
        _mm256_store_ps(vel_y_ + i, new_vy);
        _mm256_store_ps(vel_z_ + i, new_vz);

        __m256 px = _mm256_load_ps(pos_x_ + i);
        __m256 py = _mm256_load_ps(pos_y_ + i);
        __m256 pz = _mm256_load_ps(pos_z_ + i);

        _mm256_store_ps(pos_x_ + i, _mm256_add_ps(px, _mm256_mul_ps(new_vx, dt_vec)));
        _mm256_store_ps(pos_y_ + i, _mm256_add_ps(py, _mm256_mul_ps(new_vy, dt_vec)));
        _mm256_store_ps(pos_z_ + i, _mm256_add_ps(pz, _mm256_mul_ps(new_vz, dt_vec)));
    }

    for (; i < count; ++i) {
        vel_x_[i] += acc_x_[i] * dt;
        vel_y_[i] += acc_y_[i] * dt;
        vel_z_[i] += acc_z_[i] * dt;
        pos_x_[i] += vel_x_[i] * dt;
        pos_y_[i] += vel_y_[i] * dt;
        pos_z_[i] += vel_z_[i] * dt;
    }
}

float SPHSystem::get_rest_density(FluidType type) const {
    auto it = fluid_properties_.find(type);
    return it != fluid_properties_.end() ? it->second.first : 1000.0f;
}

float SPHSystem::get_viscosity(FluidType type) const {
    auto it = fluid_properties_.find(type);
    return it != fluid_properties_.end() ? it->second.second : 0.001f;
}

// ============================================================================
// Boundary Particle Coupling (Phase 4: Fluid-Solid Interaction)
// ============================================================================

void SPHSystem::apply_boundary_forces(SPHBoundarySystem* boundary_system, PhysicsCore& physics, float dt)
{
    if (!boundary_system || particles_.empty()) {
        return;
    }
    
    compute_boundary_forces_simd(boundary_system);
    boundary_system->apply_accumulated_body_forces(physics);
}

void SPHSystem::compute_boundary_forces_simd(SPHBoundarySystem* boundary_system)
{
    // This method implements two-way coupling between SPH fluid and rigid bodies
    // represented by boundary particles
    
    if (!boundary_system) return;
    
    auto& boundary_particles = boundary_system->get_particles_mut();
    if (boundary_particles.count == 0) return;
    
    const size_t fluid_count = particles_.size();
    const float h = kernel_.h;
    const float h_sq = kernel_.h_sq;
    const float repulsion_coeff = 1000.0f;  // Repulsion stiffness
    
    // For each fluid particle, check interactions with boundary particles
    for (size_t i = 0; i < fluid_count; ++i) {
        float fx = 0.0f, fy = 0.0f, fz = 0.0f;
        
        float fluid_x = pos_x_[i];
        float fluid_y = pos_y_[i];
        float fluid_z = pos_z_[i];
        
        // Check against boundary particles
        for (uint32_t j = 0; j < boundary_particles.count; ++j) {
            float dx = boundary_particles.pos_x[j] - fluid_x;
            float dy = boundary_particles.pos_y[j] - fluid_y;
            float dz = boundary_particles.pos_z[j] - fluid_z;
            
            float dist_sq = dx*dx + dy*dy + dz*dz;
            
            if (dist_sq < h_sq && dist_sq > 1e-6f) {
                float dist = std::sqrt(dist_sq);
                float kernel_val = kernel_.evaluate_poly6(dist_sq);
                
                // Repulsion force: pushes fluid away from boundary
                float mag = repulsion_coeff * kernel_val / (dist + 0.001f);
                fx += mag * dx;
                fy += mag * dy;
                fz += mag * dz;
                
                // Force is applied to fluid
                // Force is applied to fluid particle
                acc_x_[i] += fx / mass_[i];
                acc_y_[i] += fy / mass_[i];
                acc_z_[i] += fz / mass_[i];
                
                // Opposite force applied to rigid body (accumulated in boundary system)
                // This is handled externally after computing forces
                // Opposite force applied to rigid body via boundary particles
                // Accumulate in boundary system for later application
                Vec3 reaction_force(-fx, -fy, -fz);
                boundary_system->add_particle_force(j, reaction_force);
            }
        }
    }
}

void SPHSystem::update_thermal_properties(const ThermalSystem* thermal_system)
{
    if (!thermal_system) return;
    
    // Copy thermal data into particle array for rendering/diagnostics
    const auto& thermal_particles = thermal_system->get_thermal_particles();
    
    if (thermal_particles.count != particles_.size()) {
        return;
    }
    
    for (size_t i = 0; i < particles_.size(); ++i) {
        particles_[i].temperature = thermal_particles.temperature[i];
    }
}

} // namespace physics_core