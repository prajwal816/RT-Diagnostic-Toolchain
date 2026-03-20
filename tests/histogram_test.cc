#include "src/analysis/histogram.h"
#include "gtest/gtest.h"

namespace rtdiag {
namespace analysis {
namespace {

TEST(HistogramTest, BasicLinear) {
    Histogram hist(10, 0.0, 100.0);
    EXPECT_EQ(hist.NumBuckets(), 10u);
    EXPECT_EQ(hist.TotalCount(), 0u);
}

TEST(HistogramTest, AddValues) {
    Histogram hist(10, 0.0, 100.0);
    hist.Add(15.0);
    hist.Add(25.0);
    hist.Add(15.0);

    EXPECT_EQ(hist.TotalCount(), 3u);
    EXPECT_EQ(hist.BucketCount(1), 2u);  // 10-20 bucket
    EXPECT_EQ(hist.BucketCount(2), 1u);  // 20-30 bucket
}

TEST(HistogramTest, BoundaryValues) {
    Histogram hist(5, 0.0, 50.0);
    hist.Add(0.0);   // First bucket
    hist.Add(49.9);  // Last bucket
    hist.Add(100.0); // Beyond max -> last bucket

    EXPECT_EQ(hist.BucketCount(0), 1u);
    EXPECT_EQ(hist.BucketCount(4), 2u);
}

TEST(HistogramTest, BucketBounds) {
    Histogram hist(4, 0.0, 100.0);
    EXPECT_DOUBLE_EQ(hist.BucketLower(0), 0.0);
    EXPECT_DOUBLE_EQ(hist.BucketUpper(0), 25.0);
    EXPECT_DOUBLE_EQ(hist.BucketLower(3), 75.0);
    EXPECT_DOUBLE_EQ(hist.BucketUpper(3), 100.0);
}

TEST(HistogramTest, Logarithmic) {
    auto hist = Histogram::Logarithmic(0, 10);
    EXPECT_EQ(hist.NumBuckets(), 10u);

    hist.Add(1.0);    // 2^0 - 2^1 bucket
    hist.Add(512.0);  // 2^9 - 2^10 bucket

    EXPECT_EQ(hist.TotalCount(), 2u);
}

TEST(HistogramTest, Merge) {
    Histogram a(5, 0.0, 50.0);
    Histogram b(5, 0.0, 50.0);

    a.Add(5.0);
    a.Add(15.0);
    b.Add(5.0);
    b.Add(25.0);

    a.Merge(b);
    EXPECT_EQ(a.TotalCount(), 4u);
    EXPECT_EQ(a.BucketCount(0), 2u);  // 0-10: both 5.0 values
}

TEST(HistogramTest, Reset) {
    Histogram hist(5, 0.0, 50.0);
    hist.Add(10.0);
    hist.Add(20.0);
    EXPECT_EQ(hist.TotalCount(), 2u);

    hist.Reset();
    EXPECT_EQ(hist.TotalCount(), 0u);
}

TEST(HistogramTest, Render) {
    Histogram hist(5, 0.0, 100.0);
    for (int i = 0; i < 100; ++i) {
        hist.Add(static_cast<double>(i));
    }

    std::string rendered = hist.Render("Test Histogram", 40);
    EXPECT_FALSE(rendered.empty());
    EXPECT_NE(rendered.find("Test Histogram"), std::string::npos);
    EXPECT_NE(rendered.find("#"), std::string::npos);
}

TEST(HistogramTest, AddBatch) {
    Histogram hist(5, 0.0, 50.0);
    hist.Add(25.0, 100);
    EXPECT_EQ(hist.TotalCount(), 100u);
    EXPECT_EQ(hist.BucketCount(2), 100u);  // 20-30 bucket
}

}  // namespace
}  // namespace analysis
}  // namespace rtdiag
