#include "rtdiag/sched_analyzer.h"
#include "src/utils/logger.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <iomanip>

namespace rtdiag {

// ============================================================================
// DeadlineMonitor Implementation
// ============================================================================

struct DeadlineMonitor::Impl {
    std::map<int32_t, uint64_t> deadlines;       // pid -> deadline_ns
    uint64_t default_deadline_ns = 0;
    std::vector<DeadlineViolation> violations;

    // Track last event per PID for pairing start/end
    std::map<int32_t, TraceEvent> last_event;
};

DeadlineMonitor::DeadlineMonitor() : impl_(std::make_unique<Impl>()) {}
DeadlineMonitor::~DeadlineMonitor() = default;

void DeadlineMonitor::SetDeadline(int32_t pid, uint64_t deadline_ns) {
    impl_->deadlines[pid] = deadline_ns;
}

void DeadlineMonitor::SetDefaultDeadline(uint64_t deadline_ns) {
    impl_->default_deadline_ns = deadline_ns;
}

bool DeadlineMonitor::CheckDeadline(const TraceEvent& start, const TraceEvent& end) {
    uint64_t elapsed = end.timestamp_ns - start.timestamp_ns;

    // Look up deadline
    uint64_t deadline = impl_->default_deadline_ns;
    auto it = impl_->deadlines.find(start.pid);
    if (it != impl_->deadlines.end()) {
        deadline = it->second;
    }

    if (deadline == 0) return false;  // No deadline configured

    if (elapsed > deadline) {
        DeadlineViolation violation;
        violation.timestamp_ns = start.timestamp_ns;
        violation.pid = start.pid;
        violation.deadline_ns = deadline;
        violation.actual_ns = elapsed;
        violation.overrun_ns = elapsed - deadline;
        strncpy(violation.comm, start.comm, sizeof(violation.comm) - 1);
        violation.comm[sizeof(violation.comm) - 1] = '\0';

        impl_->violations.push_back(violation);
        RTDIAG_LOG_WARN("Deadline violation: PID %d (%s) took %lu ns, deadline %lu ns",
                        start.pid, start.comm, elapsed, deadline);
        return true;
    }
    return false;
}

std::vector<DeadlineViolation> DeadlineMonitor::AnalyzeEvents(
    const std::vector<TraceEvent>& events) {
    impl_->violations.clear();
    impl_->last_event.clear();

    for (const auto& event : events) {
        // Pair events: function entry/exit or sched_switch pairs
        auto it = impl_->last_event.find(event.pid);
        if (it != impl_->last_event.end()) {
            if (event.timestamp_ns > it->second.timestamp_ns) {
                CheckDeadline(it->second, event);
            }
            impl_->last_event.erase(it);
        } else {
            impl_->last_event[event.pid] = event;
        }
    }

    return impl_->violations;
}

const std::vector<DeadlineViolation>& DeadlineMonitor::Violations() const {
    return impl_->violations;
}

std::string DeadlineMonitor::RenderReport() const {
    std::ostringstream oss;
    oss << "=== Deadline Violation Report ===" << std::endl;
    oss << "Total violations: " << impl_->violations.size() << std::endl;
    oss << std::endl;

    if (impl_->violations.empty()) {
        oss << "No deadline violations detected." << std::endl;
        return oss.str();
    }

    oss << std::left
        << std::setw(8) << "PID"
        << std::setw(16) << "Task"
        << std::setw(16) << "Deadline(us)"
        << std::setw(16) << "Actual(us)"
        << std::setw(16) << "Overrun(us)"
        << std::endl;
    oss << std::string(72, '-') << std::endl;

    for (const auto& v : impl_->violations) {
        oss << std::left
            << std::setw(8) << v.pid
            << std::setw(16) << v.comm
            << std::setw(16) << std::fixed << std::setprecision(2)
            << (v.deadline_ns / 1000.0)
            << std::setw(16) << (v.actual_ns / 1000.0)
            << std::setw(16) << (v.overrun_ns / 1000.0)
            << std::endl;
    }

    return oss.str();
}

void DeadlineMonitor::Reset() {
    impl_->violations.clear();
    impl_->last_event.clear();
    impl_->deadlines.clear();
    impl_->default_deadline_ns = 0;
}

// ============================================================================
// PriorityInversionDetector Implementation
// ============================================================================

struct PriorityInversionDetector::Impl {
    std::vector<PriorityInversion> inversions;

    // Track which PIDs are currently running on each CPU
    struct CPUState {
        int32_t running_pid = 0;
        int32_t running_prio = 0;
        char    running_comm[16] = {};
        uint64_t start_ns = 0;
    };
    std::map<uint32_t, CPUState> cpu_states;
    std::map<int32_t, int32_t> waiting_pids;  // pid -> priority of waiting task
};

PriorityInversionDetector::PriorityInversionDetector()
    : impl_(std::make_unique<Impl>()) {}

PriorityInversionDetector::~PriorityInversionDetector() = default;

std::vector<PriorityInversion> PriorityInversionDetector::Analyze(
    const std::vector<SchedEvent>& events) {
    impl_->inversions.clear();
    impl_->cpu_states.clear();

    for (const auto& event : events) {
        auto& state = impl_->cpu_states[event.cpu];

        // Check for priority inversion:
        // A lower-priority task (next) is running while a higher-priority
        // task (prev) is being switched out and has higher priority
        // (lower priority number in RT = higher actual priority)
        if (event.prev_prio > 0 && event.next_prio > 0) {
            // In Linux RT scheduling: lower number = higher priority
            // Priority inversion: higher-priority task preempted by lower-priority
            if (event.prev_prio < event.next_prio) {
                // prev has higher priority but is being switched out
                // This indicates the high-priority task might be blocked
                PriorityInversion inv;
                inv.timestamp_ns = event.timestamp_ns;
                inv.high_prio_pid = event.prev_pid;
                inv.low_prio_pid = event.next_pid;
                inv.high_prio = event.prev_prio;
                inv.low_prio = event.next_prio;

                strncpy(inv.high_comm, event.prev_comm,
                        sizeof(inv.high_comm) - 1);
                inv.high_comm[sizeof(inv.high_comm) - 1] = '\0';
                strncpy(inv.low_comm, event.next_comm,
                        sizeof(inv.low_comm) - 1);
                inv.low_comm[sizeof(inv.low_comm) - 1] = '\0';

                // Calculate duration from state tracking
                if (state.running_pid == event.prev_pid && state.start_ns > 0) {
                    inv.duration_ns = event.timestamp_ns - state.start_ns;
                }

                impl_->inversions.push_back(inv);
                RTDIAG_LOG_WARN("Priority inversion: PID %d (prio %d) blocked by PID %d (prio %d)",
                                event.prev_pid, event.prev_prio,
                                event.next_pid, event.next_prio);
            }
        }

        // Update CPU state
        state.running_pid = event.next_pid;
        state.running_prio = event.next_prio;
        strncpy(state.running_comm, event.next_comm,
                sizeof(state.running_comm) - 1);
        state.running_comm[sizeof(state.running_comm) - 1] = '\0';
        state.start_ns = event.timestamp_ns;
    }

    return impl_->inversions;
}

std::vector<PriorityInversion> PriorityInversionDetector::AnalyzeTraceEvents(
    const std::vector<TraceEvent>& events) {
    // Convert relevant trace events to sched events
    std::vector<SchedEvent> sched_events;
    sched_events.reserve(events.size() / 2);  // Not all will be sched events

    for (size_t i = 0; i + 1 < events.size(); ++i) {
        if (events[i].type == EventType::kSchedSwitch) {
            SchedEvent se;
            se.timestamp_ns = events[i].timestamp_ns;
            se.cpu = events[i].cpu;
            se.prev_pid = events[i].pid;
            se.prev_prio = events[i].priority;
            strncpy(se.prev_comm, events[i].comm, sizeof(se.prev_comm) - 1);
            se.prev_comm[sizeof(se.prev_comm) - 1] = '\0';

            // Use the next event as the "next" task info
            se.next_pid = events[i + 1].pid;
            se.next_prio = events[i + 1].priority;
            strncpy(se.next_comm, events[i + 1].comm,
                    sizeof(se.next_comm) - 1);
            se.next_comm[sizeof(se.next_comm) - 1] = '\0';

            sched_events.push_back(se);
        }
    }

    return Analyze(sched_events);
}

const std::vector<PriorityInversion>& PriorityInversionDetector::Inversions() const {
    return impl_->inversions;
}

std::string PriorityInversionDetector::RenderReport() const {
    std::ostringstream oss;
    oss << "=== Priority Inversion Report ===" << std::endl;
    oss << "Total inversions detected: " << impl_->inversions.size() << std::endl;
    oss << std::endl;

    if (impl_->inversions.empty()) {
        oss << "No priority inversions detected." << std::endl;
        return oss.str();
    }

    oss << std::left
        << std::setw(18) << "Timestamp(us)"
        << std::setw(12) << "Hi-PID"
        << std::setw(12) << "Hi-Task"
        << std::setw(10) << "Hi-Prio"
        << std::setw(12) << "Lo-PID"
        << std::setw(12) << "Lo-Task"
        << std::setw(10) << "Lo-Prio"
        << std::setw(14) << "Duration(us)"
        << std::endl;
    oss << std::string(100, '-') << std::endl;

    for (const auto& inv : impl_->inversions) {
        oss << std::left
            << std::setw(18) << std::fixed << std::setprecision(2)
            << (inv.timestamp_ns / 1000.0)
            << std::setw(12) << inv.high_prio_pid
            << std::setw(12) << inv.high_comm
            << std::setw(10) << inv.high_prio
            << std::setw(12) << inv.low_prio_pid
            << std::setw(12) << inv.low_comm
            << std::setw(10) << inv.low_prio
            << std::setw(14) << (inv.duration_ns / 1000.0)
            << std::endl;
    }

    return oss.str();
}

void PriorityInversionDetector::Reset() {
    impl_->inversions.clear();
    impl_->cpu_states.clear();
    impl_->waiting_pids.clear();
}

}  // namespace rtdiag
