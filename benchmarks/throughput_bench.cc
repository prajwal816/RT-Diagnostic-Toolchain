/// @file throughput_bench.cc
/// @brief Benchmark: measures ring buffer throughput (target: 500K+ events/sec).

#include "rtdiag/ring_buffer.h"
#include "rtdiag/types.h"
#include "src/utils/timestamp.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║       Throughput Benchmark (Ring Buffer)      ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    const size_t NUM_EVENTS = 2000000;
    const size_t BUFFER_SIZE = 1 << 20;  // 1M slots

    // Prepare a template event
    rtdiag::TraceEvent tmpl;
    tmpl.cpu = 0;
    tmpl.pid = 1234;
    tmpl.tid = 1234;
    tmpl.priority = 50;
    tmpl.type = rtdiag::EventType::kSchedSwitch;
    strncpy(tmpl.comm, "bench_task", sizeof(tmpl.comm) - 1);

    // --- Single-threaded push benchmark ---
    {
        std::cout << "--- Single-threaded Push ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(BUFFER_SIZE);

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < NUM_EVENTS; ++i) {
            tmpl.timestamp_ns = i;
            ring.Push(tmpl);
        }
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double events_per_sec = (NUM_EVENTS / elapsed_ms) * 1000.0;
        double ns_per_event = (elapsed_ms * 1e6) / NUM_EVENTS;

        std::cout << "  Events:        " << NUM_EVENTS << std::endl;
        std::cout << "  Elapsed:       " << std::fixed << std::setprecision(2)
                  << elapsed_ms << " ms" << std::endl;
        std::cout << "  Throughput:    " << std::fixed << std::setprecision(0)
                  << events_per_sec << " events/sec" << std::endl;
        std::cout << "  Per-event:     " << std::fixed << std::setprecision(1)
                  << ns_per_event << " ns" << std::endl;
        std::cout << "  Status:        " << (events_per_sec >= 500000 ? "PASS" : "FAIL")
                  << " (target: 500K+)" << std::endl;
        std::cout << std::endl;
    }

    // --- SPSC producer-consumer benchmark ---
    {
        std::cout << "--- SPSC Producer-Consumer ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(BUFFER_SIZE);

        std::atomic<size_t> consumed{0};
        std::atomic<bool> done{false};

        // Consumer thread
        std::thread consumer([&]() {
            rtdiag::TraceEvent event;
            while (!done.load(std::memory_order_relaxed) || !ring.Empty()) {
                if (ring.Pop(event)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        auto start = std::chrono::high_resolution_clock::now();

        // Producer
        for (size_t i = 0; i < NUM_EVENTS; ++i) {
            tmpl.timestamp_ns = i;
            while (!ring.Push(tmpl)) {
                // Spin if full
                std::this_thread::yield();
            }
        }
        done.store(true, std::memory_order_relaxed);
        consumer.join();

        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double events_per_sec = (NUM_EVENTS / elapsed_ms) * 1000.0;

        std::cout << "  Events:        " << NUM_EVENTS << std::endl;
        std::cout << "  Consumed:      " << consumed.load() << std::endl;
        std::cout << "  Elapsed:       " << std::fixed << std::setprecision(2)
                  << elapsed_ms << " ms" << std::endl;
        std::cout << "  Throughput:    " << std::fixed << std::setprecision(0)
                  << events_per_sec << " events/sec" << std::endl;
        std::cout << "  Overflow:      " << ring.OverflowCount() << std::endl;
        std::cout << "  Status:        " << (events_per_sec >= 500000 ? "PASS" : "FAIL")
                  << " (target: 500K+)" << std::endl;
        std::cout << std::endl;
    }

    // --- Drain benchmark ---
    {
        std::cout << "--- Batch Drain ---" << std::endl;
        rtdiag::RingBuffer<rtdiag::TraceEvent> ring(BUFFER_SIZE);

        // Fill the buffer
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            tmpl.timestamp_ns = i;
            ring.Push(tmpl);
        }

        std::vector<rtdiag::TraceEvent> batch;
        batch.reserve(BUFFER_SIZE);

        auto start = std::chrono::high_resolution_clock::now();
        ring.Drain(batch, BUFFER_SIZE);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double events_per_sec = (batch.size() / elapsed_ms) * 1000.0;

        std::cout << "  Drained:       " << batch.size() << " events" << std::endl;
        std::cout << "  Elapsed:       " << std::fixed << std::setprecision(2)
                  << elapsed_ms << " ms" << std::endl;
        std::cout << "  Throughput:    " << std::fixed << std::setprecision(0)
                  << events_per_sec << " events/sec" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "Benchmark complete." << std::endl;
    return 0;
}
