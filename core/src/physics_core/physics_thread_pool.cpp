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
#include "physics_core/physics_thread_pool.h"
#include <iostream>
#include <barrier>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace physics_core {

PhysicsThreadPool::PhysicsThreadPool(size_t thread_count)
    : shutdown_(false) {
    
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 4;  // Fallback
        }
    }
    
    per_worker_queues_.resize(thread_count);
    stats_.resize(thread_count);
    
    workers_.resize(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        workers_[i] = std::make_unique<std::thread>(&PhysicsThreadPool::worker_loop, this, i);
    }
}

PhysicsThreadPool::~PhysicsThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
    }
    queue_cv_.notify_all();
    sync_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker && worker->joinable()) {
            worker->join();
        }
    }
}

void PhysicsThreadPool::submit_task(Task task) {
    size_t min_load = SIZE_MAX;
    size_t target_worker = 0;
    for (size_t i = 0; i < per_worker_queues_.size(); ++i) {
        size_t load = (per_worker_queues_[i].tail_.load(std::memory_order_relaxed) + per_worker_queues_[i].buffer_.size() - per_worker_queues_[i].head_.load(std::memory_order_relaxed)) % per_worker_queues_[i].buffer_.size();
        if (load < min_load) {
            min_load = load;
            target_worker = i;
        }
    }
    while (!per_worker_queues_[target_worker].try_push(std::move(task))) {
        target_worker = (target_worker + 1) % per_worker_queues_.size();
    }
    active_tasks_.fetch_add(1, std::memory_order_release);
    queue_cv_.notify_one();
}

void PhysicsThreadPool::submit_batch_tasks(
    size_t count,
    std::function<Task(size_t)> make_task
) {
    for (size_t i = 0; i < count; ++i) {
        submit_task(make_task(i));
    }
}

void PhysicsThreadPool::synchronize() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    sync_cv_.wait(lock, [this] {
        if (active_tasks_.load(std::memory_order_acquire) != 0) return false;
        for (const auto& queue : per_worker_queues_) {
            if (queue.head_.load(std::memory_order_relaxed) != queue.tail_.load(std::memory_order_relaxed)) {
                return false;
            }
        }
        return true;
    });
}

void PhysicsThreadPool::submit_phase1_tasks(std::vector<Task> tasks) {
    for (auto& task : tasks) {
        submit_task(std::move(task));
    }
}

void PhysicsThreadPool::wait_for_phase1() {
    synchronize();
}

void PhysicsThreadPool::submit_phase2_tasks(std::vector<Task> tasks) {
    for (auto& task : tasks) {
        submit_task(std::move(task));
    }
}

void PhysicsThreadPool::wait_all() {
    synchronize();
}

void PhysicsThreadPool::distribute_bodies(
    std::vector<PhysicsBody>& bodies,
    std::function<void(size_t start, size_t batch_size)> process_func,
    size_t batch_size
) {
    if (batch_size == 0) {
        batch_size = std::max(size_t(1), bodies.size() / workers_.size());
    }
    
    size_t remaining = bodies.size();
    size_t start_idx = 0;
    
    while (remaining > 0) {
        size_t current_batch = std::min(batch_size, remaining);
        submit_task([process_func, start_idx, current_batch]() {
            process_func(start_idx, current_batch);
        });
        
        start_idx += current_batch;
        remaining -= current_batch;
    }
    
    synchronize();
}

size_t PhysicsThreadPool::get_pending_tasks() const {
    size_t total = 0;
    for (const auto& queue : per_worker_queues_) {
        total += (queue.tail_.load() - queue.head_.load() + queue.buffer_.size()) % queue.buffer_.size();
    }
    return total;
}

PhysicsThreadPool::Stats PhysicsThreadPool::getStats() const {
    size_t total_executed = 0;
    for (const auto& stat : stats_) {
        total_executed += stat.tasks_executed.load();
    }
    return {
        total_executed,
        0.0,  // Total time not tracked yet
        0.0   // Average task time not tracked yet
    };
}

void PhysicsThreadPool::worker_loop(size_t worker_id) {
    set_thread_affinity(worker_id);

    while (true) {
        std::optional<Task> task;
        for (size_t attempt = 0; attempt < per_worker_queues_.size(); ++attempt) {
            size_t index = (worker_id + attempt) % per_worker_queues_.size();
            if (attempt == 0) {
                task = per_worker_queues_[index].try_pop();
            } else {
                task = per_worker_queues_[index].try_steal();
            }
            if (task) {
                break;
            }
        }

        if (task) {
            (*task)();
            stats_[worker_id].tasks_executed.fetch_add(1, std::memory_order_relaxed);
            active_tasks_.fetch_sub(1, std::memory_order_release);
            if (active_tasks_.load(std::memory_order_acquire) == 0) {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                sync_cv_.notify_all();
            }
            continue;
        }

        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] {
            if (shutdown_) return true;
            if (active_tasks_.load(std::memory_order_acquire) > 0) return true;
            for (const auto& queue : per_worker_queues_) {
                if (queue.head_.load(std::memory_order_relaxed) != queue.tail_.load(std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        });
        if (shutdown_) {
            break;
        }
    }
}

void PhysicsThreadPool::set_thread_affinity(size_t worker_id) {
#ifdef _WIN32
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = 1ULL << worker_id;
    SetThreadAffinityMask(thread, mask);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker_id % CPU_SETSIZE, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

}  // namespace physics_core
