#pragma once

/// @file analyzer.h
/// @brief Public API for latency analysis and histogram computation.

#include "rtdiag/types.h"

#include <string>
#include <vector>

namespace rtdiag {

/// Analyzes latency data from trace events.
class LatencyAnalyzer {
public:
    LatencyAnalyzer();
    ~LatencyAnalyzer();

    /// Add a raw latency measurement (nanoseconds).
    void AddSample(uint64_t latency_ns);

    /// Add a latency result with metadata.
    void AddResult(const LatencyResult& result);

    /// Compute summary statistics from accumulated data.
    LatencyStats ComputeStats() const;

    /// Get all raw latency samples.
    const std::vector<uint64_t>& Samples() const;

    /// Total number of samples.
    size_t Count() const;

    /// Reset all accumulated data.
    void Reset();

    /// Generate an ASCII histogram of the latency distribution.
    /// @param num_buckets  Number of histogram buckets
    /// @param width        Width of the ASCII bars
    /// @return Multi-line string with the histogram
    std::string RenderHistogram(size_t num_buckets = 20,
                                 size_t width = 60) const;

    /// Generate a summary report string.
    std::string RenderReport() const;

    /// Export samples to a CSV file.
    /// @return true on success
    bool ExportCSV(const std::string& filepath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rtdiag
