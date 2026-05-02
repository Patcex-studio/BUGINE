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
#include "physics_core/physics_container.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>  // for log_critical

namespace physics_core {

namespace {

inline uint64_t split_by_3(uint64_t value) {
    value &= 0x1fffffull;
    value = (value | (value << 32)) & 0x1f00000000ffffull;
    value = (value | (value << 16)) & 0x1f0000ff0000ffull;
    value = (value | (value << 8)) & 0x100f00f00f00f00full;
    value = (value | (value << 4)) & 0x10c30c30c30c30c3ull;
    value = (value | (value << 2)) & 0x1249249249249249ull;
    return value;
}

inline uint64_t morton3D(uint32_t x, uint32_t y, uint32_t z) {
    return (split_by_3(x) << 0) | (split_by_3(y) << 1) | (split_by_3(z) << 2);
}

constexpr uint64_t GOLDEN_RATIO_64 = 11400714819323198485ull;

}  // namespace

PhysicsContainer::PhysicsContainer(size_t reserve)
    : slots_(reserve),
      free_list_head_(0),
      total_created_(0),
      count_(0),
      registry_capacity_(1),
      entity_registry_(),
      body_cache_dirty_(false),
      origin_x_(0.0),
      origin_y_(0.0),
      origin_z_(0.0) {
    // Инициализируем свободный список
    for (uint32_t i = 0; i < reserve; ++i) {
        slots_[i].state = PhysicsSlot::State::Free;
        slots_[i].generation = 0;
        slots_[i].next_free_index = i + 1;
    }
    if (reserve > 0) {
        slots_[reserve - 1].next_free_index = INVALID_INDEX;
    }

    // Старые поля для совместимости
    size_t target = std::max<size_t>(16, reserve * 2);
    while (registry_capacity_ < target) {
        registry_capacity_ <<= 1;
    }
    entity_registry_.assign(registry_capacity_, EntitySlot{INVALID_ENTITY_ID, 0, SIZE_MAX, EntryState::EMPTY});
    index_to_registry_slot_.reserve(reserve);
    active_indices_.reserve(reserve);
    body_cache_.reserve(reserve);
    global_pos_x_.reserve(reserve);
    global_pos_y_.reserve(reserve);
    global_pos_z_.reserve(reserve);
    local_pos_x_.reserve(reserve);
    local_pos_y_.reserve(reserve);
    local_pos_z_.reserve(reserve);
    velocity_x_.reserve(reserve);
    velocity_y_.reserve(reserve);
    velocity_z_.reserve(reserve);
    mass_.reserve(reserve);
    inverse_mass_.reserve(reserve);
    entity_types_.reserve(reserve);
    sync_flags_.reserve(reserve);
    prev_positions_.reserve(reserve);
}

PhysicsContainer::~PhysicsContainer() = default;

EntityID PhysicsContainer::make_entity_id(uint32_t slot_idx, uint32_t generation) const {
    return (static_cast<EntityID>(generation) << 32) | slot_idx;
}

std::pair<uint32_t, uint32_t> PhysicsContainer::decompose_entity_id(EntityID entity) const {
    uint32_t slot_idx = entity & 0xFFFFFFFF;
    uint32_t generation = entity >> 32;
    return {slot_idx, generation};
}

void PhysicsContainer::expand_pool() {
    size_t old_size = slots_.size();
    size_t new_size = old_size * 2;
    slots_.resize(new_size);
    
    // Инициализируем новые слоты
    for (uint32_t i = old_size; i < new_size; ++i) {
        slots_[i].state = PhysicsSlot::State::Free;
        slots_[i].generation = 0;
        slots_[i].next_free_index = i + 1;
    }
    slots_[new_size - 1].next_free_index = free_list_head_;
    free_list_head_ = old_size;
}

EntityID PhysicsContainer::register_entity(const PhysicsBody& body) {
    if (free_list_head_ == INVALID_INDEX) {
        if (total_created_ > UINT32_MAX - 1000) {
            std::cerr << "PhysicsContainer generation overflow imminent" << std::endl;
            return INVALID_ENTITY_ID;
        }
        expand_pool();
    }
    
    uint32_t slot_idx = free_list_head_;
    auto& slot = slots_[slot_idx];
    free_list_head_ = slot.next_free_index;
    
    slot.body = body;
    slot.body.prev_position = body.position;  // Инициализируем для Verlet
    slot.state = PhysicsSlot::State::Occupied;
    slot.generation = (slot.generation + 1) & 0x7FFFFFFF;  // 31 бит
    
    active_indices_.push_back(slot_idx);
    std::sort(active_indices_.begin(), active_indices_.end());  // детерминированный порядок
    
    total_created_++;
    return make_entity_id(slot_idx, slot.generation);
}

void PhysicsContainer::mark_for_removal(EntityID entity) {
    auto [slot_idx, gen] = decompose_entity_id(entity);
    if (slot_idx >= slots_.size() || !slots_[slot_idx].is_valid(gen)) {
        return;
    }
    slots_[slot_idx].state = PhysicsSlot::State::PendingRemoval;
}

void PhysicsContainer::flush_pending_removals() {
    std::vector<uint32_t> to_remove;
    for (uint32_t idx : active_indices_) {
        if (slots_[idx].state == PhysicsSlot::State::PendingRemoval) {
            to_remove.push_back(idx);
        }
    }
    
    for (uint32_t idx : to_remove) {
        auto& slot = slots_[idx];
        slot.state = PhysicsSlot::State::Free;
        slot.next_free_index = free_list_head_;
        free_list_head_ = idx;
        
        // Удаляем из active_indices_
        auto it = std::lower_bound(active_indices_.begin(), active_indices_.end(), idx);
        if (it != active_indices_.end() && *it == idx) {
            active_indices_.erase(it);
        }
    }
}

PhysicsContainer::RemoveResult PhysicsContainer::remove_entity(EntityID id) {
    mark_for_removal(id);
    flush_pending_removals();
    return {true, 0, false};  // упрощённо
}

PhysicsBody* PhysicsContainer::get_body(EntityID id) {
    auto [slot_idx, gen] = decompose_entity_id(id);
    if (slot_idx >= slots_.size() || !slots_[slot_idx].is_valid(gen)) {
        return nullptr;
    }
    return &slots_[slot_idx].body;
}

std::vector<PhysicsBody>& PhysicsContainer::get_all_bodies() {
    // Для совместимости, но лучше использовать foreach_active
    body_cache_.clear();
    for (uint32_t idx : active_indices_) {
        const auto& slot = slots_[idx];
        if (slot.state == PhysicsSlot::State::Occupied) {
            body_cache_.push_back(slot.body);
        }
    }
    return body_cache_;
}

const std::vector<PhysicsBody>& PhysicsContainer::get_all_bodies() const {
    // Аналогично
    const_cast<PhysicsContainer*>(this)->get_all_bodies();
    return body_cache_;
}

size_t PhysicsContainer::get_index(EntityID id) const {
    auto [slot_idx, gen] = decompose_entity_id(id);
    return slot_idx < slots_.size() && slots_[slot_idx].is_valid(gen) ? slot_idx : SIZE_MAX;
}

bool PhysicsContainer::contains(EntityID id) const {
    auto [slot_idx, gen] = decompose_entity_id(id);
    return slot_idx < slots_.size() && slots_[slot_idx].is_valid(gen);
}

void PhysicsContainer::sync_all_to_local_simd(const double origin[3]) {
    if (active_indices_.empty()) {
        return;
    }
    sync_global_to_local_simd(active_indices_.data(), active_indices_.size(), origin);
}

void PhysicsContainer::sync_global_to_local_simd(
    const size_t* indices,
    size_t count,
    const double origin[3]
) {
    if (count == 0) {
        return;
    }

    origin_x_ = origin[0];
    origin_y_ = origin[1];
    origin_z_ = origin[2];

    __m256d origin_x = _mm256_set1_pd(origin_x_);
    __m256d origin_y = _mm256_set1_pd(origin_y_);
    __m256d origin_z = _mm256_set1_pd(origin_z_);

    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        size_t idx0 = indices[i + 0];
        size_t idx1 = indices[i + 1];
        size_t idx2 = indices[i + 2];
        size_t idx3 = indices[i + 3];

        double positions_x[4] = { global_pos_x_[idx0], global_pos_x_[idx1], global_pos_x_[idx2], global_pos_x_[idx3] };
        double positions_y[4] = { global_pos_y_[idx0], global_pos_y_[idx1], global_pos_y_[idx2], global_pos_y_[idx3] };
        double positions_z[4] = { global_pos_z_[idx0], global_pos_z_[idx1], global_pos_z_[idx2], global_pos_z_[idx3] };

        __m256d gx = _mm256_loadu_pd(positions_x);
        __m256d gy = _mm256_loadu_pd(positions_y);
        __m256d gz = _mm256_loadu_pd(positions_z);

        __m256d local_x = _mm256_sub_pd(gx, origin_x);
        __m256d local_y = _mm256_sub_pd(gy, origin_y);
        __m256d local_z = _mm256_sub_pd(gz, origin_z);

        __m128 fx = _mm256_cvtpd_ps(local_x);
        __m128 fy = _mm256_cvtpd_ps(local_y);
        __m128 fz = _mm256_cvtpd_ps(local_z);

        float temp_x[4], temp_y[4], temp_z[4];
        _mm_storeu_ps(temp_x, fx);
        _mm_storeu_ps(temp_y, fy);
        _mm_storeu_ps(temp_z, fz);

        local_pos_x_[idx0] = temp_x[0];
        local_pos_x_[idx1] = temp_x[1];
        local_pos_x_[idx2] = temp_x[2];
        local_pos_x_[idx3] = temp_x[3];
        local_pos_y_[idx0] = temp_y[0];
        local_pos_y_[idx1] = temp_y[1];
        local_pos_y_[idx2] = temp_y[2];
        local_pos_y_[idx3] = temp_y[3];
        local_pos_z_[idx0] = temp_z[0];
        local_pos_z_[idx1] = temp_z[1];
        local_pos_z_[idx2] = temp_z[2];
        local_pos_z_[idx3] = temp_z[3];

        sync_body_cache_entry(idx0);
        sync_body_cache_entry(idx1);
        sync_body_cache_entry(idx2);
        sync_body_cache_entry(idx3);
    }

    for (; i < count; ++i) {
        size_t idx = indices[i];
        local_pos_x_[idx] = static_cast<float>(global_pos_x_[idx] - origin[0]);
        local_pos_y_[idx] = static_cast<float>(global_pos_y_[idx] - origin[1]);
        local_pos_z_[idx] = static_cast<float>(global_pos_z_[idx] - origin[2]);
        sync_body_cache_entry(idx);
    }
}

void PhysicsContainer::process_active_batch_simd(size_t start_idx, size_t batch_size, float dt) {
    size_t end = std::min(start_idx + batch_size, active_indices_.size());
    if (start_idx >= end) {
        return;
    }

    size_t i = start_idx;
    for (; i + 7 < end; i += 8) {
        size_t idx0 = active_indices_[i + 0];
        size_t idx1 = active_indices_[i + 1];
        size_t idx2 = active_indices_[i + 2];
        size_t idx3 = active_indices_[i + 3];
        size_t idx4 = active_indices_[i + 4];
        size_t idx5 = active_indices_[i + 5];
        size_t idx6 = active_indices_[i + 6];
        size_t idx7 = active_indices_[i + 7];

        float positions_x[8] = { local_pos_x_[idx0], local_pos_x_[idx1], local_pos_x_[idx2], local_pos_x_[idx3], local_pos_x_[idx4], local_pos_x_[idx5], local_pos_x_[idx6], local_pos_x_[idx7] };
        float positions_y[8] = { local_pos_y_[idx0], local_pos_y_[idx1], local_pos_y_[idx2], local_pos_y_[idx3], local_pos_y_[idx4], local_pos_y_[idx5], local_pos_y_[idx6], local_pos_y_[idx7] };
        float positions_z[8] = { local_pos_z_[idx0], local_pos_z_[idx1], local_pos_z_[idx2], local_pos_z_[idx3], local_pos_z_[idx4], local_pos_z_[idx5], local_pos_z_[idx6], local_pos_z_[idx7] };
        float velocities_x[8] = { velocity_x_[idx0], velocity_x_[idx1], velocity_x_[idx2], velocity_x_[idx3], velocity_x_[idx4], velocity_x_[idx5], velocity_x_[idx6], velocity_x_[idx7] };
        float velocities_y[8] = { velocity_y_[idx0], velocity_y_[idx1], velocity_y_[idx2], velocity_y_[idx3], velocity_y_[idx4], velocity_y_[idx5], velocity_y_[idx6], velocity_y_[idx7] };
        float velocities_z[8] = { velocity_z_[idx0], velocity_z_[idx1], velocity_z_[idx2], velocity_z_[idx3], velocity_z_[idx4], velocity_z_[idx5], velocity_z_[idx6], velocity_z_[idx7] };

        __m256 px = _mm256_loadu_ps(positions_x);
        __m256 py = _mm256_loadu_ps(positions_y);
        __m256 pz = _mm256_loadu_ps(positions_z);
        __m256 vx = _mm256_loadu_ps(velocities_x);
        __m256 vy = _mm256_loadu_ps(velocities_y);
        __m256 vz = _mm256_loadu_ps(velocities_z);
        __m256 dt_v = _mm256_set1_ps(dt);

        __m256 new_px = _mm256_add_ps(px, _mm256_mul_ps(vx, dt_v));
        __m256 new_py = _mm256_add_ps(py, _mm256_mul_ps(vy, dt_v));
        __m256 new_pz = _mm256_add_ps(pz, _mm256_mul_ps(vz, dt_v));

        _mm256_storeu_ps(positions_x, new_px);
        _mm256_storeu_ps(positions_y, new_py);
        _mm256_storeu_ps(positions_z, new_pz);

        local_pos_x_[idx0] = positions_x[0]; local_pos_x_[idx1] = positions_x[1]; local_pos_x_[idx2] = positions_x[2]; local_pos_x_[idx3] = positions_x[3];
        local_pos_x_[idx4] = positions_x[4]; local_pos_x_[idx5] = positions_x[5]; local_pos_x_[idx6] = positions_x[6]; local_pos_x_[idx7] = positions_x[7];
        local_pos_y_[idx0] = positions_y[0]; local_pos_y_[idx1] = positions_y[1]; local_pos_y_[idx2] = positions_y[2]; local_pos_y_[idx3] = positions_y[3];
        local_pos_y_[idx4] = positions_y[4]; local_pos_y_[idx5] = positions_y[5]; local_pos_y_[idx6] = positions_y[6]; local_pos_y_[idx7] = positions_y[7];
        local_pos_z_[idx0] = positions_z[0]; local_pos_z_[idx1] = positions_z[1]; local_pos_z_[idx2] = positions_z[2]; local_pos_z_[idx3] = positions_z[3];
        local_pos_z_[idx4] = positions_z[4]; local_pos_z_[idx5] = positions_z[5]; local_pos_z_[idx6] = positions_z[6]; local_pos_z_[idx7] = positions_z[7];

        sync_body_cache_entry(idx0);
        sync_body_cache_entry(idx1);
        sync_body_cache_entry(idx2);
        sync_body_cache_entry(idx3);
        sync_body_cache_entry(idx4);
        sync_body_cache_entry(idx5);
        sync_body_cache_entry(idx6);
        sync_body_cache_entry(idx7);
    }

    for (; i < end; ++i) {
        size_t idx = active_indices_[i];
        float px = local_pos_x_[idx] + velocity_x_[idx] * dt;
        float py = local_pos_y_[idx] + velocity_y_[idx] * dt;
        float pz = local_pos_z_[idx] + velocity_z_[idx] * dt;
        local_pos_x_[idx] = px;
        local_pos_y_[idx] = py;
        local_pos_z_[idx] = pz;
        sync_body_cache_entry(idx);
    }
}

void PhysicsContainer::update_matter_simd(float dt) {
    std::vector<size_t> rigid_indices;
    std::vector<size_t> fluid_indices;
    std::vector<size_t> gas_indices;
    rigid_indices.reserve(count_);
    fluid_indices.reserve(count_);
    gas_indices.reserve(count_);

    for (size_t i = 0; i < count_; ++i) {
        switch (entity_types_[i]) {
            case 0:
                rigid_indices.push_back(i);
                break;
            case 1:
                fluid_indices.push_back(i);
                break;
            case 2:
                gas_indices.push_back(i);
                break;
            default:
                break;
        }
    }

    auto simulate_batch = [&](const std::vector<size_t>& index_batch) {
        size_t batch = index_batch.size();
        size_t start = 0;
        while (start < batch) {
            size_t next = std::min(batch, start + 8);
            for (size_t t = start; t < next; ++t) {
                size_t idx = index_batch[t];
                float px = local_pos_x_[idx] + velocity_x_[idx] * dt;
                float py = local_pos_y_[idx] + velocity_y_[idx] * dt;
                float pz = local_pos_z_[idx] + velocity_z_[idx] * dt;
                local_pos_x_[idx] = px;
                local_pos_y_[idx] = py;
                local_pos_z_[idx] = pz;
                sync_body_cache_entry(idx);
            }
            start = next;
        }
    };

    simulate_batch(rigid_indices);
    simulate_batch(fluid_indices);
    simulate_batch(gas_indices);
}

void PhysicsContainer::reorganize_memory_layout() {
    std::vector<size_t> order(count_);
    for (size_t i = 0; i < count_; ++i) {
        order[i] = i;
    }

    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return morton_code(a) < morton_code(b);
    });

    std::vector<double> new_global_x(count_);
    std::vector<double> new_global_y(count_);
    std::vector<double> new_global_z(count_);
    std::vector<float> new_local_x(count_);
    std::vector<float> new_local_y(count_);
    std::vector<float> new_local_z(count_);
    std::vector<float> new_vel_x(count_);
    std::vector<float> new_vel_y(count_);
    std::vector<float> new_vel_z(count_);
    std::vector<float> new_mass(count_);
    std::vector<float> new_inverse_mass(count_);
    std::vector<uint32_t> new_types(count_);
    std::vector<uint32_t> new_flags(count_);
    std::vector<PhysicsBody> new_cache(count_);
    std::vector<size_t> new_index_to_slot(count_);

    for (size_t i = 0; i < count_; ++i) {
        size_t old_index = order[i];
        new_global_x[i] = global_pos_x_[old_index];
        new_global_y[i] = global_pos_y_[old_index];
        new_global_z[i] = global_pos_z_[old_index];
        new_local_x[i] = local_pos_x_[old_index];
        new_local_y[i] = local_pos_y_[old_index];
        new_local_z[i] = local_pos_z_[old_index];
        new_vel_x[i] = velocity_x_[old_index];
        new_vel_y[i] = velocity_y_[old_index];
        new_vel_z[i] = velocity_z_[old_index];
        new_mass[i] = mass_[old_index];
        new_inverse_mass[i] = inverse_mass_[old_index];
        new_types[i] = entity_types_[old_index];
        new_flags[i] = sync_flags_[old_index];
        new_cache[i] = body_cache_[old_index];
        new_index_to_slot[i] = index_to_registry_slot_[old_index];
    }

    global_pos_x_.swap(new_global_x);
    global_pos_y_.swap(new_global_y);
    global_pos_z_.swap(new_global_z);
    local_pos_x_.swap(new_local_x);
    local_pos_y_.swap(new_local_y);
    local_pos_z_.swap(new_local_z);
    velocity_x_.swap(new_vel_x);
    velocity_y_.swap(new_vel_y);
    velocity_z_.swap(new_vel_z);
    mass_.swap(new_mass);
    inverse_mass_.swap(new_inverse_mass);
    entity_types_.swap(new_types);
    sync_flags_.swap(new_flags);
    body_cache_.swap(new_cache);
    index_to_registry_slot_.swap(new_index_to_slot);

    for (size_t i = 0; i < count_; ++i) {
        size_t registry_slot = index_to_registry_slot_[i];
        if (registry_slot < registry_capacity_) {
            entity_registry_[registry_slot].array_index = i;
        }
    }

    active_indices_.clear();
    active_indices_.reserve(count_);
    for (size_t i = 0; i < count_; ++i) {
        active_indices_.push_back(i);
    }
    body_cache_dirty_ = false;
}

bool PhysicsContainer::validate_consistency() const {
    if (body_cache_.size() != count_ ||
        global_pos_x_.size() != count_ ||
        global_pos_y_.size() != count_ ||
        global_pos_z_.size() != count_ ||
        local_pos_x_.size() != count_ ||
        local_pos_y_.size() != count_ ||
        local_pos_z_.size() != count_ ||
        velocity_x_.size() != count_ ||
        velocity_y_.size() != count_ ||
        velocity_z_.size() != count_ ||
        mass_.size() != count_ ||
        inverse_mass_.size() != count_ ||
        entity_types_.size() != count_ ||
        sync_flags_.size() != count_ ||
        index_to_registry_slot_.size() != count_ ||
        active_indices_.size() != count_) {
        return false;
    }

    for (size_t i = 0; i < count_; ++i) {
        size_t slot = index_to_registry_slot_[i];
        if (slot >= registry_capacity_) {
            return false;
        }
        const EntitySlot& entry = entity_registry_[slot];
        if (entry.state != EntryState::OCCUPIED || entry.array_index != i || entry.id == INVALID_ENTITY_ID) {
            return false;
        }
    }
    return true;
}

size_t PhysicsContainer::find_slot(EntityID id) const {
    if (id == INVALID_ENTITY_ID) {
        return registry_capacity_;
    }

    size_t index = (static_cast<uint64_t>(id) * GOLDEN_RATIO_64) & (registry_capacity_ - 1);
    for (size_t probe = 0; probe < registry_capacity_; ++probe) {
        const EntitySlot& slot = entity_registry_[index];
        if (slot.state == EntryState::EMPTY) {
            return registry_capacity_;
        }
        if (slot.state == EntryState::OCCUPIED && slot.id == id) {
            return index;
        }
        index = (index + 1) & (registry_capacity_ - 1);
    }
    return registry_capacity_;
}

size_t PhysicsContainer::find_insert_slot(EntityID id) {
    size_t index = (static_cast<uint64_t>(id) * GOLDEN_RATIO_64) & (registry_capacity_ - 1);
    size_t first_deleted = registry_capacity_;
    for (size_t probe = 0; probe < registry_capacity_; ++probe) {
        EntitySlot& slot = entity_registry_[index];
        if (slot.state == EntryState::EMPTY) {
            return (first_deleted < registry_capacity_) ? first_deleted : index;
        }
        if (slot.state == EntryState::DELETED) {
            if (first_deleted == registry_capacity_) {
                first_deleted = index;
            }
        } else if (slot.id == id) {
            return index;
        }
        index = (index + 1) & (registry_capacity_ - 1);
    }
    return first_deleted < registry_capacity_ ? first_deleted : registry_capacity_;
}

void PhysicsContainer::ensure_registry_capacity(size_t min_capacity) {
    if (registry_capacity_ >= min_capacity) {
        return;
    }
    size_t new_capacity = registry_capacity_;
    while (new_capacity < min_capacity) {
        new_capacity <<= 1;
    }
    rehash(new_capacity);
}

void PhysicsContainer::rehash(size_t new_capacity) {
    new_capacity = std::max<size_t>(16, new_capacity);
    if ((new_capacity & (new_capacity - 1)) != 0) {
        size_t cap = 1;
        while (cap < new_capacity) {
            cap <<= 1;
        }
        new_capacity = cap;
    }
    std::vector<EntitySlot> new_registry(new_capacity, EntitySlot{INVALID_ENTITY_ID, 0, SIZE_MAX, EntryState::EMPTY});
    for (size_t i = 0; i < registry_capacity_; ++i) {
        const EntitySlot& slot = entity_registry_[i];
        if (slot.state == EntryState::OCCUPIED) {
            size_t index = (static_cast<uint64_t>(slot.id) * GOLDEN_RATIO_64) & (new_capacity - 1);
            while (new_registry[index].state == EntryState::OCCUPIED) {
                index = (index + 1) & (new_capacity - 1);
            }
            new_registry[index] = slot;
        }
    }
    entity_registry_.swap(new_registry);
    registry_capacity_ = new_capacity;
}

void PhysicsContainer::move_entity_data(size_t dest_index, size_t src_index) {
    body_cache_[dest_index] = body_cache_[src_index];
    global_pos_x_[dest_index] = global_pos_x_[src_index];
    global_pos_y_[dest_index] = global_pos_y_[src_index];
    global_pos_z_[dest_index] = global_pos_z_[src_index];
    local_pos_x_[dest_index] = local_pos_x_[src_index];
    local_pos_y_[dest_index] = local_pos_y_[src_index];
    local_pos_z_[dest_index] = local_pos_z_[src_index];
    velocity_x_[dest_index] = velocity_x_[src_index];
    velocity_y_[dest_index] = velocity_y_[src_index];
    velocity_z_[dest_index] = velocity_z_[src_index];
    mass_[dest_index] = mass_[src_index];
    inverse_mass_[dest_index] = inverse_mass_[src_index];
    entity_types_[dest_index] = entity_types_[src_index];
    sync_flags_[dest_index] = sync_flags_[src_index];
}

void PhysicsContainer::sync_body_cache_entry(size_t index) {
    PhysicsBody& body = body_cache_[index];
    body.position.x = static_cast<float>(local_pos_x_[index] + origin_x_);
    body.position.y = static_cast<float>(local_pos_y_[index] + origin_y_);
    body.position.z = static_cast<float>(local_pos_z_[index] + origin_z_);
    body.velocity.x = velocity_x_[index];
    body.velocity.y = velocity_y_[index];
    body.velocity.z = velocity_z_[index];

    global_pos_x_[index] = body.position.x;
    global_pos_y_[index] = body.position.y;
    global_pos_z_[index] = body.position.z;
}

uint64_t PhysicsContainer::morton_code(size_t index) const {
    uint32_t x = static_cast<uint32_t>(std::lround(global_pos_x_[index]));
    uint32_t y = static_cast<uint32_t>(std::lround(global_pos_y_[index]));
    uint32_t z = static_cast<uint32_t>(std::lround(global_pos_z_[index]));
    return morton3D(x, y, z);
}

}  // namespace physics_core
