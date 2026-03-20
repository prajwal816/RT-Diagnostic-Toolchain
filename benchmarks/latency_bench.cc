/// @file latency_bench.cc
/// @brief Benchmark: measures per-event processing overhead (target: <5µs).

#include "rtdiag/types.h"
#include "rtdiag/ring_buffer.h"
#include "src/analysis/histogram.h"
#include "src/analysis/statistics.h"
#include "src/utils/timestamp.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          Latency Overhead Benchmark           ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    const size_t ITERATIONS = 500000;

    // --- Timestamp overhead ---
    {
        std::cout << "--- Timestamp Read Overhead ---" << std::endl;
        rtdiag::analysis::RunningStats stats;

        for (size_t i = 0; i < ITERATIONS; ++i) {
            uint64_t start = rtdiag::utils::MonotonicNanos();
            uint64_t end = rtdiag::utils::MonotonicNanos();
            stats.Add(static_cast<double>(end - start));
        }

        std::cout << "  Iterations:    " << ITERATIONS << std::endl;
        std::cout << "  Mean:          " << std::fixed << std::setprecision(1)
                  << stats.Mean() << " ns" << std::endl;
        std::cout << "  Min:           " << stats.Min() << " ns" << std::endl;
        std::cout << "  Max:           " << stats.Max() << " ns" << std::endl;
        std::cout << "  StdDev:        " << std::setprecision(1)
                  << stats.StdDev() << " ns" << std::endl;
        std::cout << std::endl;
    }

    // --- Ring buffer push+pop overhead ---
    {
        std::cout << "--- Ring Buffer Push+Pop Overhead ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(1024);
        rtdiag::analysis::RunningStats stats;

        rtdiag::TraceEvent event;
        event.cpu = 0;
        event.pid = 1;
        strncpy(event.comm, "bench", sizeof(event.comm) - 1);

        for (size_t i = 0; i < ITERATIONS; ++i) {
            event.timestamp_ns = i;

            uint64_t start = rtdiag::utils::MonotonicNanos();
            ring.Push(event);
            rtdiag::TraceEvent out;
            ring.Pop(out);
            uint64_t end = rtdiag::utils::MonotonicNanos();

            stats.Add(static_cast<double>(end - start));
        }

        std::cout << "  Iterations:    " << ITERATIONS << std::endl;
        std::cout << "  Mean:          " << std::fixed << std::setprecision(1)
                  << stats.Mean() << " ns ("
                  << (stats.Mean() / 1000.0) << " µs)" << std::endl;
        std::cout << "  Min:           " << stats.Min() << " ns" << std::endl;
        std::cout << "  Max:           " << stats.Max() << " ns" << std::endl;
        std::cout << "  StdDev:        " << std::setprecision(1)
                  << stats.StdDev() << " ns" << std::endl;
        std::cout << "  Status:        "
                  << (stats.Mean() < 5000.0 ? "PASS" : "FAIL")
                  << " (target: <5µs)" << std::endl;
        std::cout << std::endl;
    }

    // --- Full event processing pipeline overhead ---
    {
        std::cout << "--- Full Pipeline Overhead (push + stats + pop) ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(1024);
        rtdiag::analysis::RunningStats pipeline_stats;
        rtdiag::analysis::RunningStats overhead_stats;

        rtdiag::TraceEvent event;
        event.cpu = 0;
        event.pid = 1;
        strncpy(event.comm, "bench", sizeof(event.comm) - 1);

        for (size_t i = 0; i < ITERATIONS; ++i) {
            event.timestamp_ns = i * 1000;

            uint64_t start = rtdiag::utils::MonotonicNanos();

            // Simulate full pipeline
            ring.Push(event);
            rtdiag::TraceEvent out;
            ring.Pop(out);
            pipeline_stats.Add(static_cast<double>(out.timestamp_ns));

            uint64_t end = rtdiag::utils::MonotonicNanos();
            overhead_stats.Add(static_cast<double>(end - start));
        }

        std::cout << "  Iterations:    " << ITERATIONS << std::endl;
        std::cout << "  Mean overhead: " << std::fixed << std::setprecision(1)
                  << overhead_stats.Mean() << " ns ("
                  << (overhead_stats.Mean() / 1000.0) << " µs)" << std::endl;
        std::cout << "  Min:           " << overhead_stats.Min() << " ns" << std::endl;
        std::cout << "  Max:           " << overhead_stats.Max() << " ns" << std::endl;
        std::cout << "  Status:        "
                  << (overhead_stats.Mean() < 5000.0 ? "PASS" : "FAIL")
                  << " (target: <5µs)" << std::endl;
        std::cout << std::endl;
    }

    // --- Overhead histogram ---
    {
        std::cout << "--- Overhead Distribution ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(1024);
        rtdiag::analysis::Histogram hist(20, 0, 5000);

        rtdiag::TraceEvent event;
        event.cpu = 0;
        event.pid = 1;
        strncpy(event.comm, "bench", sizeof(event.comm) - 1);

        for (size_t i = 0; i < ITERATIONS; ++i) {
            event.timestamp_ns = i;
            uint64_t start = rtdiag::utils::MonotonicNanos();
            ring.Push(event);
            rtdiag::TraceEvent out;
            ring.Pop(out);
            uint64_t end = rtdiag::utils::MonotonicNanos();
            hist.Add(static_cast<double>(end - start));
        }

        std::cout << hist.Render("Per-Event Overhead (ns)", 50) << std::endl;
    }

    std::cout << "Benchmark complete." << std::endl;
    return 0;
}
