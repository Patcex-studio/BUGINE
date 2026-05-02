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

#include <gtest/gtest.h>
#include <thread>

using namespace performance;

TEST(PerformanceMonitorTest, MetricsSamplerAverages) {
    MetricsSampler sampler;
    sampler.add_sample(16.0f, 2.0f, 8.0f, 20.0f);
    sampler.add_sample(17.0f, 3.0f, 9.0f, 25.0f);
    EXPECT_NEAR(sampler.average_frame_time(), 16.5f, 1e-3f);
    EXPECT_NEAR(sampler.average_physics_time(), 2.5f, 1e-3f);
    EXPECT_NEAR(sampler.average_render_time(), 8.5f, 1e-3f);
    EXPECT_NEAR(sampler.average_network_latency(), 22.5f, 1e-3f);
}

TEST(PerformanceAnalyzerTest, DetectCpuBottleneck) {
    PerformanceAnalyzer analyzer;
    PerformanceMetrics metrics;
    metrics.frame_time_ms = 22.0f;
    metrics.physics_update_time_ms = 5.0f;
    metrics.render_time_ms = 5.0f;

    std::vector<PerformanceBottleneck> bottlenecks;
    analyzer.analyze_performance(metrics, bottlenecks);

    ASSERT_FALSE(bottlenecks.empty());
    EXPECT_EQ(bottlenecks.front().bottleneck_type, static_cast<uint32_t>(BottleneckType::CPU_BOUND));
}

TEST(PerformanceReporterTest, ExportFormatsContainKeyValues) {
    PerformanceReporter reporter;
    PerformanceMetrics metrics;
    metrics.frame_time_ms = 15.0f;
    metrics.fps = 66;
    std::vector<PerformanceBottleneck> bottlenecks;
    PerformanceReport report;

    reporter.generate_performance_report(metrics, bottlenecks, report);
    EXPECT_TRUE(report.details.find("Frame: 15") != std::string::npos);

    auto json = reporter.export_json(metrics, bottlenecks);
    EXPECT_TRUE(json.find("\"frame_time_ms\"") != std::string::npos);

    auto csv = reporter.export_csv(metrics, bottlenecks);
    EXPECT_TRUE(csv.find("frame_time_ms") != std::string::npos);

    auto html = reporter.export_html(metrics, bottlenecks);
    EXPECT_TRUE(html.find("<html>") != std::string::npos);
}

TEST(GlobalProfilerTest, ProfileScopeGuardRecordsData) {
    GlobalProfiler profiler;
    profiler.start_profiling_session();
    GlobalProfiler::register_function(1001, "test_function", "test_performance_monitoring.cpp", 10);
    float elapsed_time = 0.0f;
    {
        ProfileScopeGuard guard(1001, elapsed_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    profiler.stop_profiling_session();

    auto top = profiler.get_top_performance_hogs(1);
    ASSERT_FALSE(top.empty());
    EXPECT_EQ(top.front().function_id, 1001u);
    EXPECT_GT(top.front().call_count, 0u);
    EXPECT_GT(elapsed_time, 0.0f);
}
