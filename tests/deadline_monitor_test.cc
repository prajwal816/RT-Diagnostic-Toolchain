#include "rtdiag/sched_analyzer.h"
#include "gtest/gtest.h"

#include <cstring>

namespace rtdiag {
namespace {

TEST(DeadlineMonitorTest, NoViolation) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(10000);  // 10µs

    TraceEvent start, end;
    start.timestamp_ns = 1000000;
    start.pid = 100;
    strncpy(start.comm, "task", sizeof(start.comm) - 1);

    end = start;
    end.timestamp_ns = 1005000;  // 5µs later (within deadline)

    EXPECT_FALSE(dm.CheckDeadline(start, end));
    EXPECT_TRUE(dm.Violations().empty());
}

TEST(DeadlineMonitorTest, DetectsViolation) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(10000);  // 10µs

    TraceEvent start, end;
    start.timestamp_ns = 1000000;
    start.pid = 100;
    strncpy(start.comm, "task", sizeof(start.comm) - 1);

    end = start;
    end.timestamp_ns = 1015000;  // 15µs later (exceeds deadline)

    EXPECT_TRUE(dm.CheckDeadline(start, end));
    ASSERT_EQ(dm.Violations().size(), 1u);
    EXPECT_EQ(dm.Violations()[0].overrun_ns, 5000u);
}

TEST(DeadlineMonitorTest, PerPidDeadline) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(10000);
    dm.SetDeadline(200, 5000);  // PID 200 has tighter deadline

    TraceEvent start, end;
    start.timestamp_ns = 1000000;
    start.pid = 200;
    strncpy(start.comm, "critical", sizeof(start.comm) - 1);

    end = start;
    end.timestamp_ns = 1007000;  // 7µs - violates PID 200's 5µs deadline

    EXPECT_TRUE(dm.CheckDeadline(start, end));
}

TEST(DeadlineMonitorTest, AnalyzeEvents) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(5000);  // 5µs

    std::vector<TraceEvent> events;
    // Create pairs: PID 100 with 3µs (OK), PID 200 with 8µs (violation)
    TraceEvent e1;
    e1.timestamp_ns = 1000000;
    e1.pid = 100;
    strncpy(e1.comm, "fast", sizeof(e1.comm) - 1);
    events.push_back(e1);

    TraceEvent e2 = e1;
    e2.timestamp_ns = 1003000;  // 3µs
    events.push_back(e2);

    TraceEvent e3;
    e3.timestamp_ns = 2000000;
    e3.pid = 200;
    strncpy(e3.comm, "slow", sizeof(e3.comm) - 1);
    events.push_back(e3);

    TraceEvent e4 = e3;
    e4.timestamp_ns = 2008000;  // 8µs
    events.push_back(e4);

    auto violations = dm.AnalyzeEvents(events);
    EXPECT_EQ(violations.size(), 1u);
    EXPECT_EQ(violations[0].pid, 200);
}

TEST(DeadlineMonitorTest, RenderReport) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(1000);

    std::string report = dm.RenderReport();
    EXPECT_NE(report.find("Deadline Violation Report"), std::string::npos);
    EXPECT_NE(report.find("No deadline violations"), std::string::npos);
}

TEST(DeadlineMonitorTest, Reset) {
    DeadlineMonitor dm;
    dm.SetDefaultDeadline(1000);

    TraceEvent start, end;
    start.timestamp_ns = 0;
    start.pid = 1;
    strncpy(start.comm, "t", sizeof(start.comm));
    end = start;
    end.timestamp_ns = 5000;
    dm.CheckDeadline(start, end);

    EXPECT_EQ(dm.Violations().size(), 1u);
    dm.Reset();
    EXPECT_TRUE(dm.Violations().empty());
}

}  // namespace
}  // namespace rtdiag
