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
#include "performance_monitoring.h"

#include <algorithm>
#include <cmath>
#include <iomanip>

namespace performance {

void PerformanceMetrics::reset() noexcept {
    frame_time_ms = 0.0f;
    avg_frame_time_ms = 0.0f;
    min_frame_time_ms = 0.0f;
    max_frame_time_ms = 0.0f;
    fps = 0;
    avg_fps = 0;
    total_memory_used_mb = 0;
    peak_memory_used_mb = 0;
    gpu_memory_used_mb = 0;
    memory_fragmentation_pct = 0.0f;
    physics_bodies_count = 0;
    physics_update_time_ms = 0.0f;
    collision_checks_count = 0;
    broad_phase_time_ms = 0.0f;
    narrow_phase_time_ms = 0.0f;
    integration_time_ms = 0.0f;
    draw_calls_count = 0;
    triangles_rendered = 0;
    render_time_ms = 0.0f;
    gpu_time_ms = 0.0f;
    vsync_wait_time_ms = 0.0f;
    texture_memory_mb = 0;
    vertex_memory_mb = 0;
    network_packets_sent = 0;
    network_packets_recv = 0;
    network_latency_ms = 0.0f;
    network_jitter_ms = 0.0f;
    bandwidth_upload_kbps = 0;
    bandwidth_download_kbps = 0;
    ai_units_count = 0;
    ai_decision_time_ms = 0.0f;
    pathfinding_queries = 0;
    pathfinding_avg_time_ms = 0.0f;
    behavior_tree_ticks = 0;
}

MetricsSampler::MetricsSampler() noexcept {
    reset();
}

static inline float horizontal_add(const __m256 value) noexcept {
    alignas(32) float temp[8];
    _mm256_store_ps(temp, value);
    float result = 0.0f;
    for (int i = 0; i < 8; ++i) {
        result += temp[i];
    }
    return result;
}

void MetricsSampler::add_sample(float frame_time,
                                float physics_time,
                                float render_time,
                                float network_latency) noexcept {
    frame_times[current_sample] = frame_time;
    physics_times[current_sample] = physics_time;
    render_times[current_sample] = render_time;
    network_latencies[current_sample] = network_latency;
    current_sample = (current_sample + 1u) & 7u;
    sample_count = std::min<uint32_t>(sample_count + 1u, 8u);
}

float MetricsSampler::average_frame_time() const noexcept {
    if (sample_count == 0u) {
        return 0.0f;
    }
    const __m256 values = _mm256_load_ps(frame_times.data());
    return horizontal_add(values) / static_cast<float>(sample_count);
}

float MetricsSampler::average_physics_time() const noexcept {
    if (sample_count == 0u) {
        return 0.0f;
    }
    const __m256 values = _mm256_load_ps(physics_times.data());
    return horizontal_add(values) / static_cast<float>(sample_count);
}

float MetricsSampler::average_render_time() const noexcept {
    if (sample_count == 0u) {
        return 0.0f;
    }
    const __m256 values = _mm256_load_ps(render_times.data());
    return horizontal_add(values) / static_cast<float>(sample_count);
}

float MetricsSampler::average_network_latency() const noexcept {
    if (sample_count == 0u) {
        return 0.0f;
    }
    const __m256 values = _mm256_load_ps(network_latencies.data());
    return horizontal_add(values) / static_cast<float>(sample_count);
}

void MetricsSampler::reset() noexcept {
    frame_times.fill(0.0f);
    physics_times.fill(0.0f);
    render_times.fill(0.0f);
    network_latencies.fill(0.0f);
    sample_count = 0;
    current_sample = 0;
    collection_interval_ms = 16.0f;
}

PerformanceAnalyzer::PerformanceAnalyzer() {
    thresholds_.push_back({PerformanceMetric::FrameTimeMs, 16.67f, true});
    thresholds_.push_back({PerformanceMetric::FPS, 55.0f, false});
    thresholds_.push_back({PerformanceMetric::PhysicsTimeMs, 4.0f, true});
    thresholds_.push_back({PerformanceMetric::RenderTimeMs, 8.0f, true});
    thresholds_.push_back({PerformanceMetric::NetworkLatencyMs, 50.0f, true});
    known_patterns_.push_back({"Physics spike under load", 0.25f});
    known_patterns_.push_back({"GPU-bound shading burst", 0.30f});
}

PerformanceBottleneck PerformanceAnalyzer::make_bottleneck(BottleneckType type,
                                                           const char* name,
                                                           float severity,
                                                           float impact,
                                                           uint32_t timestamp,
                                                           float duration) const {
    PerformanceBottleneck bottleneck;
    bottleneck.bottleneck_type = static_cast<uint32_t>(type);
    std::strncpy(bottleneck.bottleneck_name, name, sizeof(bottleneck.bottleneck_name) - 1);
    bottleneck.severity_score = severity;
    bottleneck.performance_impact_pct = impact;
    bottleneck.detection_timestamp = timestamp;
    bottleneck.duration_seconds = duration;
    bottleneck.is_resolved = false;
    bottleneck.resolution_time = 0.0f;
    return bottleneck;
}

bool PerformanceAnalyzer::detect_cpu_bound_bottleneck(const PerformanceMetrics& metrics) const {
    const bool high_frame = metrics.frame_time_ms > 16.67f;
    const bool cpu_heavy = metrics.physics_update_time_ms > metrics.render_time_ms * 1.15f ||
                          metrics.physics_update_time_ms > 4.0f;
    return high_frame && cpu_heavy;
}

bool PerformanceAnalyzer::detect_gpu_bound_bottleneck(const PerformanceMetrics& metrics) const {
    const bool high_frame = metrics.frame_time_ms > 16.67f;
    const bool gpu_heavy = metrics.gpu_time_ms > metrics.render_time_ms * 0.85f ||
                          metrics.render_time_ms > 10.0f;
    return high_frame && gpu_heavy;
}

bool PerformanceAnalyzer::detect_memory_bound_bottleneck(const PerformanceMetrics& metrics) const {
    return metrics.memory_fragmentation_pct > 70.0f || metrics.total_memory_used_mb > 8192ull;
}

bool PerformanceAnalyzer::detect_io_bound_bottleneck(const PerformanceMetrics& metrics) const {
    return metrics.texture_memory_mb > 1024u && metrics.vertex_memory_mb > 512u &&
           metrics.render_time_ms > 12.0f;
}

bool PerformanceAnalyzer::detect_network_bound_bottleneck(const PerformanceMetrics& metrics) const {
    return (metrics.network_latency_ms > 100.0f || metrics.network_jitter_ms > 20.0f) &&
           metrics.network_packets_sent + metrics.network_packets_recv > 2000u;
}

bool PerformanceAnalyzer::match_known_patterns(const PerformanceMetrics& metrics) {
    const bool cpu_spike = detect_cpu_bound_bottleneck(metrics) && metrics.physics_bodies_count > 5000u;
    const bool gpu_spike = detect_gpu_bound_bottleneck(metrics) && metrics.draw_calls_count > 1200u;
    if (cpu_spike) {
        return true;
    }
    if (gpu_spike) {
        return true;
    }
    return false;
}

void PerformanceAnalyzer::learn_new_patterns(const PerformanceMetrics& metrics) {
    if (metrics.frame_time_ms > 33.3f && metrics.gpu_time_ms > metrics.physics_update_time_ms) {
        if (known_patterns_.size() < 16) {
            known_patterns_.push_back({"GPU-bound large geometry update", 0.18f});
        }
    }
}

std::vector<Recommendation> PerformanceAnalyzer::generate_recommendations(
    const PerformanceBottleneck& bottleneck) const {
    std::vector<Recommendation> recommendations;
    switch (static_cast<BottleneckType>(bottleneck.bottleneck_type)) {
        case BottleneckType::CPU_BOUND:
            recommendations.push_back({"Reduce physics update frequency or prune inactive bodies."});
            recommendations.push_back({"Profile hot path in AI and collision systems."});
            break;
        case BottleneckType::GPU_BOUND:
            recommendations.push_back({"Lower shadow and post-process quality for large battles."});
            recommendations.push_back({"Batch draw calls and simplify materials."});
            break;
        case BottleneckType::MEMORY_BOUND:
            recommendations.push_back({"Compact buffers and reduce texture residency."});
            recommendations.push_back({"Perform memory pooling and defragmentation."});
            break;
        case BottleneckType::IO_BOUND:
            recommendations.push_back({"Reduce streaming bandwidth and prefetch assets."});
            recommendations.push_back({"Use GPU memory reuse and texture atlases."});
            break;
        case BottleneckType::NETWORK_BOUND:
            recommendations.push_back({"Throttle update rates and coalesce network packets."});
            recommendations.push_back({"Use delta-compression for state synchronization."});
            break;
        default:
            recommendations.push_back({"Maintain current configuration and continue monitoring."});
            break;
    }
    return recommendations;
}

void PerformanceAnalyzer::update_thresholds(const PerformanceMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    historical_metrics_.push_back(metrics);
    if (historical_metrics_.size() > 128u) {
        historical_metrics_.pop_front();
    }
    baseline_metrics_.avg_frame_time_ms = 0.0f;
    baseline_metrics_.avg_fps = 0;
    if (!historical_metrics_.empty()) {
        float sum = 0.0f;
        uint64_t fps_sum = 0;
        for (const auto& snapshot : historical_metrics_) {
            sum += snapshot.frame_time_ms;
            fps_sum += snapshot.fps;
        }
        baseline_metrics_.avg_frame_time_ms = sum / static_cast<float>(historical_metrics_.size());
        baseline_metrics_.avg_fps = static_cast<uint32_t>(fps_sum / historical_metrics_.size());
    }
}

bool PerformanceAnalyzer::is_threshold_crossed(const PerformanceMetrics& metrics,
                                              const PerformanceThreshold& threshold) const {
    switch (threshold.metric) {
        case PerformanceMetric::FrameTimeMs:
            return threshold.above_is_bad ? metrics.frame_time_ms > threshold.limit
                                           : metrics.frame_time_ms < threshold.limit;
        case PerformanceMetric::FPS:
            return threshold.above_is_bad ? metrics.fps > threshold.limit
                                           : metrics.fps < threshold.limit;
        case PerformanceMetric::PhysicsTimeMs:
            return threshold.above_is_bad ? metrics.physics_update_time_ms > threshold.limit
                                           : metrics.physics_update_time_ms < threshold.limit;
        case PerformanceMetric::RenderTimeMs:
            return threshold.above_is_bad ? metrics.render_time_ms > threshold.limit
                                           : metrics.render_time_ms < threshold.limit;
        case PerformanceMetric::NetworkLatencyMs:
            return threshold.above_is_bad ? metrics.network_latency_ms > threshold.limit
                                           : metrics.network_latency_ms < threshold.limit;
        default:
            return false;
    }
}

void PerformanceAnalyzer::analyze_performance(const PerformanceMetrics& current_metrics,
                                             std::vector<PerformanceBottleneck>& detected_bottlenecks) {
    detected_bottlenecks.clear();
    update_thresholds(current_metrics);
    if (detect_cpu_bound_bottleneck(current_metrics)) {
        auto bottleneck = make_bottleneck(BottleneckType::CPU_BOUND,
                                          "CPU bound",
                                          0.85f,
                                          0.45f,
                                          static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::high_resolution_clock::now().time_since_epoch()).count()),
                                          0.0f);
        bottleneck.recommendations = generate_recommendations(bottleneck);
        detected_bottlenecks.push_back(bottleneck);
    }
    if (detect_gpu_bound_bottleneck(current_metrics)) {
        auto bottleneck = make_bottleneck(BottleneckType::GPU_BOUND,
                                          "GPU bound",
                                          0.78f,
                                          0.40f,
                                          static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::high_resolution_clock::now().time_since_epoch()).count()),
                                          0.0f);
        bottleneck.recommendations = generate_recommendations(bottleneck);
        detected_bottlenecks.push_back(bottleneck);
    }
    if (detect_memory_bound_bottleneck(current_metrics)) {
        auto bottleneck = make_bottleneck(BottleneckType::MEMORY_BOUND,
                                          "Memory bound",
                                          0.60f,
                                          0.25f,
                                          static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::high_resolution_clock::now().time_since_epoch()).count()),
                                          0.0f);
        bottleneck.recommendations = generate_recommendations(bottleneck);
        detected_bottlenecks.push_back(bottleneck);
    }
    if (detect_network_bound_bottleneck(current_metrics)) {
        auto bottleneck = make_bottleneck(BottleneckType::NETWORK_BOUND,
                                          "Network bound",
                                          0.70f,
                                          0.30f,
                                          static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::high_resolution_clock::now().time_since_epoch()).count()),
                                          0.0f);
        bottleneck.recommendations = generate_recommendations(bottleneck);
        detected_bottlenecks.push_back(bottleneck);
    }
    learn_new_patterns(current_metrics);
}

PerformanceReporter::PerformanceReporter() = default;

void PerformanceReporter::generate_performance_report(const PerformanceMetrics& metrics,
                                                      const std::vector<PerformanceBottleneck>& bottlenecks,
                                                      PerformanceReport& report) const {
    std::ostringstream details;
    details << "Performance Summary:\n";
    details << "  Frame: " << metrics.frame_time_ms << " ms (avg " << metrics.avg_frame_time_ms << " ms)\n";
    details << "  FPS: " << metrics.fps << " (avg " << metrics.avg_fps << ")\n";
    details << "  Physics: " << metrics.physics_update_time_ms << " ms, bodies " << metrics.physics_bodies_count << "\n";
    details << "  Render: " << metrics.render_time_ms << " ms, draw calls " << metrics.draw_calls_count << "\n";
    details << "  Network: " << metrics.network_latency_ms << " ms latency, " << metrics.network_packets_sent << " sent\n";
    details << "  AI: " << metrics.ai_decision_time_ms << " ms, " << metrics.pathfinding_queries << " queries\n";

    if (!bottlenecks.empty()) {
        details << "Detected bottlenecks:\n";
        for (const auto& bottleneck : bottlenecks) {
            details << "  - " << bottleneck.bottleneck_name << " (severity=" << bottleneck.severity_score << ", impact=" << bottleneck.performance_impact_pct << ")\n";
            for (const auto& recommendation : bottleneck.recommendations) {
                details << "      * " << recommendation.text << "\n";
            }
        }
    } else {
        details << "No active bottlenecks detected.\n";
    }

    report.summary = "Realtime performance report generated.";
    report.details = details.str();
    report.format = "text";
}

std::string PerformanceReporter::export_json(const PerformanceMetrics& metrics,
                                             const std::vector<PerformanceBottleneck>& bottlenecks) const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"frame_time_ms\": " << metrics.frame_time_ms << ",\n";
    json << "  \"fps\": " << metrics.fps << ",\n";
    json << "  \"physics_update_time_ms\": " << metrics.physics_update_time_ms << ",\n";
    json << "  \"render_time_ms\": " << metrics.render_time_ms << ",\n";
    json << "  \"network_latency_ms\": " << metrics.network_latency_ms << ",\n";
    json << "  \"ai_decision_time_ms\": " << metrics.ai_decision_time_ms << ",\n";
    json << "  \"bottlenecks\": [\n";
    for (size_t i = 0; i < bottlenecks.size(); ++i) {
        const auto& bottleneck = bottlenecks[i];
        json << "    {\n";
        json << "      \"name\": \"" << bottleneck.bottleneck_name << "\",\n";
        json << "      \"severity_score\": " << bottleneck.severity_score << ",\n";
        json << "      \"performance_impact_pct\": " << bottleneck.performance_impact_pct << "\n";
        json << "    }" << (i + 1 < bottlenecks.size() ? ",\n" : "\n");
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::string PerformanceReporter::export_csv(const PerformanceMetrics& metrics,
                                             const std::vector<PerformanceBottleneck>& bottlenecks) const {
    std::ostringstream csv;
    csv << "metric,value\n";
    csv << "frame_time_ms," << metrics.frame_time_ms << "\n";
    csv << "fps," << metrics.fps << "\n";
    csv << "physics_update_time_ms," << metrics.physics_update_time_ms << "\n";
    csv << "render_time_ms," << metrics.render_time_ms << "\n";
    csv << "network_latency_ms," << metrics.network_latency_ms << "\n";
    csv << "ai_decision_time_ms," << metrics.ai_decision_time_ms << "\n";
    for (const auto& bottleneck : bottlenecks) {
        csv << "bottleneck," << bottleneck.bottleneck_name << "\n";
    }
    return csv.str();
}

std::string PerformanceReporter::export_html(const PerformanceMetrics& metrics,
                                              const std::vector<PerformanceBottleneck>& bottlenecks) const {
    std::ostringstream html;
    html << "<html><head><title>Performance Report</title></head><body>\n";
    html << "<h1>Performance Report</h1>\n";
    html << "<ul>\n";
    html << "<li>Frame Time: " << metrics.frame_time_ms << " ms</li>\n";
    html << "<li>FPS: " << metrics.fps << "</li>\n";
    html << "<li>Physics: " << metrics.physics_update_time_ms << " ms</li>\n";
    html << "<li>Render: " << metrics.render_time_ms << " ms</li>\n";
    html << "<li>Network Latency: " << metrics.network_latency_ms << " ms</li>\n";
    html << "<li>AI Decision Time: " << metrics.ai_decision_time_ms << " ms</li>\n";
    html << "</ul>\n";
    if (!bottlenecks.empty()) {
        html << "<h2>Bottlenecks</h2><ul>\n";
        for (const auto& bottleneck : bottlenecks) {
            html << "<li>" << bottleneck.bottleneck_name << " (severity=" << bottleneck.severity_score << ", impact=" << bottleneck.performance_impact_pct << ")</li>\n";
        }
        html << "</ul>\n";
    }
    html << "</body></html>\n";
    return html.str();
}

void PerformanceReporter::generate_historical_analysis(const std::vector<PerformanceMetrics>& historical_data,
                                                       PerformanceReport& report) const {
    if (historical_data.empty()) {
        report.summary = "No historical data available.";
        report.details = "";
        report.format = "text";
        return;
    }
    float average_frame = 0.0f;
    float max_frame = 0.0f;
    for (const auto& snapshot : historical_data) {
        average_frame += snapshot.frame_time_ms;
        max_frame = std::max(max_frame, snapshot.frame_time_ms);
    }
    average_frame /= static_cast<float>(historical_data.size());
    std::ostringstream details;
    details << "Historical performance over " << historical_data.size() << " samples:\n";
    details << "  Average frame time: " << average_frame << " ms\n";
    details << "  Peak frame time: " << max_frame << " ms\n";
    report.summary = "Historical trend analysis generated.";
    report.details = details.str();
    report.format = "text";
}

void FunctionProfile::update(float duration_ms, float exclusive_ms, uint32_t current_depth) noexcept {
    ++call_count;
    total_time_ms += duration_ms;
    exclusive_time_ms += exclusive_ms;
    inclusive_time_ms += duration_ms;
    recursion_depth = std::max(recursion_depth, current_depth);
    if (min_time_ms <= 0.0f || duration_ms < min_time_ms) {
        min_time_ms = duration_ms;
    }
    max_time_ms = std::max(max_time_ms, duration_ms);
    average_time_ms = total_time_ms / static_cast<float>(call_count);
}

std::atomic<GlobalProfiler*> GlobalProfiler::active_profiler_{nullptr};
thread_local std::vector<GlobalProfiler::CallFrame> GlobalProfiler::call_stack_;

GlobalProfiler::GlobalProfiler() {
    active_profiler_.store(this, std::memory_order_release);
}

GlobalProfiler::~GlobalProfiler() {
    active_profiler_.store(nullptr, std::memory_order_release);
}

uint64_t GlobalProfiler::now_ns() noexcept {
    return static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

float GlobalProfiler::elapsed_ms(const std::chrono::high_resolution_clock::time_point& start) noexcept {
    return static_cast<float>(std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - start).count());
}

void GlobalProfiler::start_profiling_session() {
    profile_start_time_ns_ = now_ns();
    is_enabled_.store(true, std::memory_order_release);
    is_recording_.store(true, std::memory_order_release);
    active_profiler_.store(this, std::memory_order_release);
}

void GlobalProfiler::stop_profiling_session() {
    is_recording_.store(false, std::memory_order_release);
    is_enabled_.store(false, std::memory_order_release);
    active_profiler_.store(nullptr, std::memory_order_release);
}

void GlobalProfiler::pause_profiling() {
    is_recording_.store(false, std::memory_order_release);
}

void GlobalProfiler::resume_profiling() {
    if (is_enabled_.load(std::memory_order_acquire)) {
        is_recording_.store(true, std::memory_order_release);
    }
}

void GlobalProfiler::reset_profiling_data() {
    std::lock_guard<std::mutex> lock(mutex_);
    function_profiles_.clear();
    sorted_profiles_.clear();
}

std::vector<FunctionProfile> GlobalProfiler::get_top_performance_hogs(uint32_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    sorted_profiles_.clear();
    sorted_profiles_.reserve(function_profiles_.size());
    for (auto const& entry : function_profiles_) {
        sorted_profiles_.push_back(entry.second);
    }
    std::sort(sorted_profiles_.begin(), sorted_profiles_.end(), [](const FunctionProfile& a, const FunctionProfile& b) {
        return a.total_time_ms > b.total_time_ms;
    });
    if (sorted_profiles_.size() > count) {
        sorted_profiles_.resize(count);
    }
    return sorted_profiles_;
}

float GlobalProfiler::get_total_profile_time() const {
    if (profile_start_time_ns_ == 0ull) {
        return 0.0f;
    }
    return static_cast<float>(now_ns() - profile_start_time_ns_) * 1e-6f;
}

float GlobalProfiler::get_average_frame_time() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (function_profiles_.empty()) {
        return 0.0f;
    }
    float total = 0.0f;
    uint64_t count = 0;
    for (auto const& entry : function_profiles_) {
        total += entry.second.total_time_ms;
        count += entry.second.call_count;
    }
    return count == 0 ? 0.0f : total / static_cast<float>(count);
}

void GlobalProfiler::register_function(uint32_t function_id,
                                       const char* name,
                                       const char* source_file,
                                       uint32_t line_number) {
    if (!active_profiler_.load(std::memory_order_acquire)) {
        return;
    }
    auto profiler = active_profiler_.load(std::memory_order_acquire);
    std::lock_guard<std::mutex> lock(profiler->mutex_);
    auto& profile = profiler->function_profiles_[function_id];
    profile.function_id = function_id;
    std::strncpy(profile.function_name, name, sizeof(profile.function_name) - 1);
    std::strncpy(profile.source_file, source_file, sizeof(profile.source_file) - 1);
    profile.line_number = line_number;
}

void GlobalProfiler::start_profiling(uint32_t function_id) {
    auto profiler = active_profiler_.load(std::memory_order_acquire);
    if (!profiler || !profiler->is_recording_.load(std::memory_order_acquire)) {
        return;
    }
    call_stack_.push_back({function_id, std::chrono::high_resolution_clock::now(), 0.0f});
}

void GlobalProfiler::stop_profiling(uint32_t function_id) {
    auto profiler = active_profiler_.load(std::memory_order_acquire);
    if (!profiler || !profiler->is_recording_.load(std::memory_order_acquire)) {
        return;
    }
    if (call_stack_.empty()) {
        return;
    }
    auto frame = call_stack_.back();
    call_stack_.pop_back();
    if (frame.function_id != function_id) {
        return;
    }
    const float duration_ms = elapsed_ms(frame.start_time);
    const float exclusive_ms = std::max(0.0f, duration_ms - frame.child_duration_ms);
    const uint32_t current_depth = static_cast<uint32_t>(call_stack_.size() + 1u);
    profiler->update_profile_data(function_id, duration_ms, exclusive_ms, current_depth);
    if (!call_stack_.empty()) {
        call_stack_.back().child_duration_ms += duration_ms;
    }
}

void GlobalProfiler::update_profile_data(uint32_t function_id,
                                         float duration_ms,
                                         float exclusive_ms,
                                         uint32_t recursion_depth) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& profile = function_profiles_[function_id];
    profile.function_id = function_id;
    profile.update(duration_ms, exclusive_ms, recursion_depth);
}

ProfileScopeGuard::ProfileScopeGuard(uint32_t func_id, float& total_time_ref)
    : function_id_(func_id),
      start_time_(std::chrono::high_resolution_clock::now()),
      total_time_ptr_(&total_time_ref) {
    GlobalProfiler::start_profiling(func_id);
}

ProfileScopeGuard::~ProfileScopeGuard() {
    const float elapsed = GlobalProfiler::elapsed_ms(start_time_);
    if (total_time_ptr_) {
        *total_time_ptr_ += elapsed;
    }
    GlobalProfiler::stop_profiling(function_id_);
}

void ProfileScopeGuard::start_profiling(uint32_t function_id) {
    GlobalProfiler::start_profiling(function_id);
}

void ProfileScopeGuard::stop_profiling(uint32_t function_id) {
    GlobalProfiler::stop_profiling(function_id);
}

void ProfileScopeGuard::register_function(const char* name,
                                          const char* file,
                                          uint32_t line) {
    uint32_t function_id = std::hash<std::string_view>{}(std::string_view(name));
    GlobalProfiler::register_function(function_id, name, file, line);
}

PerformanceMonitor::PerformanceMonitor() {
    sampler_.collection_interval_ms = 16.0f;
}

void PerformanceMonitor::collect_metrics(PerformanceMetrics& metrics) {
    metrics.avg_frame_time_ms = sampler_.average_frame_time();
    metrics.physics_update_time_ms = sampler_.average_physics_time();
    metrics.render_time_ms = sampler_.average_render_time();
    metrics.network_latency_ms = sampler_.average_network_latency();
    if (metrics.avg_frame_time_ms > 0.0f) {
        metrics.avg_fps = static_cast<uint32_t>(1000.0f / metrics.avg_frame_time_ms);
    }
    sampler_.reset();
    history_.push_back(metrics);
    if (history_.size() > max_history_) {
        history_.pop_front();
    }
}

void PerformanceMonitor::monitor_physics_performance(const PerformanceMetrics& physics_snapshot,
                                                     PerformanceMetrics& metrics) {
    metrics.physics_bodies_count = physics_snapshot.physics_bodies_count;
    metrics.physics_update_time_ms = physics_snapshot.physics_update_time_ms;
    metrics.collision_checks_count = physics_snapshot.collision_checks_count;
    metrics.broad_phase_time_ms = physics_snapshot.broad_phase_time_ms;
    metrics.narrow_phase_time_ms = physics_snapshot.narrow_phase_time_ms;
    metrics.integration_time_ms = physics_snapshot.integration_time_ms;
}

void PerformanceMonitor::monitor_rendering_performance(const PerformanceMetrics& rendering_snapshot,
                                                       PerformanceMetrics& metrics) {
    metrics.draw_calls_count = rendering_snapshot.draw_calls_count;
    metrics.triangles_rendered = rendering_snapshot.triangles_rendered;
    metrics.render_time_ms = rendering_snapshot.render_time_ms;
    metrics.gpu_time_ms = rendering_snapshot.gpu_time_ms;
    metrics.vsync_wait_time_ms = rendering_snapshot.vsync_wait_time_ms;
    metrics.texture_memory_mb = rendering_snapshot.texture_memory_mb;
    metrics.vertex_memory_mb = rendering_snapshot.vertex_memory_mb;
}

void PerformanceMonitor::monitor_network_performance(const PerformanceMetrics& network_snapshot,
                                                     PerformanceMetrics& metrics) {
    metrics.network_packets_sent = network_snapshot.network_packets_sent;
    metrics.network_packets_recv = network_snapshot.network_packets_recv;
    metrics.network_latency_ms = network_snapshot.network_latency_ms;
    metrics.network_jitter_ms = network_snapshot.network_jitter_ms;
    metrics.bandwidth_upload_kbps = network_snapshot.bandwidth_upload_kbps;
    metrics.bandwidth_download_kbps = network_snapshot.bandwidth_download_kbps;
}

void PerformanceMonitor::monitor_ai_performance(const PerformanceMetrics& ai_snapshot,
                                                PerformanceMetrics& metrics) {
    metrics.ai_units_count = ai_snapshot.ai_units_count;
    metrics.ai_decision_time_ms = ai_snapshot.ai_decision_time_ms;
    metrics.pathfinding_queries = ai_snapshot.pathfinding_queries;
    metrics.pathfinding_avg_time_ms = ai_snapshot.pathfinding_avg_time_ms;
    metrics.behavior_tree_ticks = ai_snapshot.behavior_tree_ticks;
}

void PerformanceMonitor::monitor_large_battle_performance(uint32_t expected_units,
                                                          uint32_t expected_entities,
                                                          uint32_t active_entities,
                                                          PerformanceMetrics& metrics) {
    metrics.ai_units_count = expected_units;
    metrics.physics_bodies_count = expected_entities;
    if (active_entities > 0) {
        metrics.frame_time_ms += static_cast<float>(active_entities) * 0.0005f;
    }
}

void PerformanceMonitor::set_collection_interval(float interval_ms) noexcept {
    sampler_.collection_interval_ms = interval_ms;
}

const std::deque<PerformanceMetrics>& PerformanceMonitor::history() const noexcept {
    return history_;
}

} // namespace performance
