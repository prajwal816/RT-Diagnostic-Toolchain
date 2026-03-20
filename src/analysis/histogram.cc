#include "histogram.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace rtdiag {
namespace analysis {

Histogram::Histogram(size_t num_buckets, double min_val, double max_val)
    : total_count_(0), is_log_(false) {
    counts_.resize(num_buckets, 0);
    boundaries_.resize(num_buckets + 1);

    double step = (max_val - min_val) / static_cast<double>(num_buckets);
    for (size_t i = 0; i <= num_buckets; ++i) {
        boundaries_[i] = min_val + step * static_cast<double>(i);
    }
}

Histogram Histogram::Logarithmic(int min_exp, int max_exp) {
    Histogram h(1, 0, 1);  // Placeholder construction
    int num_buckets = max_exp - min_exp;
    if (num_buckets <= 0) num_buckets = 1;

    h.counts_.resize(static_cast<size_t>(num_buckets), 0);
    h.boundaries_.resize(static_cast<size_t>(num_buckets + 1));
    h.is_log_ = true;
    h.total_count_ = 0;

    for (int i = 0; i <= num_buckets; ++i) {
        h.boundaries_[static_cast<size_t>(i)] = std::pow(2.0, min_exp + i);
    }

    return h;
}

void Histogram::Add(double value) {
    size_t bucket = FindBucket(value);
    ++counts_[bucket];
    ++total_count_;
}

void Histogram::Add(double value, size_t count) {
    size_t bucket = FindBucket(value);
    counts_[bucket] += count;
    total_count_ += count;
}

void Histogram::Merge(const Histogram& other) {
    if (counts_.size() != other.counts_.size()) return;
    for (size_t i = 0; i < counts_.size(); ++i) {
        counts_[i] += other.counts_[i];
    }
    total_count_ += other.total_count_;
}

uint64_t Histogram::BucketCount(size_t bucket) const {
    if (bucket >= counts_.size()) return 0;
    return counts_[bucket];
}

double Histogram::BucketLower(size_t bucket) const {
    if (bucket >= counts_.size()) return 0;
    return boundaries_[bucket];
}

double Histogram::BucketUpper(size_t bucket) const {
    if (bucket >= counts_.size()) return 0;
    return boundaries_[bucket + 1];
}

uint64_t Histogram::TotalCount() const {
    return total_count_;
}

size_t Histogram::FindBucket(double value) const {
    if (value <= boundaries_.front()) return 0;
    if (value >= boundaries_.back()) return counts_.size() - 1;

    // Binary search for the correct bucket
    auto it = std::upper_bound(boundaries_.begin(), boundaries_.end(), value);
    size_t idx = static_cast<size_t>(it - boundaries_.begin());
    if (idx > 0) --idx;
    if (idx >= counts_.size()) idx = counts_.size() - 1;
    return idx;
}

std::string Histogram::Render(size_t width) const {
    return Render("Histogram", width);
}

std::string Histogram::Render(const std::string& title, size_t width) const {
    std::ostringstream oss;

    uint64_t max_count = *std::max_element(counts_.begin(), counts_.end());
    if (max_count == 0) max_count = 1;

    oss << title << " (total: " << total_count_ << ")" << std::endl;
    oss << std::string(title.size() + 20, '-') << std::endl;

    for (size_t i = 0; i < counts_.size(); ++i) {
        // Format the bucket range
        std::ostringstream range_ss;
        if (is_log_) {
            range_ss << std::setw(10) << std::right << std::fixed
                     << std::setprecision(0) << boundaries_[i]
                     << " - " << std::setw(10) << std::left
                     << boundaries_[i + 1];
        } else {
            range_ss << std::setw(10) << std::right << std::fixed
                     << std::setprecision(1) << boundaries_[i]
                     << " - " << std::setw(10) << std::left
                     << std::setprecision(1) << boundaries_[i + 1];
        }

        oss << range_ss.str() << " |";

        // Draw the bar
        size_t bar_len = static_cast<size_t>(
            (static_cast<double>(counts_[i]) / static_cast<double>(max_count)) * width);
        for (size_t j = 0; j < bar_len; ++j) oss << '#';

        // Print count
        if (counts_[i] > 0) {
            oss << " " << counts_[i];
        }

        oss << std::endl;
    }

    return oss.str();
}

void Histogram::Reset() {
    std::fill(counts_.begin(), counts_.end(), 0);
    total_count_ = 0;
}

}  // namespace analysis
}  // namespace rtdiag
