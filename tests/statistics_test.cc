#include "src/analysis/statistics.h"
#include "gtest/gtest.h"

#include <cmath>

namespace rtdiag {
namespace analysis {
namespace {

TEST(RunningStatsTest, Empty) {
    RunningStats stats;
    EXPECT_EQ(stats.Count(), 0u);
    EXPECT_DOUBLE_EQ(stats.Mean(), 0.0);
    EXPECT_DOUBLE_EQ(stats.Min(), 0.0);
    EXPECT_DOUBLE_EQ(stats.Max(), 0.0);
}

TEST(RunningStatsTest, SingleValue) {
    RunningStats stats;
    stats.Add(42.0);
    EXPECT_EQ(stats.Count(), 1u);
    EXPECT_DOUBLE_EQ(stats.Mean(), 42.0);
    EXPECT_DOUBLE_EQ(stats.Min(), 42.0);
    EXPECT_DOUBLE_EQ(stats.Max(), 42.0);
    EXPECT_DOUBLE_EQ(stats.Variance(), 0.0);
}

TEST(RunningStatsTest, KnownDistribution) {
    RunningStats stats;
    // Values 1 through 10
    for (int i = 1; i <= 10; ++i) {
        stats.Add(static_cast<double>(i));
    }
    EXPECT_EQ(stats.Count(), 10u);
    EXPECT_DOUBLE_EQ(stats.Min(), 1.0);
    EXPECT_DOUBLE_EQ(stats.Max(), 10.0);
    EXPECT_DOUBLE_EQ(stats.Mean(), 5.5);

    // Population variance of 1..10 = 8.25
    EXPECT_NEAR(stats.Variance(), 8.25, 0.01);
    // Sample variance = 9.1667
    EXPECT_NEAR(stats.SampleVariance(), 9.1667, 0.01);
}

TEST(RunningStatsTest, StdDev) {
    RunningStats stats;
    for (int i = 1; i <= 100; ++i) {
        stats.Add(static_cast<double>(i));
    }
    EXPECT_GT(stats.StdDev(), 0.0);
    EXPECT_NEAR(stats.StdDev(), 28.866, 0.01);
}

TEST(RunningStatsTest, Merge) {
    RunningStats a, b;
    for (int i = 1; i <= 50; ++i) a.Add(static_cast<double>(i));
    for (int i = 51; i <= 100; ++i) b.Add(static_cast<double>(i));

    a.Merge(b);
    EXPECT_EQ(a.Count(), 100u);
    EXPECT_DOUBLE_EQ(a.Min(), 1.0);
    EXPECT_DOUBLE_EQ(a.Max(), 100.0);
    EXPECT_NEAR(a.Mean(), 50.5, 0.01);
}

TEST(RunningStatsTest, MergeEmpty) {
    RunningStats a, empty;
    a.Add(5.0);
    a.Merge(empty);
    EXPECT_EQ(a.Count(), 1u);
    EXPECT_DOUBLE_EQ(a.Mean(), 5.0);

    RunningStats empty2, b;
    b.Add(10.0);
    empty2.Merge(b);
    EXPECT_EQ(empty2.Count(), 1u);
    EXPECT_DOUBLE_EQ(empty2.Mean(), 10.0);
}

TEST(RunningStatsTest, Reset) {
    RunningStats stats;
    stats.Add(1.0);
    stats.Add(2.0);
    stats.Reset();
    EXPECT_EQ(stats.Count(), 0u);
    EXPECT_DOUBLE_EQ(stats.Mean(), 0.0);
}

}  // namespace
}  // namespace analysis
}  // namespace rtdiag
