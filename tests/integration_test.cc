/// @file integration_test.cc
/// @brief End-to-end integration test: synthetic events → analysis → verification.

#include "rtdiag/analyzer.h"
#include "rtdiag/ring_buffer.h"
#include "rtdiag/sched_analyzer.h"
#include "rtdiag/tracer.h"
#include "rtdiag/types.h"
#include "src/analysis/histogram.h"
#include "src/analysis/statistics.h"
#include "src/tracing/event_parser.h"
#include "src/tracing/trace_buffer.h"

#include "gtest/gtest.h"

#include <cstring>

namespace rtdiag {
namespace {

// Full pipeline: generate → buffer → drain → analyze
TEST(IntegrationTest, FullPipeline) {
    // 1. Generate synthetic events via the tracer
    FtraceTracer tracer;
    EXPECT_TRUE(tracer.IsSimulated());

    const size_t NUM_EVENTS = 10000;
    tracer.GenerateSyntheticEvents(NUM_EVENTS);

    // 2. Read events into a vector
    std::vector<TraceEvent> events;
    size_t read = tracer.ReadEvents(events, NUM_EVENTS);
    EXPECT_GT(read, 0u);
    EXPECT_EQ(events.size(), read);

    // 3. Feed into latency analyzer (use inter-event timestamps as latencies)
    LatencyAnalyzer analyzer;
    for (size_t i = 1; i < events.size(); ++i) {
        uint64_t latency = events[i].timestamp_ns - events[i - 1].timestamp_ns;
        analyzer.AddSample(latency);
    }

    EXPECT_EQ(analyzer.Count(), events.size() - 1);

    // 4. Compute stats
    auto stats = analyzer.ComputeStats();
    EXPECT_GT(stats.count, 0u);
    EXPECT_GT(stats.mean_ns, 0.0);
    EXPECT_LE(stats.min_ns, stats.max_ns);
    EXPECT_LE(stats.p50_ns, stats.p99_ns);

    // 5. Render reports
    std::string report = analyzer.RenderReport();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Min"), std::string::npos);

    std::string hist = analyzer.RenderHistogram();
    EXPECT_FALSE(hist.empty());
}

// Trace buffer integration: push → filter drain → verify
TEST(IntegrationTest, TraceBufferPipeline) {
    tracing::TraceBuffer buffer(1024);

    // Push mixed events
    for (int i = 0; i < 100; ++i) {
        TraceEvent event;
        event.timestamp_ns = i * 1000;
        event.pid = (i % 2 == 0) ? 100 : 200;
        event.type = (i % 2 == 0) ? EventType::kSchedSwitch
                                   : EventType::kIrqEntry;
        strncpy(event.comm, (i % 2 == 0) ? "task_a" : "task_b",
                sizeof(event.comm) - 1);
        buffer.Push(event);
    }

    EXPECT_EQ(buffer.Size(), 100u);

    // Filtered drain: only sched_switch events
    std::vector<TraceEvent> sched_events;
    buffer.DrainFiltered(sched_events, 100,
        [](const TraceEvent& e) {
            return e.type == EventType::kSchedSwitch;
        });

    EXPECT_EQ(sched_events.size(), 50u);
    for (const auto& e : sched_events) {
        EXPECT_EQ(e.type, EventType::kSchedSwitch);
        EXPECT_EQ(e.pid, 100);
    }
}

// Scheduling analysis integration
TEST(IntegrationTest, SchedulingAnalysis) {
    // Generate events where some will violate deadlines
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(5000);  // 5µs

    std::vector<TraceEvent> events;
    for (int i = 0; i < 20; ++i) {
        TraceEvent start;
        start.timestamp_ns = i * 100000;
        start.pid = 100 + (i % 3);
        start.type = EventType::kFunctionEntry;
        snprintf(start.comm, sizeof(start.comm), "task_%d", i % 3);
        events.push_back(start);

        TraceEvent end = start;
        // Some will exceed 5µs deadline
        end.timestamp_ns = start.timestamp_ns + (i % 4 == 0 ? 8000 : 3000);
        end.type = EventType::kFunctionExit;
        events.push_back(end);
    }

    auto violations = dm.AnalyzeEvents(events);
    EXPECT_GT(violations.size(), 0u);

    std::string report = dm.RenderReport();
    EXPECT_NE(report.find("Deadline Violation Report"), std::string::npos);
}

// Priority inversion integration
TEST(IntegrationTest, PriorityInversionAnalysis) {
    PriorityInversionDetector detector;

    std::vector<SchedEvent> events;
    // Create a scenario with inversions
    for (int i = 0; i < 10; ++i) {
        SchedEvent se;
        se.timestamp_ns = i * 10000;
        se.cpu = 0;
        se.prev_pid = 100;
        se.next_pid = 200 + i;
        // Alternate: sometimes high-prio switches to low-prio (inversion)
        se.prev_prio = (i % 3 == 0) ? 5 : 50;
        se.next_prio = (i % 3 == 0) ? 80 : 10;
        strncpy(se.prev_comm, "prev", sizeof(se.prev_comm) - 1);
        strncpy(se.next_comm, "next", sizeof(se.next_comm) - 1);
        events.push_back(se);
    }

    auto inversions = detector.Analyze(events);
    // i=0,3,6,9 should be inversions (prev_prio=5 < next_prio=80)
    EXPECT_GT(inversions.size(), 0u);

    std::string report = detector.RenderReport();
    EXPECT_FALSE(report.empty());
}

// Statistics + Histogram integration
TEST(IntegrationTest, StatisticsHistogram) {
    analysis::RunningStats stats;
    analysis::Histogram hist(10, 0, 10000);

    for (int i = 0; i < 1000; ++i) {
        double val = (i * 7 + 13) % 10000;
        stats.Add(val);
        hist.Add(val);
    }

    EXPECT_EQ(stats.Count(), 1000u);
    EXPECT_EQ(hist.TotalCount(), 1000u);
    EXPECT_GT(stats.StdDev(), 0.0);

    std::string rendered = hist.Render("Integration Test", 40);
    EXPECT_NE(rendered.find("1000"), std::string::npos);
}

// Event parser integration
TEST(IntegrationTest, EventParsing) {
    std::vector<std::string> raw_lines = {
        "# tracer: nop",
        "  worker-500 [001] d..2  2000.000100: sched_switch: "
        "prev_comm=worker prev_pid=500 prev_prio=20 prev_state=R "
        "==> next_comm=rt_task next_pid=300 next_prio=5",
        "  irqd-10    [002] d.h.  2000.000200: irq_handler_entry: irq=42 name=eth0",
        "  rt_task-300 [001] ....  2000.000300: sched_switch: "
        "prev_comm=rt_task prev_pid=300 prev_prio=5 prev_state=S "
        "==> next_comm=idle next_pid=0 next_prio=120",
    };

    auto events = tracing::ParseFtraceOutput(raw_lines);
    EXPECT_EQ(events.size(), 3u);

    // First event should be sched_switch
    EXPECT_EQ(events[0].type, EventType::kSchedSwitch);
    EXPECT_EQ(events[0].cpu, 1u);

    // Second event should be IRQ entry
    EXPECT_EQ(events[1].type, EventType::kIrqEntry);
    EXPECT_EQ(events[1].cpu, 2u);

    // Feed into latency analyzer
    LatencyAnalyzer la;
    for (size_t i = 1; i < events.size(); ++i) {
        la.AddSample(events[i].timestamp_ns - events[i - 1].timestamp_ns);
    }
    EXPECT_EQ(la.Count(), 2u);
}

}  // namespace
}  // namespace rtdiag
