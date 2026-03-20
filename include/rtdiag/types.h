#pragma once

/// @file types.h
/// @brief Core type definitions for the RT Diagnostic Toolchain.

#include <cstdint>
#include <string>
#include <vector>

namespace rtdiag {

/// Clock source for timestamps.
enum class ClockSource : uint8_t {
    kTSC = 0,       ///< CPU Time Stamp Counter (cycle-accurate)
    kMonotonic,      ///< CLOCK_MONOTONIC
    kMonotonicRaw,   ///< CLOCK_MONOTONIC_RAW
    kRealtime,       ///< CLOCK_REALTIME
};

/// Type of trace event captured from ftrace.
enum class EventType : uint8_t {
    kSchedSwitch = 0,
    kSchedWakeup,
    kIrqEntry,
    kIrqExit,
    kSoftirqEntry,
    kSoftirqExit,
    kFunctionEntry,
    kFunctionExit,
    kUserDefined,
    kUnknown,
};

/// Scheduling policy (mirrors Linux SCHED_* constants).
enum class SchedPolicy : uint8_t {
    kOther = 0,      ///< SCHED_OTHER  (CFS)
    kFifo  = 1,      ///< SCHED_FIFO
    kRR    = 2,      ///< SCHED_RR
    kDeadline = 6,   ///< SCHED_DEADLINE
};

/// A single trace event captured from the kernel.
struct TraceEvent {
    uint64_t    timestamp_ns;   ///< Nanosecond timestamp
    uint64_t    tsc;            ///< Raw TSC value (if available)
    uint32_t    cpu;            ///< CPU core ID
    int32_t     pid;            ///< Process ID
    int32_t     tid;            ///< Thread ID
    int32_t     priority;       ///< Scheduling priority
    EventType   type;           ///< Event type
    char        comm[16];       ///< Task command name
    char        details[128];   ///< Event-specific details

    TraceEvent()
        : timestamp_ns(0), tsc(0), cpu(0), pid(0), tid(0),
          priority(0), type(EventType::kUnknown) {
        comm[0] = '\0';
        details[0] = '\0';
    }
};

/// Scheduling-specific event (enriched from TraceEvent).
struct SchedEvent {
    uint64_t    timestamp_ns;
    uint32_t    cpu;
    int32_t     prev_pid;
    int32_t     next_pid;
    int32_t     prev_prio;
    int32_t     next_prio;
    SchedPolicy prev_policy;
    SchedPolicy next_policy;
    char        prev_comm[16];
    char        next_comm[16];

    SchedEvent()
        : timestamp_ns(0), cpu(0), prev_pid(0), next_pid(0),
          prev_prio(0), next_prio(0),
          prev_policy(SchedPolicy::kOther),
          next_policy(SchedPolicy::kOther) {
        prev_comm[0] = '\0';
        next_comm[0] = '\0';
    }
};

/// Result of latency analysis for a single measurement.
struct LatencyResult {
    uint64_t latency_ns;     ///< Measured latency in nanoseconds
    uint64_t timestamp_ns;   ///< When this latency was measured
    int32_t  pid;            ///< Process that experienced this latency
    uint32_t cpu;            ///< CPU where the measurement was taken
};

/// Summary statistics for a latency distribution.
struct LatencyStats {
    uint64_t count;
    double   min_ns;
    double   max_ns;
    double   mean_ns;
    double   stddev_ns;
    double   p50_ns;
    double   p90_ns;
    double   p95_ns;
    double   p99_ns;
    double   p999_ns;

    LatencyStats()
        : count(0), min_ns(0), max_ns(0), mean_ns(0), stddev_ns(0),
          p50_ns(0), p90_ns(0), p95_ns(0), p99_ns(0), p999_ns(0) {}
};

/// A deadline violation record.
struct DeadlineViolation {
    uint64_t timestamp_ns;        ///< When the violation occurred
    int32_t  pid;                 ///< Offending process
    uint64_t deadline_ns;         ///< Configured deadline
    uint64_t actual_ns;           ///< Actual execution/response time
    uint64_t overrun_ns;          ///< Amount over deadline
    char     comm[16];            ///< Task name

    DeadlineViolation()
        : timestamp_ns(0), pid(0), deadline_ns(0), actual_ns(0), overrun_ns(0) {
        comm[0] = '\0';
    }
};

/// A detected priority inversion instance.
struct PriorityInversion {
    uint64_t    timestamp_ns;         ///< When detected
    uint64_t    duration_ns;          ///< Duration of inversion
    int32_t     high_prio_pid;        ///< High-priority task (blocked)
    int32_t     low_prio_pid;         ///< Low-priority task (holding resource)
    int32_t     high_prio;            ///< Priority of blocked task
    int32_t     low_prio;             ///< Priority of holding task
    char        high_comm[16];        ///< High-priority task name
    char        low_comm[16];         ///< Low-priority task name

    PriorityInversion()
        : timestamp_ns(0), duration_ns(0), high_prio_pid(0), low_prio_pid(0),
          high_prio(0), low_prio(0) {
        high_comm[0] = '\0';
        low_comm[0] = '\0';
    }
};

/// Configuration for tracing sessions.
struct TraceConfig {
    ClockSource   clock_source = ClockSource::kTSC;
    uint32_t      buffer_size_kb = 4096;          ///< Per-CPU buffer size
    uint32_t      ring_buffer_capacity = 1 << 20; ///< Event ring buffer slots
    bool          trace_irqs = true;
    bool          trace_sched = true;
    bool          trace_functions = false;
    std::vector<std::string> filter_comms;         ///< Filter by task name
    std::vector<int32_t>     filter_pids;          ///< Filter by PID
};

}  // namespace rtdiag
