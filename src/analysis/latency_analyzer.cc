#include "rtdiag/analyzer.h"
#include "src/analysis/histogram.h"
#include "src/analysis/statistics.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace rtdiag {

struct LatencyAnalyzer::Impl {
    std::vector<uint64_t> samples;
    analysis::RunningStats stats;
};

LatencyAnalyzer::LatencyAnalyzer() : impl_(std::make_unique<Impl>()) {}
LatencyAnalyzer::~LatencyAnalyzer() = default;

void LatencyAnalyzer::AddSample(uint64_t latency_ns) {
    impl_->samples.push_back(latency_ns);
    impl_->stats.Add(static_cast<double>(latency_ns));
}

void LatencyAnalyzer::AddResult(const LatencyResult& result) {
    AddSample(result.latency_ns);
}

LatencyStats LatencyAnalyzer::ComputeStats() const {
    LatencyStats stats;
    if (impl_->samples.empty()) return stats;

    stats.count = impl_->samples.size();
    stats.min_ns = impl_->stats.Min();
    stats.max_ns = impl_->stats.Max();
    stats.mean_ns = impl_->stats.Mean();
    stats.stddev_ns = impl_->stats.SampleStdDev();

    // Compute percentiles from sorted copy
    std::vector<uint64_t> sorted = impl_->samples;
    std::sort(sorted.begin(), sorted.end());

    auto percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p / 100.0 * static_cast<double>(sorted.size() - 1));
        return static_cast<double>(sorted[idx]);
    };

    stats.p50_ns = percentile(50.0);
    stats.p90_ns = percentile(90.0);
    stats.p95_ns = percentile(95.0);
    stats.p99_ns = percentile(99.0);
    stats.p999_ns = percentile(99.9);

    return stats;
}

const std::vector<uint64_t>& LatencyAnalyzer::Samples() const {
    return impl_->samples;
}

size_t LatencyAnalyzer::Count() const {
    return impl_->samples.size();
}

void LatencyAnalyzer::Reset() {
    impl_->samples.clear();
    impl_->stats.Reset();
}

std::string LatencyAnalyzer::RenderHistogram(size_t num_buckets,
                                               size_t width) const {
    if (impl_->samples.empty()) return "No data to display.\n";

    double min_val = impl_->stats.Min();
    double max_val = impl_->stats.Max();
    if (min_val == max_val) max_val = min_val + 1.0;

    analysis::Histogram hist(num_buckets, min_val, max_val + 1.0);
    for (uint64_t s : impl_->samples) {
        hist.Add(static_cast<double>(s));
    }

    return hist.Render("Latency Distribution (ns)", width);
}

std::string LatencyAnalyzer::RenderReport() const {
    if (impl_->samples.empty()) return "No latency data collected.\n";

    LatencyStats stats = ComputeStats();
    std::ostringstream oss;

    oss << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    oss << "║           RT Latency Analysis Report                       ║" << std::endl;
    oss << "╠══════════════════════════════════════════════════════════════╣" << std::endl;

    auto fmt = [&](const char* label, double val_ns) {
        oss << "║  " << std::left << std::setw(20) << label
            << std::right << std::setw(12) << std::fixed << std::setprecision(2)
            << (val_ns / 1000.0) << " µs"
            << std::setw(25) << " ║" << std::endl;
    };

    oss << "║  Samples: " << std::setw(49) << std::left << stats.count
        << "║" << std::endl;
    oss << "╠══════════════════════════════════════════════════════════════╣" << std::endl;

    fmt("Min", stats.min_ns);
    fmt("Max", stats.max_ns);
    fmt("Mean", stats.mean_ns);
    fmt("Std Dev", stats.stddev_ns);
    oss << "╠══════════════════════════════════════════════════════════════╣" << std::endl;
    fmt("P50 (median)", stats.p50_ns);
    fmt("P90", stats.p90_ns);
    fmt("P95", stats.p95_ns);
    fmt("P99", stats.p99_ns);
    fmt("P99.9", stats.p999_ns);
    oss << "╚══════════════════════════════════════════════════════════════╝" << std::endl;

    oss << std::endl;
    oss << RenderHistogram();

    return oss.str();
}

bool LatencyAnalyzer::ExportCSV(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "sample_index,latency_ns,latency_us" << std::endl;
    for (size_t i = 0; i < impl_->samples.size(); ++i) {
        file << i << "," << impl_->samples[i] << ","
             << std::fixed << std::setprecision(3)
             << (impl_->samples[i] / 1000.0) << std::endl;
    }

    return true;
}

}  // namespace rtdiag
