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

#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <thread>

namespace physics_core {

// ============================================================================
// PROFILING INFRASTRUCTURE - Lock-Free, Zero-Overhead in Release Mode
// ============================================================================

#ifdef PHYSICS_PROFILING

// ============================================================================
// ProfileRecord: Lock-free ring buffer entry
// ============================================================================
struct alignas(64) ProfileRecord {
    const char* name;
    uint64_t start_ticks;
    uint64_t end_ticks;
    uint32_t thread_id;
    uint8_t padding[64 - sizeof(const char*) - 2*sizeof(uint64_t) - sizeof(uint32_t)];
};

// ============================================================================
// LockFreeProfileBuffer: Per-thread profiling ring buffer
// ============================================================================
template<size_t Capacity = 4096>
class LockFreeProfileBuffer {
    std::array<ProfileRecord, Capacity> buffer_;
    std::atomic<size_t> write_idx_{0};

public:
    bool push(const ProfileRecord& record) noexcept {
        size_t idx = write_idx_.fetch_add(1, std::memory_order_relaxed);
        buffer_[idx % Capacity] = record;
        return true;
    }

    void clear() noexcept {
        write_idx_.store(0, std::memory_order_relaxed);
    }

    size_t size() const noexcept {
        return write_idx_.load(std::memory_order_relaxed);
    }

    const ProfileRecord& get(size_t idx) const noexcept {
        return buffer_[idx % Capacity];
    }
};

// Thread-local profiling buffer (per-thread to avoid false sharing)
extern thread_local LockFreeProfileBuffer<4096> g_thread_profile_buffer;

// ============================================================================
// ProfileBufferRegistry: Track thread-local buffers for aggregation
// ============================================================================
class ProfileBufferRegistry {
public:
    static ProfileBufferRegistry& instance() noexcept {
        static ProfileBufferRegistry s_instance;
        return s_instance;
    }

    void register_buffer(LockFreeProfileBuffer<4096>* buffer) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        buffers_.push_back(buffer);
    }

    std::vector<LockFreeProfileBuffer<4096>*> get_buffers() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffers_;
    }

private:
    ProfileBufferRegistry() = default;
    ~ProfileBufferRegistry() = default;
    ProfileBufferRegistry(const ProfileBufferRegistry&) = delete;
    ProfileBufferRegistry& operator=(const ProfileBufferRegistry&) = delete;

    mutable std::mutex mutex_;
    std::vector<LockFreeProfileBuffer<4096>*> buffers_;
};

// ============================================================================
// ProfileTimer: RAII timer using RDTSC or chrono
// ============================================================================
class ProfileTimer {
    const char* name_;
    uint64_t start_ticks_;

    // Inline RDTSC for fast timing (X86/X64 only)
    static inline uint64_t get_rdtsc() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned int lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (uint64_t)hi << 32 | lo;
#elif defined(__i386__) || defined(_M_IX86)
        unsigned int lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (uint64_t)hi << 32 | lo;
#else
        // Fallback to chrono for non-x86 platforms
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#endif
    }

public:
    explicit ProfileTimer(const char* name) noexcept
        : name_(name), start_ticks_(get_rdtsc()) {}

    ~ProfileTimer() noexcept {
        uint64_t end_ticks = get_rdtsc();
        uint32_t thread_id = static_cast<uint32_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFFFF);

        ProfileRecord record{
            .name = name_,
            .start_ticks = start_ticks_,
            .end_ticks = end_ticks,
            .thread_id = thread_id
        };
        g_thread_profile_buffer.push(record);
    }
};

// ============================================================================
// ProfileManager: Singleton for collecting and aggregating profiling data
// ============================================================================
class ProfileManager {
public:
    struct AggregateStats {
        uint64_t total_ticks = 0;
        uint64_t call_count = 0;
        uint64_t min_ticks = ~0ULL;
        uint64_t max_ticks = 0;

        void add_sample(uint64_t ticks) noexcept {
            total_ticks += ticks;
            call_count++;
            if (ticks < min_ticks) min_ticks = ticks;
            if (ticks > max_ticks) max_ticks = ticks;
        }

        double avg_us(double cpu_freq_ghz) const noexcept {
            if (call_count == 0) return 0.0;
            return (total_ticks / double(call_count)) / (cpu_freq_ghz * 1000.0);
        }

        double avg_ms(double cpu_freq_ghz) const noexcept {
            return avg_us(cpu_freq_ghz) / 1000.0;
        }

        void reset() noexcept {
            total_ticks = 0;
            call_count = 0;
            min_ticks = ~0ULL;
            max_ticks = 0;
        }
    };

    static ProfileManager& instance() noexcept {
        static ProfileManager s_instance;
        return s_instance;
    }

    // Add sample directly (used by ProfileTimer)
    void add_record(const char* name, uint64_t ticks) noexcept {
        std::string key(name ? name : "<unknown>");
        stats_[key].add_sample(ticks);
    }

    // Aggregate data from all thread-local buffers
    void aggregate_frame_data() noexcept {
        auto buffers = ProfileBufferRegistry::instance().get_buffers();
        for (auto* buffer : buffers) {
            if (!buffer) continue;
            size_t count = buffer->size();
            for (size_t i = 0; i < count; ++i) {
                const ProfileRecord& record = buffer->get(i);
                uint64_t ticks = record.end_ticks - record.start_ticks;
                add_record(record.name, ticks);
            }
            buffer->clear();
        }
    }

    // Print summary (call once per second or on demand)
    void print_summary(double cpu_freq_ghz = 3.6) noexcept {
        if (stats_.empty()) return;

        printf("\n========== PHYSICS PROFILING SUMMARY (%.2f GHz) ==========\n", cpu_freq_ghz);
        printf("%-40s | %10s | %10s | %10s | %10s\n",
               "Scope Name", "Avg (us)", "Min (us)", "Max (us)", "Calls");
        printf("%-40s-%-10s-%-10s-%-10s-%-10s\n",
               "------------------------------------", "----------", "----------", "----------", "----------");

        for (const auto& [name, stats] : stats_) {
            printf("%-40s | %10.3f | %10.3f | %10.3f | %10zu\n",
                   name.c_str(),
                   stats.avg_us(cpu_freq_ghz),
                   stats.min_ticks / (cpu_freq_ghz * 1000.0),
                   stats.max_ticks / (cpu_freq_ghz * 1000.0),
                   stats.call_count);
        }
        printf("=========================================================\n\n");
    }

    // Reset all statistics
    void reset() noexcept {
        stats_.clear();
    }

    const std::unordered_map<std::string, AggregateStats>& get_stats() const noexcept {
        return stats_;
    }

private:
    ProfileManager() = default;
    ~ProfileManager() = default;
    ProfileManager(const ProfileManager&) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    std::unordered_map<std::string, AggregateStats> stats_;
};

// ============================================================================
// Public Macros: Use these in your code
// ============================================================================

#define PHYSICS_PROFILE_SCOPE(name) \
    physics_core::ProfileTimer _physics_profile_timer_##__LINE__(name)

#define PHYSICS_PROFILE_FUNCTION() \
    PHYSICS_PROFILE_SCOPE(__FUNCTION__)

#else
// ============================================================================
// Release mode: All profiling is compiled out to zero overhead
// ============================================================================

#define PHYSICS_PROFILE_SCOPE(name) static_assert(true)
#define PHYSICS_PROFILE_FUNCTION() static_assert(true)

#endif  // PHYSICS_PROFILING

}  // namespace physics_core
