#include "timestamp.h"

#include <chrono>
#include <thread>

namespace rtdiag {
namespace utils {

uint64_t MonotonicNanos() {
#if defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    // Portable fallback
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch())
            .count());
#endif
}

uint64_t MonotonicRawNanos() {
#if defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    return MonotonicNanos();  // Fallback
#endif
}

/// Calibrate TSC frequency by measuring cycles over a known interval.
static uint64_t CalibrateTSCFrequency() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    const uint64_t start_tsc = ReadTSC();
    const uint64_t start_ns = MonotonicNanos();

    // Sleep for ~10ms to calibrate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const uint64_t end_tsc = ReadTSC();
    const uint64_t end_ns = MonotonicNanos();

    const uint64_t delta_tsc = end_tsc - start_tsc;
    const uint64_t delta_ns = end_ns - start_ns;

    if (delta_ns == 0) return 1000000000ULL;  // Fallback: 1 GHz
    return (delta_tsc * 1000000000ULL) / delta_ns;
#else
    return 1000000000ULL;  // Simulated: 1 GHz
#endif
}

uint64_t GetTSCFrequency() {
    static const uint64_t freq = CalibrateTSCFrequency();
    return freq;
}

ScopeTimer::ScopeTimer(uint64_t& out_ns)
    : out_ns_(out_ns), start_(MonotonicNanos()) {}

ScopeTimer::~ScopeTimer() {
    out_ns_ = MonotonicNanos() - start_;
}

}  // namespace utils
}  // namespace rtdiag
