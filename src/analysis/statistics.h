#pragma once

/// @file statistics.h
/// @brief Running statistics accumulator using Welford's online algorithm.

#include <cmath>
#include <cstdint>
#include <limits>

namespace rtdiag {
namespace analysis {

/// Online statistics accumulator (mean, variance, min, max).
/// Uses Welford's algorithm for numerically stable variance computation.
class RunningStats {
public:
    RunningStats() { Reset(); }

    /// Add a sample.
    void Add(double value) {
        ++count_;
        if (value < min_) min_ = value;
        if (value > max_) max_ = value;

        // Welford's online algorithm
        double delta = value - mean_;
        mean_ += delta / static_cast<double>(count_);
        double delta2 = value - mean_;
        m2_ += delta * delta2;
    }

    /// Number of samples.
    uint64_t Count() const { return count_; }

    /// Minimum value.
    double Min() const { return count_ > 0 ? min_ : 0.0; }

    /// Maximum value.
    double Max() const { return count_ > 0 ? max_ : 0.0; }

    /// Mean value.
    double Mean() const { return count_ > 0 ? mean_ : 0.0; }

    /// Population variance.
    double Variance() const {
        return count_ > 1 ? m2_ / static_cast<double>(count_) : 0.0;
    }

    /// Sample variance (Bessel's correction).
    double SampleVariance() const {
        return count_ > 1 ? m2_ / static_cast<double>(count_ - 1) : 0.0;
    }

    /// Standard deviation (population).
    double StdDev() const { return std::sqrt(Variance()); }

    /// Sample standard deviation.
    double SampleStdDev() const { return std::sqrt(SampleVariance()); }

    /// Merge another RunningStats into this one.
    void Merge(const RunningStats& other) {
        if (other.count_ == 0) return;
        if (count_ == 0) {
            *this = other;
            return;
        }

        uint64_t new_count = count_ + other.count_;
        double delta = other.mean_ - mean_;
        double new_mean = mean_ + delta * static_cast<double>(other.count_) /
                                          static_cast<double>(new_count);
        double new_m2 = m2_ + other.m2_ +
                        delta * delta *
                        static_cast<double>(count_) *
                        static_cast<double>(other.count_) /
                        static_cast<double>(new_count);

        count_ = new_count;
        mean_ = new_mean;
        m2_ = new_m2;
        min_ = std::min(min_, other.min_);
        max_ = std::max(max_, other.max_);
    }

    /// Reset all statistics.
    void Reset() {
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
        min_ = std::numeric_limits<double>::max();
        max_ = std::numeric_limits<double>::lowest();
    }

private:
    uint64_t count_;
    double   mean_;
    double   m2_;     ///< Sum of squared differences from the mean
    double   min_;
    double   max_;
};

}  // namespace analysis
}  // namespace rtdiag
