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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <immintrin.h>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace performance {

enum class BottleneckType : uint32_t {
    None = 0,
    CPU_BOUND,
    GPU_BOUND,
    MEMORY_BOUND,
    IO_BOUND,
    NETWORK_BOUND
};

enum class PerformanceMetric : uint8_t {
    FrameTimeMs,
    FPS,
    PhysicsTimeMs,
    RenderTimeMs,
    MemoryUsageMb,
    NetworkLatencyMs,
    AIDecisionTimeMs
};

struct Recommendation {
    std::string text;
};

struct BottleneckPattern {
    std::string name;
    float expected_impact_pct = 0.0f;
};

struct PerformanceThreshold {
    PerformanceMetric metric;
    float limit;
    bool above_is_bad;
};

struct PerformanceMetrics {
    float frame_time_ms = 0.0f;
    float avg_frame_time_ms = 0.0f;
    float min_frame_time_ms = 0.0f;
    float max_frame_time_ms = 0.0f;
    uint32_t fps = 0;
    uint32_t avg_fps = 0;

    uint64_t total_memory_used_mb = 0;
    uint64_t peak_memory_used_mb = 0;
    uint64_t gpu_memory_used_mb = 0;
    float memory_fragmentation_pct = 0.0f;

    uint32_t physics_bodies_count = 0;
    float physics_update_time_ms = 0.0f;
    uint32_t collision_checks_count = 0;
    float broad_phase_time_ms = 0.0f;
    float narrow_phase_time_ms = 0.0f;
    float integration_time_ms = 0.0f;

    uint32_t draw_calls_count = 0;
    uint32_t triangles_rendered = 0;
    float render_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;
    float vsync_wait_time_ms = 0.0f;
    uint32_t texture_memory_mb = 0;
    uint32_t vertex_memory_mb = 0;

    uint32_t network_packets_sent = 0;
    uint32_t network_packets_recv = 0;
    float network_latency_ms = 0.0f;
    float network_jitter_ms = 0.0f;
    uint32_t bandwidth_upload_kbps = 0;
    uint32_t bandwidth_download_kbps = 0;

    uint32_t ai_units_count = 0;
    float ai_decision_time_ms = 0.0f;
    uint32_t pathfinding_queries = 0;
    float pathfinding_avg_time_ms = 0.0f;
    uint32_t behavior_tree_ticks = 0;

    void reset() noexcept;
};

struct alignas(32) MetricsSampler {
    std::array<float, 8> frame_times{};
    std::array<float, 8> physics_times{};
    std::array<float, 8> render_times{};
    std::array<float, 8> network_latencies{};
    uint32_t sample_count = 0;
    uint32_t current_sample = 0;
    float collection_interval_ms = 16.0f;

    MetricsSampler() noexcept;
    void add_sample(float frame_time, float physics_time, float render_time, float network_latency) noexcept;
    float average_frame_time() const noexcept;
    float average_physics_time() const noexcept;
    float average_render_time() const noexcept;
    float average_network_latency() const noexcept;
    void reset() noexcept;
};

struct PerformanceBottleneck {
    uint32_t bottleneck_type = static_cast<uint32_t>(BottleneckType::None);
    char bottleneck_name[64] = {};
    float severity_score = 0.0f;
    float performance_impact_pct = 0.0f;
    uint32_t detection_timestamp = 0;
    float duration_seconds = 0.0f;
    std::vector<Recommendation> recommendations;
    bool is_resolved = false;
    float resolution_time = 0.0f;
};

struct PerformanceReport {
    std::string summary;
    std::string details;
    std::string format;
};

class PerformanceAnalyzer {
public:
    PerformanceAnalyzer();

    void analyze_performance(const PerformanceMetrics& current_metrics,
                             std::vector<PerformanceBottleneck>& detected_bottlenecks);
    bool detect_cpu_bound_bottleneck(const PerformanceMetrics& metrics) const;
    bool detect_gpu_bound_bottleneck(const PerformanceMetrics& metrics) const;
    bool detect_memory_bound_bottleneck(const PerformanceMetrics& metrics) const;
    bool detect_io_bound_bottleneck(const PerformanceMetrics& metrics) const;
    bool detect_network_bound_bottleneck(const PerformanceMetrics& metrics) const;

    bool match_known_patterns(const PerformanceMetrics& metrics);
    void learn_new_patterns(const PerformanceMetrics& metrics);
    std::vector<Recommendation> generate_recommendations(
        const PerformanceBottleneck& bottleneck) const;
    void update_thresholds(const PerformanceMetrics& metrics);
    bool is_threshold_crossed(const PerformanceMetrics& metrics,
                              const PerformanceThreshold& threshold) const;

private:
    std::mutex mutex_;
    std::vector<PerformanceBottleneck> detected_bottlenecks_;
    std::vector<BottleneckPattern> known_patterns_;
    std::vector<PerformanceThreshold> thresholds_;
    std::deque<PerformanceMetrics> historical_metrics_;
    PerformanceMetrics baseline_metrics_;

    PerformanceBottleneck make_bottleneck(BottleneckType type,
                                          const char* name,
                                          float severity,
                                          float impact,
                                          uint32_t timestamp,
                                          float duration) const;
};

class PerformanceReporter {
public:
    PerformanceReporter();

    void generate_performance_report(const PerformanceMetrics& metrics,
                                     const std::vector<PerformanceBottleneck>& bottlenecks,
                                     PerformanceReport& report) const;
    std::string export_json(const PerformanceMetrics& metrics,
                            const std::vector<PerformanceBottleneck>& bottlenecks) const;
    std::string export_csv(const PerformanceMetrics& metrics,
                            const std::vector<PerformanceBottleneck>& bottlenecks) const;
    std::string export_html(const PerformanceMetrics& metrics,
                             const std::vector<PerformanceBottleneck>& bottlenecks) const;
    void generate_historical_analysis(const std::vector<PerformanceMetrics>& historical_data,
                                      PerformanceReport& report) const;
};

struct FunctionProfile {
    uint32_t function_id = 0;
    char function_name[128] = {};
    char source_file[64] = {};
    uint32_t line_number = 0;
    uint64_t call_count = 0;
    float total_time_ms = 0.0f;
    float average_time_ms = 0.0f;
    float min_time_ms = 0.0f;
    float max_time_ms = 0.0f;
    float exclusive_time_ms = 0.0f;
    float inclusive_time_ms = 0.0f;
    float self_time_percent = 0.0f;
    uint32_t recursion_depth = 0;
    float memory_allocated_kb = 0.0f;

    void update(float duration_ms, float exclusive_ms, uint32_t current_depth) noexcept;
};

class GlobalProfiler {
public:
    GlobalProfiler();
    ~GlobalProfiler();

    void start_profiling_session();
    void stop_profiling_session();
    void pause_profiling();
    void resume_profiling();
    void reset_profiling_data();

    std::vector<FunctionProfile> get_top_performance_hogs(uint32_t count = 10) const;
    float get_total_profile_time() const;
    float get_average_frame_time() const;

    static void register_function(uint32_t function_id,
                                  const char* name,
                                  const char* source_file,
                                  uint32_t line_number);
    static void start_profiling(uint32_t function_id);
    static void stop_profiling(uint32_t function_id);
    static float elapsed_ms(const std::chrono::high_resolution_clock::time_point& start) noexcept;

private:
    struct CallFrame {
        uint32_t function_id;
        std::chrono::high_resolution_clock::time_point start_time;
        float child_duration_ms = 0.0f;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, FunctionProfile> function_profiles_;
    mutable std::vector<FunctionProfile> sorted_profiles_;
    std::atomic<bool> is_enabled_{false};
    std::atomic<bool> is_recording_{false};
    uint64_t profile_start_time_ns_ = 0;

    static std::atomic<GlobalProfiler*> active_profiler_;
    static thread_local std::vector<CallFrame> call_stack_;

    static uint64_t now_ns() noexcept;
    void update_profile_data(uint32_t function_id,
                             float duration_ms,
                             float exclusive_ms,
                             uint32_t recursion_depth) noexcept;
};

class ProfileScopeGuard {
public:
    ProfileScopeGuard(uint32_t func_id, float& total_time_ref);
    ~ProfileScopeGuard();

    static void start_profiling(uint32_t function_id);
    static void stop_profiling(uint32_t function_id);
    static void register_function(const char* name,
                                  const char* file,
                                  uint32_t line);

private:
    uint32_t function_id_;
    std::chrono::high_resolution_clock::time_point start_time_;
    float* total_time_ptr_;
};

class PerformanceMonitor {
public:
    PerformanceMonitor();

    void collect_metrics(PerformanceMetrics& metrics);
    void monitor_physics_performance(const PerformanceMetrics& physics_snapshot,
                                     PerformanceMetrics& metrics);
    void monitor_rendering_performance(const PerformanceMetrics& rendering_snapshot,
                                       PerformanceMetrics& metrics);
    void monitor_network_performance(const PerformanceMetrics& network_snapshot,
                                     PerformanceMetrics& metrics);
    void monitor_ai_performance(const PerformanceMetrics& ai_snapshot,
                                PerformanceMetrics& metrics);
    void monitor_large_battle_performance(uint32_t expected_units,
                                          uint32_t expected_entities,
                                          uint32_t active_entities,
                                          PerformanceMetrics& metrics);

    void set_collection_interval(float interval_ms) noexcept;
    const std::deque<PerformanceMetrics>& history() const noexcept;

private:
    MetricsSampler sampler_;
    std::deque<PerformanceMetrics> history_;
    size_t max_history_ = 256;
};

} // namespace performance
