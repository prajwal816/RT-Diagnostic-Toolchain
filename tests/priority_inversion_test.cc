#include "rtdiag/sched_analyzer.h"
#include "gtest/gtest.h"

#include <cstring>

namespace rtdiag {
namespace {

TEST(PriorityInversionTest, NoInversion) {
    PriorityInversionDetector detector;

    std::vector<SchedEvent> events;
    SchedEvent se;
    se.timestamp_ns = 1000000;
    se.cpu = 0;
    se.prev_pid = 100;
    se.next_pid = 200;
    se.prev_prio = 50;   // Lower priority (higher number)
    se.next_prio = 10;   // Higher priority (lower number)
    strncpy(se.prev_comm, "low", sizeof(se.prev_comm) - 1);
    strncpy(se.next_comm, "high", sizeof(se.next_comm) - 1);
    events.push_back(se);

    auto inversions = detector.Analyze(events);
    EXPECT_TRUE(inversions.empty());
}

TEST(PriorityInversionTest, DetectsInversion) {
    PriorityInversionDetector detector;

    std::vector<SchedEvent> events;

    // First: high-priority task running
    SchedEvent se1;
    se1.timestamp_ns = 1000000;
    se1.cpu = 0;
    se1.prev_pid = 0;
    se1.next_pid = 100;
    se1.prev_prio = 120;
    se1.next_prio = 10;   // High priority task starts
    strncpy(se1.prev_comm, "idle", sizeof(se1.prev_comm) - 1);
    strncpy(se1.next_comm, "rt_task", sizeof(se1.next_comm) - 1);
    events.push_back(se1);

    // Second: high-priority task preempted by low-priority task (inversion!)
    SchedEvent se2;
    se2.timestamp_ns = 1005000;
    se2.cpu = 0;
    se2.prev_pid = 100;
    se2.next_pid = 200;
    se2.prev_prio = 10;    // High priority being switched out
    se2.next_prio = 50;    // Lower priority taking over
    strncpy(se2.prev_comm, "rt_task", sizeof(se2.prev_comm) - 1);
    strncpy(se2.next_comm, "low_task", sizeof(se2.next_comm) - 1);
    events.push_back(se2);

    auto inversions = detector.Analyze(events);
    ASSERT_GE(inversions.size(), 1u);
    EXPECT_EQ(inversions[0].high_prio_pid, 100);
    EXPECT_EQ(inversions[0].low_prio_pid, 200);
    EXPECT_EQ(inversions[0].high_prio, 10);
    EXPECT_EQ(inversions[0].low_prio, 50);
}

TEST(PriorityInversionTest, RenderReport) {
    PriorityInversionDetector detector;
    std::string report = detector.RenderReport();
    EXPECT_NE(report.find("Priority Inversion Report"), std::string::npos);
    EXPECT_NE(report.find("No priority inversions"), std::string::npos);
}

TEST(PriorityInversionTest, Reset) {
    PriorityInversionDetector detector;

    // Add some inversions
    std::vector<SchedEvent> events;
    SchedEvent se;
    se.timestamp_ns = 1000000;
    se.cpu = 0;
    se.prev_pid = 100;
    se.next_pid = 200;
    se.prev_prio = 5;
    se.next_prio = 80;
    strncpy(se.prev_comm, "high", sizeof(se.prev_comm) - 1);
    strncpy(se.next_comm, "low", sizeof(se.next_comm) - 1);
    events.push_back(se);

    detector.Analyze(events);
    EXPECT_FALSE(detector.Inversions().empty());

    detector.Reset();
    EXPECT_TRUE(detector.Inversions().empty());
}

TEST(PriorityInversionTest, MultiCPU) {
    PriorityInversionDetector detector;

    std::vector<SchedEvent> events;

    // Inversion on CPU 0
    SchedEvent se1;
    se1.timestamp_ns = 1000000;
    se1.cpu = 0;
    se1.prev_pid = 100;
    se1.next_pid = 300;
    se1.prev_prio = 5;
    se1.next_prio = 90;
    strncpy(se1.prev_comm, "rt_a", sizeof(se1.prev_comm) - 1);
    strncpy(se1.next_comm, "bg_a", sizeof(se1.next_comm) - 1);
    events.push_back(se1);

    // Inversion on CPU 1
    SchedEvent se2;
    se2.timestamp_ns = 1000100;
    se2.cpu = 1;
    se2.prev_pid = 200;
    se2.next_pid = 400;
    se2.prev_prio = 10;
    se2.next_prio = 80;
    strncpy(se2.prev_comm, "rt_b", sizeof(se2.prev_comm) - 1);
    strncpy(se2.next_comm, "bg_b", sizeof(se2.next_comm) - 1);
    events.push_back(se2);

    auto inversions = detector.Analyze(events);
    EXPECT_EQ(inversions.size(), 2u);
}

}  // namespace
}  // namespace rtdiag
