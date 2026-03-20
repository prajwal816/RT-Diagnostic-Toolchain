#include "rtdiag/analyzer.h"
#include "gtest/gtest.h"

#include <cmath>

namespace rtdiag {
namespace {

TEST(LatencyAnalyzerTest, EmptyAnalyzer) {
    LatencyAnalyzer analyzer;
    EXPECT_EQ(analyzer.Count(), 0u);

    auto stats = analyzer.ComputeStats();
    EXPECT_EQ(stats.count, 0u);
}

TEST(LatencyAnalyzerTest, SingleSample) {
    LatencyAnalyzer analyzer;
    analyzer.AddSample(5000);

    EXPECT_EQ(analyzer.Count(), 1u);
    auto stats = analyzer.ComputeStats();
    EXPECT_EQ(stats.count, 1u);
    EXPECT_DOUBLE_EQ(stats.min_ns, 5000.0);
    EXPECT_DOUBLE_EQ(stats.max_ns, 5000.0);
    EXPECT_DOUBLE_EQ(stats.mean_ns, 5000.0);
}

TEST(LatencyAnalyzerTest, MultiplesSamples) {
    LatencyAnalyzer analyzer;
    // Add known values
    for (uint64_t i = 1; i <= 100; ++i) {
        analyzer.AddSample(i * 100);
    }

    EXPECT_EQ(analyzer.Count(), 100u);
    auto stats = analyzer.ComputeStats();
    EXPECT_DOUBLE_EQ(stats.min_ns, 100.0);
    EXPECT_DOUBLE_EQ(stats.max_ns, 10000.0);
    EXPECT_NEAR(stats.mean_ns, 5050.0, 1.0);
    EXPECT_GT(stats.stddev_ns, 0.0);
}

TEST(LatencyAnalyzerTest, Percentiles) {
    LatencyAnalyzer analyzer;
    for (uint64_t i = 1; i <= 1000; ++i) {
        analyzer.AddSample(i);
    }

    auto stats = analyzer.ComputeStats();
    EXPECT_NEAR(stats.p50_ns, 500.0, 10.0);
    EXPECT_NEAR(stats.p90_ns, 900.0, 10.0);
    EXPECT_NEAR(stats.p99_ns, 990.0, 10.0);
}

TEST(LatencyAnalyzerTest, RenderHistogram) {
    LatencyAnalyzer analyzer;
    for (uint64_t i = 0; i < 100; ++i) {
        analyzer.AddSample(1000 + i * 10);
    }

    std::string hist = analyzer.RenderHistogram(10, 40);
    EXPECT_FALSE(hist.empty());
    EXPECT_NE(hist.find("Latency"), std::string::npos);
}

TEST(LatencyAnalyzerTest, RenderReport) {
    LatencyAnalyzer analyzer;
    for (uint64_t i = 0; i < 50; ++i) {
        analyzer.AddSample(2000 + i * 100);
    }

    std::string report = analyzer.RenderReport();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Min"), std::string::npos);
    EXPECT_NE(report.find("P99"), std::string::npos);
}

TEST(LatencyAnalyzerTest, AddResult) {
    LatencyAnalyzer analyzer;
    LatencyResult result;
    result.latency_ns = 3000;
    result.timestamp_ns = 1000000;
    result.pid = 42;
    result.cpu = 1;

    analyzer.AddResult(result);
    EXPECT_EQ(analyzer.Count(), 1u);
    EXPECT_EQ(analyzer.Samples()[0], 3000u);
}

TEST(LatencyAnalyzerTest, Reset) {
    LatencyAnalyzer analyzer;
    analyzer.AddSample(100);
    analyzer.AddSample(200);
    EXPECT_EQ(analyzer.Count(), 2u);

    analyzer.Reset();
    EXPECT_EQ(analyzer.Count(), 0u);
}

}  // namespace
}  // namespace rtdiag
