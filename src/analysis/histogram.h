#pragma once

/// @file histogram.h
/// @brief Configurable histogram with ASCII rendering for latency distributions.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rtdiag {
namespace analysis {

/// A histogram with configurable linear or logarithmic buckets.
class Histogram {
public:
    /// Create a linear histogram.
    /// @param num_buckets  Number of buckets
    /// @param min_val      Minimum value (inclusive)
    /// @param max_val      Maximum value (exclusive for all but last bucket)
    Histogram(size_t num_buckets, double min_val, double max_val);

    /// Create a logarithmic histogram (base-2 buckets).
    /// @param min_exp  Minimum exponent (e.g., 0 = 1ns)
    /// @param max_exp  Maximum exponent (e.g., 20 = ~1ms)
    static Histogram Logarithmic(int min_exp, int max_exp);

    /// Add a value to the histogram.
    void Add(double value);

    /// Add multiple identical values.
    void Add(double value, size_t count);

    /// Merge another histogram into this one (must have same bucket config).
    void Merge(const Histogram& other);

    /// Get the count for a specific bucket.
    uint64_t BucketCount(size_t bucket) const;

    /// Get the lower bound of a bucket.
    double BucketLower(size_t bucket) const;

    /// Get the upper bound of a bucket.
    double BucketUpper(size_t bucket) const;

    /// Total number of samples.
    uint64_t TotalCount() const;

    /// Number of buckets.
    size_t NumBuckets() const { return counts_.size(); }

    /// Render as ASCII art.
    /// @param width  Maximum bar width in characters
    std::string Render(size_t width = 60) const;

    /// Render with a title.
    std::string Render(const std::string& title, size_t width = 60) const;

    /// Reset all counts.
    void Reset();

private:
    size_t FindBucket(double value) const;

    std::vector<uint64_t> counts_;
    std::vector<double>   boundaries_;  ///< num_buckets + 1 boundaries
    uint64_t total_count_;
    bool is_log_;
};

}  // namespace analysis
}  // namespace rtdiag
