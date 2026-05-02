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

#include "physics_body.h"
#include <cstdint>
#include <vector>
#include <immintrin.h>

namespace physics_core {

class PhysicsContainer {
public:
    struct RemoveResult {
        bool success;
        size_t removed_index;
        bool swapped_from_back;
    };

    static constexpr uint32_t FLAG_ACTIVE = 0x01;
    static constexpr uint32_t FLAG_DIRTY = 0x02;

    explicit PhysicsContainer(size_t reserve = 1024);
    ~PhysicsContainer();

    EntityID register_entity(const PhysicsBody& body);
    RemoveResult remove_entity(EntityID id);
    PhysicsBody* get_body(EntityID id);
    size_t get_index(EntityID id) const;
    std::vector<PhysicsBody>& get_all_bodies();
    const std::vector<PhysicsBody>& get_all_bodies() const;
    size_t get_entity_count() const { return count_; }
    size_t get_active_count() const { return active_indices_.size(); }

    float* get_local_pos_x_ptr() { return local_pos_x_.data(); }
    float* get_local_pos_y_ptr() { return local_pos_y_.data(); }
    float* get_local_pos_z_ptr() { return local_pos_z_.data(); }

    void sync_all_to_local_simd(const double origin[3]);
    void sync_global_to_local_simd(
        const size_t* indices,
        size_t count,
        const double origin[3]
    );

    void process_active_batch_simd(size_t start_idx, size_t batch_size, float dt);
    void update_matter_simd(float dt);

    void reorganize_memory_layout();
    bool validate_consistency() const;
    bool contains(EntityID id) const;

    // Новые методы для отложенного удаления
    void mark_for_removal(EntityID entity);
    void flush_pending_removals();

    // Детерминированная итерация
    template<typename Func>
    void foreach_active(Func&& fn) const {
        for (uint32_t idx : active_indices_) {
            const auto& slot = slots_[idx];
            if (slot.state == PhysicsSlot::State::Occupied) {
                fn(slot.body, make_entity_id(idx, slot.generation));
            }
        }
    }

private:
    // Удалим старые методы, которые больше не нужны
    enum class EntryState : uint8_t {
        EMPTY,
        OCCUPIED,
        DELETED
    };

    struct EntitySlot {
        EntityID id;
        uint32_t flags;
        size_t array_index;
        EntryState state;
    };

    // Новый слот с поколением
    struct alignas(64) PhysicsSlot {
        enum class State : uint8_t { Free, Occupied, PendingRemoval };
        
        PhysicsBody body;
        State state;
        uint32_t generation;          // версия слота (для валидности указателей)
        uint32_t next_free_index;     // для свободной очереди (индекс, не указатель)
        
        // Проверка валидности с защитой от переполнения
        bool is_valid(uint32_t query_generation) const noexcept {
            // Если поколение "перескочило" через максимум — считаем невалидным
            if (generation == UINT32_MAX && query_generation < UINT32_MAX / 2) return false;
            return state == State::Occupied && generation == query_generation;
        }
    };

    // Вспомогательные функции
    EntityID make_entity_id(uint32_t slot_idx, uint32_t generation) const;
    std::pair<uint32_t, uint32_t> decompose_entity_id(EntityID entity) const;
    void expand_pool();

    std::vector<PhysicsSlot> slots_;
    uint32_t free_list_head_;     // индекс первого свободного слота
    uint32_t total_created_;      // счётчик созданий (для детекта приближения к overflow)
    
    // Детерминированный порядок итерации (по индексу, не по хэшу)
    std::vector<size_t> active_indices_;  // индексы занятых слотов, отсортированы

    // Старые поля для совместимости (пока что)
    size_t count_;
    size_t registry_capacity_;
    std::vector<EntitySlot> entity_registry_;
    std::vector<size_t> index_to_registry_slot_;
    std::vector<size_t> free_registry_slots_;

    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    std::vector<double> global_pos_x_;
    std::vector<double> global_pos_y_;
    std::vector<double> global_pos_z_;
    std::vector<float> local_pos_x_;
    std::vector<float> local_pos_y_;
    std::vector<float> local_pos_z_;
    std::vector<float> velocity_x_;
    std::vector<float> velocity_y_;
    std::vector<float> velocity_z_;
    std::vector<float> mass_;
    std::vector<float> inverse_mass_;
    std::vector<uint32_t> entity_types_;
    std::vector<uint32_t> sync_flags_;

    std::vector<PhysicsBody> body_cache_;
    mutable bool body_cache_dirty_;

    // Для Verlet интегратора
    std::vector<Vec3> prev_positions_;

    double origin_x_;
    double origin_y_;
    double origin_z_;

    size_t find_slot(EntityID id) const;
    size_t find_insert_slot(EntityID id);
    void ensure_registry_capacity(size_t min_capacity);
    void rehash(size_t new_capacity);
    void move_entity_data(size_t dest_index, size_t src_index);
    void sync_body_cache_entry(size_t index);
    void sync_body_cache_range(size_t start, size_t count);
    uint64_t morton_code(size_t index) const;
};

}  // namespace physics_core
