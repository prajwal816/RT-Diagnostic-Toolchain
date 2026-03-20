#pragma once

/// @file timestamp.h
/// @brief Cycle-accurate timestamp utilities using TSC and clock_gettime.

#include <cstdint>

namespace rtdiag {
namespace utils {

/// Read the CPU Time Stamp Counter (x86 RDTSC instruction).
/// Falls back to a monotonic clock on non-x86 or simulated environments.
inline uint64_t ReadTSC() {
#if defined(__x86_64__) || defined(_M_X64)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#elif defined(__i386__)
    uint64_t val;
    __asm__ __volatile__("rdtsc" : "=A"(val));
    return val;
#else
    // Fallback: use monotonic clock
    return MonotonicNanos();
#endif
}

/// Read RDTSCP (serializing variant, includes core ID).
/// @param[out] aux  IA32_TSC_AUX (typically encodes processor ID)
inline uint64_t ReadTSCP(uint32_t& aux) {
#if defined(__x86_64__) || defined(_M_X64)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    aux = 0;
    return MonotonicNanos();
#endif
}

/// Get current time in nanoseconds from CLOCK_MONOTONIC.
uint64_t MonotonicNanos();

/// Get current time in nanoseconds from CLOCK_MONOTONIC_RAW.
uint64_t MonotonicRawNanos();

/// Get the estimated TSC frequency in Hz.
/// Determined at startup by calibration.
uint64_t GetTSCFrequency();

/// Convert a TSC delta to nanoseconds.
/// @param tsc_delta  Number of TSC ticks
/// @return Equivalent nanoseconds
inline uint64_t TSCToNanos(uint64_t tsc_delta) {
    const uint64_t freq = GetTSCFrequency();
    if (freq == 0) return 0;
#if defined(__SIZEOF_INT128__) || defined(__x86_64__)
    // Use 128-bit multiplication to avoid overflow
    return static_cast<uint64_t>(
        (static_cast<__uint128_t>(tsc_delta) * 1000000000ULL) / freq);
#else
    // Portable fallback: split the computation to avoid overflow
    //   tsc_delta * 1e9 / freq ≈ (tsc_delta / freq) * 1e9 + remainder
    uint64_t seconds = tsc_delta / freq;
    uint64_t remainder = tsc_delta % freq;
    return seconds * 1000000000ULL + (remainder * 1000000000ULL) / freq;
#endif
}

/// A timestamp pair for measuring intervals.
struct TimestampPair {
    uint64_t tsc_start;
    uint64_t tsc_end;
    uint64_t ns_start;
    uint64_t ns_end;

    uint64_t ElapsedTSC() const { return tsc_end - tsc_start; }
    uint64_t ElapsedNanos() const { return ns_end - ns_start; }
};

/// RAII scope timer that records elapsed time.
class ScopeTimer {
public:
    explicit ScopeTimer(uint64_t& out_ns);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    uint64_t& out_ns_;
    uint64_t start_;
};

}  // namespace utils
}  // namespace rtdiag
