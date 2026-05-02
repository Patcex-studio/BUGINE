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
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace physics_core {

// ============================================================================
// Lock-Free Queue for Work-Stealing
// ============================================================================

template<size_t QUEUE_SIZE = 1024>
struct alignas(64) LockFreeQueue {
    std::array<std::function<void()>, QUEUE_SIZE> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    LockFreeQueue() = default;
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&& other) noexcept {
        head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        tail_.store(other.tail_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        for (size_t i = 0; i < QUEUE_SIZE; ++i) {
            buffer_[i] = std::move(other.buffer_[i]);
        }
    }
    LockFreeQueue& operator=(LockFreeQueue&& other) noexcept {
        if (this != &other) {
            head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            tail_.store(other.tail_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            for (size_t i = 0; i < QUEUE_SIZE; ++i) {
                buffer_[i] = std::move(other.buffer_[i]);
            }
        }
        return *this;
    }

    bool try_push(std::function<void()> task) noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next = (tail + 1) % QUEUE_SIZE;
        if (next == head_.load(std::memory_order_acquire)) return false; // full
        buffer_[tail] = std::move(task);
        tail_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<std::function<void()>> try_pop() noexcept {
        size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) return std::nullopt;
        auto task = std::move(buffer_[head]);
        head_.store((head + 1) % QUEUE_SIZE, std::memory_order_release);
        return task;
    }

    std::optional<std::function<void()>> try_steal() noexcept {
        return try_pop(); // For simplicity, steal from head
    }
};

// ============================================================================
// Worker Stats
// ============================================================================

struct alignas(64) WorkerStats {
    std::atomic<size_t> tasks_executed{0};
    std::atomic<size_t> steal_attempts{0};

    WorkerStats() = default;
    WorkerStats(const WorkerStats&) = delete;
    WorkerStats& operator=(const WorkerStats&) = delete;
    WorkerStats(WorkerStats&& other) noexcept {
        tasks_executed.store(other.tasks_executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
        steal_attempts.store(other.steal_attempts.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    WorkerStats& operator=(WorkerStats&& other) noexcept {
        if (this != &other) {
            tasks_executed.store(other.tasks_executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            steal_attempts.store(other.steal_attempts.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
};

// ============================================================================
// Physics Thread Pool
// ============================================================================

/**
 * @class PhysicsThreadPool
 * @brief Multi-threaded physics task execution with work-stealing
 * 
 * Criteria:
 * - Support up to 16 worker threads
 * - No race conditions
 * - Synchronization with nanosecond precision
 */
class PhysicsThreadPool {
public:
    // Task function type
    using Task = std::function<void()>;

    /**
     * Constructor
     * @param thread_count Number of worker threads (0 = auto-detect)
     */
    PhysicsThreadPool(size_t thread_count = 0);
    ~PhysicsThreadPool();

    // ========== Task Submission ==========

    /**
     * Submit a task to the thread pool
     * @param task Function to execute
     */
    void submit_task(Task task);

    /**
     * Submit multiple identical tasks
     * Useful for distributing body processing
     * @param count Number of tasks
     * @param make_task Function that creates a task given task index
     */
    void submit_batch_tasks(
        size_t count,
        std::function<Task(size_t)> make_task
    );

    /**
     * Wait for all tasks to complete
     */
    void synchronize();

    /**
     * Submit tasks for phase 1 (broad-phase)
     */
    void submit_phase1_tasks(std::vector<Task> tasks);

    /**
     * Wait for phase 1 completion
     */
    void wait_for_phase1();

    /**
     * Submit tasks for phase 2 (narrow-phase)
     */
    void submit_phase2_tasks(std::vector<Task> tasks);

    /**
     * Wait for all phases
     */
    void wait_all();

    // ========== Body Distribution ==========

    /**
     * Distribute bodies among threads for parallel processing
     * @param bodies Vector of bodies to process
     * @param process_func Function to call for each batch
     * @param batch_size Bodies per thread batch
     */
    void distribute_bodies(
        std::vector<PhysicsBody>& bodies,
        std::function<void(size_t start, size_t batch_size)> process_func,
        size_t batch_size = 0
    );

    /**
     * Get number of worker threads
     */
    size_t get_thread_count() const { return workers_.size(); }

    /**
     * Get number of queued tasks
     */
    size_t get_pending_tasks() const;

    /**
     * Get statistics
     */
    struct Stats {
        size_t total_tasks_executed;
        double total_time_ms;
        double average_task_time_us;
    };

    Stats getStats() const;

private:
    std::vector<std::unique_ptr<std::thread>> workers_;
    std::vector<LockFreeQueue<>> per_worker_queues_;
    alignas(64) std::atomic<size_t> active_tasks_{0};
    std::vector<WorkerStats> stats_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable sync_cv_;
    bool shutdown_;

    void worker_loop(size_t worker_id);
    void set_thread_affinity(size_t worker_id);
};

}  // namespace physics_core
