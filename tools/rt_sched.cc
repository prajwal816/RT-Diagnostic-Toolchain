/// @file rt_sched.cc
/// @brief CLI tool for scheduling analysis: deadline monitoring and
///        priority inversion detection.
///
/// Usage:
///   rt-sched [OPTIONS]
///
/// Options:
///   --input <file>       Input trace file
///   --deadline <ns>      Default deadline in nanoseconds (default: 10000)
///   --samples <count>    Synthetic event count (default: 10000)
///   --help               Show this help message

#include "rtdiag/sched_analyzer.h"
#include "rtdiag/tracer.h"
#include "rtdiag/types.h"
#include "src/utils/timestamp.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static void PrintUsage(const char* prog) {
    std::cout << "RT Diagnostic Toolchain - Scheduling Analyzer" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << prog << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --input <file>       Trace file to analyze" << std::endl;
    std::cout << "  --deadline <ns>      Default deadline (default: 10000)" << std::endl;
    std::cout << "  --samples <count>    Synthetic events (default: 10000)" << std::endl;
    std::cout << "  --help               Show this message" << std::endl;
}

/// Generate synthetic scheduling events for demonstration.
static std::vector<rtdiag::SchedEvent> GenerateSyntheticSchedEvents(
    size_t count, uint64_t base_ns) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> cpu_dist(0, 3);
    std::uniform_int_distribution<int32_t> pid_dist(100, 500);
    std::uniform_int_distribution<int32_t> prio_dist(1, 99);
    std::normal_distribution<double> interval_dist(5000.0, 2000.0);

    const char* task_names[] = {
        "rt_ctrl", "sensor_rd", "motor_wr", "planner",
        "logger", "monitor", "watchdog", "idle"
    };
    const int num_names = sizeof(task_names) / sizeof(task_names[0]);

    std::vector<rtdiag::SchedEvent> events;
    events.reserve(count);

    uint64_t ts = base_ns;

    for (size_t i = 0; i < count; ++i) {
        rtdiag::SchedEvent se;
        se.timestamp_ns = ts;
        se.cpu = cpu_dist(rng);
        se.prev_pid = pid_dist(rng);
        se.next_pid = pid_dist(rng);
        se.prev_prio = prio_dist(rng);
        se.next_prio = prio_dist(rng);

        strncpy(se.prev_comm, task_names[se.prev_pid % num_names],
                sizeof(se.prev_comm) - 1);
        se.prev_comm[sizeof(se.prev_comm) - 1] = '\0';
        strncpy(se.next_comm, task_names[se.next_pid % num_names],
                sizeof(se.next_comm) - 1);
        se.next_comm[sizeof(se.next_comm) - 1] = '\0';

        events.push_back(se);

        double interval = std::max(500.0, interval_dist(rng));
        ts += static_cast<uint64_t>(interval);
    }

    return events;
}

/// Generate synthetic trace events for deadline monitoring.
static std::vector<rtdiag::TraceEvent> GenerateSyntheticTraceEvents(
    size_t count, uint64_t base_ns) {
    std::mt19937 rng(123);
    std::uniform_int_distribution<uint32_t> cpu_dist(0, 3);
    std::normal_distribution<double> exec_time(8000.0, 4000.0);

    const int32_t pids[] = {101, 102, 103, 104, 105};
    const char* names[] = {"rt_ctrl", "sensor", "motor", "planner", "logger"};
    const int num_tasks = sizeof(pids) / sizeof(pids[0]);

    std::vector<rtdiag::TraceEvent> events;
    events.reserve(count * 2);

    uint64_t ts = base_ns;

    for (size_t i = 0; i < count; ++i) {
        int task_idx = static_cast<int>(i % num_tasks);

        // Start event
        rtdiag::TraceEvent start;
        start.timestamp_ns = ts;
        start.cpu = cpu_dist(rng);
        start.pid = pids[task_idx];
        start.tid = pids[task_idx];
        start.priority = 10 + task_idx * 10;
        start.type = rtdiag::EventType::kFunctionEntry;
        strncpy(start.comm, names[task_idx], sizeof(start.comm) - 1);
        events.push_back(start);

        // End event (after some execution time)
        double exec = std::max(500.0, exec_time(rng));
        rtdiag::TraceEvent end = start;
        end.timestamp_ns = ts + static_cast<uint64_t>(exec);
        end.type = rtdiag::EventType::kFunctionExit;
        events.push_back(end);

        ts += static_cast<uint64_t>(exec) + 1000;
    }

    return events;
}

int main(int argc, char* argv[]) {
    std::string input_file;
    uint64_t default_deadline_ns = 10000;
    size_t num_samples = 10000;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_file = argv[++i];
        } else if (strcmp(argv[i], "--deadline") == 0 && i + 1 < argc) {
            default_deadline_ns = static_cast<uint64_t>(std::atol(argv[++i]));
        } else if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            num_samples = static_cast<size_t>(std::atol(argv[++i]));
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║      RT Diagnostic Scheduling Analyzer       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    uint64_t base_ns = rtdiag::utils::MonotonicNanos();

    // --- Deadline Monitoring ---
    std::cout << "━━━ Part 1: Deadline Monitoring ━━━" << std::endl;
    std::cout << "Default deadline: " << default_deadline_ns << " ns ("
              << (default_deadline_ns / 1000.0) << " µs)" << std::endl;
    std::cout << std::endl;

    rtdiag::DeadlineMonitor dm;
    dm.SetDefaultDeadline(default_deadline_ns);

    auto trace_events = GenerateSyntheticTraceEvents(num_samples, base_ns);
    std::cout << "[INFO] Generated " << trace_events.size()
              << " trace events for deadline analysis" << std::endl;

    auto violations = dm.AnalyzeEvents(trace_events);
    std::cout << std::endl;
    std::cout << dm.RenderReport() << std::endl;

    // --- Priority Inversion Detection ---
    std::cout << "━━━ Part 2: Priority Inversion Detection ━━━" << std::endl;
    std::cout << std::endl;

    rtdiag::PriorityInversionDetector pid_detector;

    auto sched_events = GenerateSyntheticSchedEvents(num_samples, base_ns);
    std::cout << "[INFO] Generated " << sched_events.size()
              << " scheduling events for inversion analysis" << std::endl;

    auto inversions = pid_detector.Analyze(sched_events);
    std::cout << std::endl;
    std::cout << pid_detector.RenderReport() << std::endl;

    // Summary
    std::cout << "━━━ Summary ━━━" << std::endl;
    std::cout << "Deadline violations: " << violations.size() << std::endl;
    std::cout << "Priority inversions: " << inversions.size() << std::endl;

    return 0;
}
