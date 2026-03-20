#include "src/tracing/event_parser.h"
#include "gtest/gtest.h"

namespace rtdiag {
namespace tracing {
namespace {

TEST(EventParserTest, ClassifySchedSwitch) {
    std::string line = "  task-1234  [001] d..1  1000.000100: sched_switch: "
                       "prev_comm=task prev_pid=1234 prev_prio=50 prev_state=S "
                       "==> next_comm=idle next_pid=0 next_prio=120";
    EXPECT_EQ(ClassifyEvent(line), EventType::kSchedSwitch);
}

TEST(EventParserTest, ClassifyIrqEntry) {
    std::string line = "  <idle>-0    [002] d.h.  1000.000200: irq_handler_entry: "
                       "irq=42 name=eth0";
    EXPECT_EQ(ClassifyEvent(line), EventType::kIrqEntry);
}

TEST(EventParserTest, ClassifySoftirqEntry) {
    std::string line = "  ksoftirq-10 [000] ..s.  1000.000300: softirq_entry: vec=1";
    EXPECT_EQ(ClassifyEvent(line), EventType::kSoftirqEntry);
}

TEST(EventParserTest, ClassifyUnknown) {
    std::string line = "some random text";
    EXPECT_EQ(ClassifyEvent(line), EventType::kUnknown);
}

TEST(EventParserTest, ParseTimestamp) {
    std::string line = "  task-100   [000] ....  1234.567890123: sched_switch: details";
    TraceEvent event;
    EXPECT_TRUE(ParseFtraceLine(line, event));
    EXPECT_EQ(event.timestamp_ns, 1234567890123ULL);
}

TEST(EventParserTest, ParseCPU) {
    std::string line = "  task-100   [003] ....  1000.000001: sched_switch: details";
    TraceEvent event;
    EXPECT_TRUE(ParseFtraceLine(line, event));
    EXPECT_EQ(event.cpu, 3u);
}

TEST(EventParserTest, ParseSchedSwitchLine) {
    std::string line =
        "  worker-500 [001] d..2  2000.000100: sched_switch: "
        "prev_comm=worker prev_pid=500 prev_prio=20 prev_state=R "
        "==> next_comm=rt_task next_pid=300 next_prio=5";

    SchedEvent event;
    EXPECT_TRUE(ParseSchedSwitch(line, event));
    EXPECT_EQ(event.prev_pid, 500);
    EXPECT_EQ(event.next_pid, 300);
    EXPECT_EQ(event.prev_prio, 20);
    EXPECT_EQ(event.next_prio, 5);
    EXPECT_STREQ(event.prev_comm, "worker");
    EXPECT_STREQ(event.next_comm, "rt_task");
}

TEST(EventParserTest, SkipComments) {
    std::string line = "# tracer: nop";
    TraceEvent event;
    EXPECT_FALSE(ParseFtraceLine(line, event));
}

TEST(EventParserTest, ParseMultipleLines) {
    std::vector<std::string> lines = {
        "# header",
        "  task-100 [000] .... 1000.000001: sched_switch: data",
        "  task-200 [001] .... 1000.000002: irq_handler_entry: irq=1",
        "",
        "  task-300 [002] .... 1000.000003: softirq_entry: vec=1",
    };

    auto events = ParseFtraceOutput(lines);
    EXPECT_EQ(events.size(), 3u);
}

}  // namespace
}  // namespace tracing
}  // namespace rtdiag
