# Hard Real-Time Diagnostic Toolchain

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/) [![Bazel](https://img.shields.io/badge/Build-Bazel-43A047.svg)](https://bazel.build/) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A production-grade tracing and diagnostics toolkit for profiling hard real-time C++ applications on Linux PREEMPT_RT systems. Integrates with **ftrace** and **perf** to provide cycle-accurate latency tracing, deadline-miss detection, priority-inversion analysis, and high-throughput event processing (500K+ events/sec with < 5 µs overhead).

---

## Architecture

```
┌───────────────────────────────────────────────────────────┐
│                     CLI Tools Layer                       │
│   rt-trace          rt-analyze          rt-sched          │
├───────────────┬──────────────────┬────────────────────────┤
│   Tracing     │    Analysis      │    Scheduling          │
│ ┌───────────┐ │ ┌──────────────┐ │ ┌────────────────────┐ │
│ │ FtraceRdr │ │ │ LatencyAnlzr │ │ │ DeadlineMonitor    │ │
│ │ EvtParser │ │ │ Histogram    │ │ │ PrioInvDetector    │ │
│ │ TraceBuf  │ │ │ Statistics   │ │ │                    │ │
│ └───────────┘ │ └──────────────┘ │ └────────────────────┘ │
├───────────────┴──────────────────┴────────────────────────┤
│                    Core Utilities                         │
│    RingBuffer<T> (SPSC)    Timestamp (RDTSC)    Logger    │
├───────────────────────────────────────────────────────────┤
│                  Public API (include/rtdiag/)             │
│  types.h  ring_buffer.h  tracer.h  analyzer.h  sched_*.h │
└───────────────────────────────────────────────────────────┘
           │                        │
    ┌──────┴──────┐          ┌──────┴──────┐
    │   ftrace    │          │    perf     │
    │ trace_pipe  │          │  subsystem  │
    └─────────────┘          └─────────────┘
```

### Key Design Decisions

| Decision | Rationale |
|---|---|
| **Lock-free SPSC Ring Buffer** | Zero-contention event buffering between producer (kernel reader) and consumer (analyzer) |
| **RDTSC Timestamps** | Cycle-accurate timing with < 25 ns read overhead |
| **Cache-line Aligned Atomics** | Prevents false sharing in concurrent hot paths |
| **Power-of-Two Buffer Sizing** | Enables bitmask modulo (single AND instruction vs. division) |
| **PIMPL Pattern** | ABI stability for public API; implementation changes don't break consumers |
| **Fixed-size Char Arrays** | Zero heap allocation in `TraceEvent` — critical for hot paths |

---

## Linux PREEMPT_RT Overview

The **PREEMPT_RT** patch set transforms the Linux kernel into a fully preemptible real-time operating system:

### What PREEMPT_RT Does

1. **Converts spinlocks to RT mutexes** — Makes nearly all kernel code preemptible
2. **Threaded interrupt handlers** — IRQs run as schedulable kernel threads with configurable priorities
3. **Priority inheritance** — Prevents unbounded priority inversion on mutexes
4. **High-resolution timers** — Nanosecond-precision timer infrastructure
5. **Deterministic scheduling** — Bounded worst-case latency (typically < 100 µs)

### Scheduling Policies

| Policy | Use Case | Priority Range |
|---|---|---|
| `SCHED_FIFO` | Highest-priority RT tasks | 1–99 (99 = highest) |
| `SCHED_RR` | Round-robin among same-priority RT tasks | 1–99 |
| `SCHED_DEADLINE` | Earliest Deadline First (EDF) scheduling | N/A (uses runtime/deadline/period) |
| `SCHED_OTHER` | Default CFS for non-RT tasks | 0 (nice -20 to +19) |

### Common RT Problems This Toolchain Detects

- **Deadline misses** — Task execution exceeds its timing constraint
- **Priority inversion** — Low-priority task blocks a high-priority task (unbounded if no PI protocol)
- **IRQ storms** — Excessive interrupt handling starving RT tasks
- **Scheduling latency** — Delay between wakeup and actual execution

---

## Features

### 1. Latency Tracing
- Direct ftrace integration via `/sys/kernel/debug/tracing/trace_pipe`
- Cycle-accurate timestamps using `RDTSC` / `RDTSCP` with auto-calibration
- Simulated fallback mode for development/CI without root access

### 2. Deadline Monitoring
- Per-task and global deadline configuration
- Automatic event pairing (function entry/exit)
- Violation logging with overrun metrics

### 3. Priority Inversion Detection
- Analyzes `sched_switch` events across all CPUs
- Tracks CPU state machines to detect blocking patterns
- Reports inversion duration, involved tasks, and priorities

### 4. High-Throughput Processing
- Lock-free SPSC ring buffer handles **500K+ events/sec**
- Configurable overflow policy: **drop** (non-blocking) or **overwrite** (latest wins)
- Batch drain with optional event filtering

### 5. Visualization & Reporting
- ASCII latency histograms (linear and logarithmic buckets)
- Box-drawing formatted statistics reports (min/max/mean/p50/p90/p95/p99/p99.9)
- CSV export for offline analysis

---

## Project Structure

```
RT-Diagnostic-Toolchain/
├── WORKSPACE              # Bazel workspace (GoogleTest dependency)
├── BUILD.bazel            # Top-level build
├── .bazelrc               # C++17, strict warnings, optimizations
│
├── include/rtdiag/        # Public API headers
│   ├── types.h            # Core types (TraceEvent, SchedEvent, etc.)
│   ├── ring_buffer.h      # Lock-free SPSC ring buffer
│   ├── tracer.h           # FtraceTracer API
│   ├── analyzer.h         # LatencyAnalyzer API
│   └── sched_analyzer.h   # DeadlineMonitor, PriorityInversionDetector
│
├── src/
│   ├── utils/             # Timestamp (RDTSC), Logger
│   ├── tracing/           # FtraceReader, EventParser, TraceBuffer
│   ├── analysis/          # Histogram, RunningStats, LatencyAnalyzer
│   └── sched/             # DeadlineMonitor, PriorityInversionDetector
│
├── tools/                 # CLI binaries
│   ├── rt_trace.cc        # Capture ftrace events
│   ├── rt_analyze.cc      # Latency analysis + histograms
│   └── rt_sched.cc        # Deadline + priority inversion analysis
│
├── benchmarks/            # Performance benchmarks
│   ├── throughput_bench.cc
│   └── latency_bench.cc
│
├── tests/                 # Unit + integration tests (GoogleTest)
│   ├── ring_buffer_test.cc
│   ├── event_parser_test.cc
│   ├── latency_analyzer_test.cc
│   ├── histogram_test.cc
│   ├── statistics_test.cc
│   ├── deadline_monitor_test.cc
│   ├── priority_inversion_test.cc
│   └── integration_test.cc
│
└── scripts/
    ├── setup_ftrace.sh       # Configure ftrace tracers
    ├── check_rt_config.sh    # Validate PREEMPT_RT system config
    └── run_benchmarks.sh     # Build & run all benchmarks
```

---

## Setup Instructions

### Prerequisites

| Requirement | Minimum Version | Notes |
|---|---|---|
| **Bazel** | 6.0+ | Build system |
| **GCC / Clang** | GCC 9+ / Clang 11+ | C++17 with `__uint128_t` support |
| **Linux kernel** | 5.x+ | PREEMPT_RT patch recommended |
| **Root access** | — | Required for live ftrace (simulated mode works without) |

### Building

```bash
# Clone the repository
git clone https://github.com/your-org/RT-Diagnostic-Toolchain.git
cd RT-Diagnostic-Toolchain

# Build everything (hermetic — downloads GoogleTest automatically)
bazel build //...

# Build specific targets
bazel build //tools:rt_trace
bazel build //tools:rt_analyze
bazel build //tools:rt_sched
```

### Running Tests

```bash
# All tests
bazel test //tests/...

# Individual test
bazel test //tests:ring_buffer_test
```

### Kernel Setup (Linux with PREEMPT_RT)

```bash
# 1. Check your system
sudo ./scripts/check_rt_config.sh

# 2. Configure ftrace
sudo ./scripts/setup_ftrace.sh function_graph

# 3. Optimize for RT (optional)
echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us   # Disable RT throttling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

---

## Usage

### rt-trace — Capture Events

```bash
# Simulated mode (no root required)
bazel run //tools:rt_trace -- --simulated --events 100000

# Live ftrace capture (requires root)
sudo bazel-bin/tools/rt_trace --duration 10 --output trace.log
```

### rt-analyze — Latency Analysis

```bash
# Analyze with synthetic data (demo)
bazel run //tools:rt_analyze -- --samples 50000 --buckets 20

# Analyze a trace file
bazel run //tools:rt_analyze -- --input trace.log --csv latencies.csv
```

### rt-sched — Scheduling Analysis

```bash
# Run with synthetic scheduling data
bazel run //tools:rt_sched -- --samples 10000 --deadline 10000
```

---

## Example Analysis Output

### Latency Report
```
╔══════════════════════════════════════════════════════════════╗
║           RT Latency Analysis Report                       ║
╠══════════════════════════════════════════════════════════════╣
║  Samples: 50000                                            ║
╠══════════════════════════════════════════════════════════════╣
║  Min                         0.10 µs                       ║
║  Max                        25.38 µs                       ║
║  Mean                        3.29 µs                       ║
║  Std Dev                     4.10 µs                       ║
╠══════════════════════════════════════════════════════════════╣
║  P50 (median)                2.05 µs                       ║
║  P90                         4.12 µs                       ║
║  P95                        12.30 µs                       ║
║  P99                        19.44 µs                       ║
║  P99.9                      23.87 µs                       ║
╚══════════════════════════════════════════════════════════════╝

Latency Distribution (ns) (total: 49999)
-----------------------------------------
     100.0 -      1362.0 |############################################# 12804
    1362.0 -      2624.0 |############################################################ 16012
    2624.0 -      3886.0 |################################### 9345
    3886.0 -      5148.0 |############# 3422
    5148.0 -      6410.0 |###### 1508
    ...
```

### Deadline Violation Report
```
=== Deadline Violation Report ===
Total violations: 42

PID     Task            Deadline(us)    Actual(us)      Overrun(us)
------------------------------------------------------------------------
101     rt_ctrl             10.00           15.23            5.23
103     motor               10.00           12.87            2.87
104     planner             10.00           18.41            8.41
...
```

### Priority Inversion Report
```
=== Priority Inversion Report ===
Total inversions detected: 15

Timestamp(us)     Hi-PID      Hi-Task     Hi-Prio   Lo-PID      Lo-Task     Lo-Prio   Duration(us)
----------------------------------------------------------------------------------------------------
1234567.89        100         rt_ctrl     5         300         logger      80        12.34
1234890.12        200         sensor_rd   10        400         monitor     90        8.56
...
```

---

## Benchmarks

### Expected Results

| Metric | Target | Typical Result |
|---|---|---|
| **Ring buffer push throughput** | ≥ 500K events/sec | > 5M events/sec |
| **SPSC producer-consumer** | ≥ 500K events/sec | > 2M events/sec |
| **Per-event overhead (push+pop)** | < 5 µs | < 500 ns |
| **Timestamp read overhead** | < 100 ns | < 50 ns |

```bash
# Run all benchmarks
./scripts/run_benchmarks.sh

# Or individually
bazel run //benchmarks:throughput_bench
bazel run //benchmarks:latency_bench
```

---

## API Quick Start

```cpp
#include "rtdiag/tracer.h"
#include "rtdiag/analyzer.h"
#include "rtdiag/sched_analyzer.h"

// 1. Capture events
rtdiag::FtraceTracer tracer;
tracer.GenerateSyntheticEvents(10000);  // or tracer.Start() for live

std::vector<rtdiag::TraceEvent> events;
tracer.ReadEvents(events, 10000);

// 2. Analyze latencies
rtdiag::LatencyAnalyzer analyzer;
for (size_t i = 1; i < events.size(); ++i)
    analyzer.AddSample(events[i].timestamp_ns - events[i-1].timestamp_ns);

std::cout << analyzer.RenderReport();
analyzer.ExportCSV("latencies.csv");

// 3. Check deadlines
rtdiag::DeadlineMonitor dm;
dm.SetDefaultDeadline(10000);  // 10µs
dm.AnalyzeEvents(events);
std::cout << dm.RenderReport();

// 4. Detect priority inversions
rtdiag::PriorityInversionDetector pid;
pid.AnalyzeTraceEvents(events);
std::cout << pid.RenderReport();
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.
