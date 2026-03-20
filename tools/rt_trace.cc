/// @file rt_trace.cc
/// @brief CLI tool for capturing ftrace events from a real-time Linux system.
///
/// Usage:
///   rt-trace [OPTIONS]
///
/// Options:
///   --duration <sec>   Capture duration in seconds (default: 5)
///   --events <count>   Max events to capture (default: 100000)
///   --output <file>    Output file for trace dump (default: stdout)
///   --simulated        Force simulated mode (no real ftrace)
///   --buffer-size <n>  Ring buffer capacity (default: 1048576)
///   --help             Show this help message

#include "rtdiag/tracer.h"
#include "rtdiag/types.h"
#include "src/utils/logger.h"
#include "src/utils/timestamp.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static void PrintUsage(const char* prog) {
    std::cout << "RT Diagnostic Toolchain - Trace Capture" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << prog << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --duration <sec>   Capture duration (default: 5)" << std::endl;
    std::cout << "  --events <count>   Max events (default: 100000)" << std::endl;
    std::cout << "  --output <file>    Output file (default: stdout)" << std::endl;
    std::cout << "  --simulated        Force simulated mode" << std::endl;
    std::cout << "  --buffer-size <n>  Ring buffer capacity (default: 1048576)" << std::endl;
    std::cout << "  --help             Show this message" << std::endl;
}

static const char* EventTypeName(rtdiag::EventType type) {
    switch (type) {
        case rtdiag::EventType::kSchedSwitch: return "sched_switch";
        case rtdiag::EventType::kSchedWakeup: return "sched_wakeup";
        case rtdiag::EventType::kIrqEntry:    return "irq_entry";
        case rtdiag::EventType::kIrqExit:     return "irq_exit";
        case rtdiag::EventType::kSoftirqEntry:return "softirq_entry";
        case rtdiag::EventType::kSoftirqExit: return "softirq_exit";
        case rtdiag::EventType::kFunctionEntry:return "func_entry";
        case rtdiag::EventType::kFunctionExit: return "func_exit";
        case rtdiag::EventType::kUserDefined: return "user";
        default:                              return "unknown";
    }
}

int main(int argc, char* argv[]) {
    int duration_sec = 5;
    size_t max_events = 100000;
    std::string output_file;
    bool simulated = false;
    uint32_t buffer_size = 1 << 20;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_sec = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
            max_events = static_cast<size_t>(std::atol(argv[++i]));
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--simulated") == 0) {
            simulated = true;
        } else if (strcmp(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            buffer_size = static_cast<uint32_t>(std::atol(argv[++i]));
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         RT Diagnostic Trace Capture          ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    // Configure tracer
    rtdiag::TraceConfig config;
    config.ring_buffer_capacity = buffer_size;

    rtdiag::FtraceTracer tracer(config);

    if (simulated || tracer.IsSimulated()) {
        std::cout << "[INFO] Running in simulated mode" << std::endl;
        std::cout << "[INFO] Generating " << max_events << " synthetic events..." << std::endl;
        std::cout << std::endl;

        // Generate synthetic events
        tracer.GenerateSyntheticEvents(max_events);
    } else {
        std::cout << "[INFO] Starting live ftrace capture for " << duration_sec << "s" << std::endl;
        std::cout << "[INFO] Buffer size: " << buffer_size << " events" << std::endl;
        std::cout << std::endl;

        if (!tracer.Start()) {
            std::cerr << "[ERROR] Failed to start tracer" << std::endl;
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
        tracer.Stop();
    }

    // Read captured events
    std::vector<rtdiag::TraceEvent> events;
    tracer.ReadEvents(events, max_events);

    std::cout << "[INFO] Captured " << events.size() << " events" << std::endl;
    std::cout << "[INFO] Dropped events: " << tracer.DroppedEvents() << std::endl;
    std::cout << std::endl;

    // Output
    std::ostream* out = &std::cout;
    std::ofstream file_out;
    if (!output_file.empty()) {
        file_out.open(output_file);
        if (!file_out.is_open()) {
            std::cerr << "[ERROR] Cannot open output file: " << output_file << std::endl;
            return 1;
        }
        out = &file_out;
        std::cout << "[INFO] Writing to " << output_file << std::endl;
    }

    // Write header
    *out << "# RT Diagnostic Trace Dump" << std::endl;
    *out << "# Events: " << events.size() << std::endl;
    *out << "# Format: timestamp_ns cpu pid priority type comm details" << std::endl;
    *out << "#" << std::endl;

    // Write events
    for (const auto& event : events) {
        *out << std::setw(20) << event.timestamp_ns
             << " [" << std::setw(3) << event.cpu << "]"
             << " " << std::setw(8) << event.pid
             << " p=" << std::setw(3) << event.priority
             << " " << std::setw(14) << EventTypeName(event.type)
             << " " << event.comm
             << " " << event.details
             << std::endl;
    }

    if (!output_file.empty()) {
        std::cout << "[INFO] Trace dump written successfully" << std::endl;
    }

    return 0;
}
