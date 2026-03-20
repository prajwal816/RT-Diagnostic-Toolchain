#include "event_parser.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <sstream>

namespace rtdiag {
namespace tracing {

// Example ftrace line formats:
//   <idle>-0     [001] d..1  1234.567890: sched_switch: prev_comm=idle ...
//   task-1234    [002] d.h.  1234.567891: irq_handler_entry: irq=42 name=eth0
//   app-5678     [000] ....  1234.567892: funcgraph_entry: 0.123 us | func()

EventType ClassifyEvent(const std::string& line) {
    if (line.find("sched_switch:") != std::string::npos)
        return EventType::kSchedSwitch;
    if (line.find("sched_wakeup:") != std::string::npos)
        return EventType::kSchedWakeup;
    if (line.find("irq_handler_entry:") != std::string::npos ||
        line.find("irq_entry:") != std::string::npos)
        return EventType::kIrqEntry;
    if (line.find("irq_handler_exit:") != std::string::npos ||
        line.find("irq_exit:") != std::string::npos)
        return EventType::kIrqExit;
    if (line.find("softirq_entry:") != std::string::npos)
        return EventType::kSoftirqEntry;
    if (line.find("softirq_exit:") != std::string::npos)
        return EventType::kSoftirqExit;
    if (line.find("funcgraph_entry:") != std::string::npos)
        return EventType::kFunctionEntry;
    if (line.find("funcgraph_exit:") != std::string::npos)
        return EventType::kFunctionExit;
    return EventType::kUnknown;
}

/// Helper: extract the CPU number from "[NNN]" in an ftrace line.
static int ExtractCPU(const std::string& line) {
    auto pos = line.find('[');
    if (pos == std::string::npos) return -1;
    auto end_pos = line.find(']', pos);
    if (end_pos == std::string::npos) return -1;
    std::string cpu_str = line.substr(pos + 1, end_pos - pos - 1);
    // Remove leading whitespace
    cpu_str.erase(0, cpu_str.find_first_not_of(' '));
    return std::atoi(cpu_str.c_str());
}

/// Helper: extract the timestamp (seconds.microseconds) from ftrace line.
static uint64_t ExtractTimestamp(const std::string& line) {
    // Find the pattern: digits.digits followed by ':'
    auto bracket_end = line.find(']');
    if (bracket_end == std::string::npos) return 0;

    auto colon_pos = line.find(':', bracket_end);
    if (colon_pos == std::string::npos) return 0;

    // Scan backwards from colon to find timestamp start
    auto ts_end = colon_pos;
    auto ts_start = ts_end;
    while (ts_start > 0 && (std::isdigit(line[ts_start - 1]) || line[ts_start - 1] == '.')) {
        --ts_start;
    }

    std::string ts_str = line.substr(ts_start, ts_end - ts_start);
    auto dot_pos = ts_str.find('.');
    if (dot_pos == std::string::npos) return 0;

    uint64_t seconds = std::strtoull(ts_str.substr(0, dot_pos).c_str(), nullptr, 10);
    std::string frac_str = ts_str.substr(dot_pos + 1);
    // Pad or truncate to 9 digits (nanoseconds)
    while (frac_str.size() < 9) frac_str += '0';
    frac_str = frac_str.substr(0, 9);
    uint64_t nanos = std::strtoull(frac_str.c_str(), nullptr, 10);

    return seconds * 1000000000ULL + nanos;
}

/// Helper: extract comm and PID from the beginning of an ftrace line.
/// Format: "comm-PID" or "<idle>-0"
static void ExtractCommPid(const std::string& line, char* comm, size_t comm_size, int32_t& pid) {
    auto dash_pos = line.find('-');
    if (dash_pos == std::string::npos || dash_pos == 0) {
        comm[0] = '\0';
        pid = 0;
        return;
    }

    // Find the real dash (after removing leading spaces)
    auto start = line.find_first_not_of(' ');
    if (start == std::string::npos) {
        comm[0] = '\0';
        pid = 0;
        return;
    }

    // Find dash between comm and PID
    auto line_trimmed = line.substr(start);
    dash_pos = line_trimmed.rfind('-', line_trimmed.find('['));
    if (dash_pos == std::string::npos) {
        comm[0] = '\0';
        pid = 0;
        return;
    }

    std::string comm_str = line_trimmed.substr(0, dash_pos);
    // Remove leading/trailing whitespace from comm
    auto first = comm_str.find_first_not_of(' ');
    auto last = comm_str.find_last_not_of(' ');
    if (first != std::string::npos) {
        comm_str = comm_str.substr(first, last - first + 1);
    }

    strncpy(comm, comm_str.c_str(), comm_size - 1);
    comm[comm_size - 1] = '\0';

    // Extract PID
    auto pid_start = dash_pos + 1;
    auto pid_end = line_trimmed.find(' ', pid_start);
    if (pid_end == std::string::npos) pid_end = line_trimmed.size();
    std::string pid_str = line_trimmed.substr(pid_start, pid_end - pid_start);
    pid = std::atoi(pid_str.c_str());
}

bool ParseFtraceLine(const std::string& line, TraceEvent& event) {
    if (line.empty() || line[0] == '#') return false;

    event = TraceEvent();
    event.type = ClassifyEvent(line);

    int cpu = ExtractCPU(line);
    if (cpu >= 0) event.cpu = static_cast<uint32_t>(cpu);

    event.timestamp_ns = ExtractTimestamp(line);
    ExtractCommPid(line, event.comm, sizeof(event.comm), event.pid);
    event.tid = event.pid;  // ftrace doesn't always distinguish

    // Copy the event-specific part into details
    auto detail_pos = line.find(": ", line.find(']'));
    if (detail_pos != std::string::npos) {
        detail_pos += 2;  // Skip ": "
        // Skip the timestamp field
        auto second_colon = line.find(": ", detail_pos);
        if (second_colon != std::string::npos) {
            std::string details = line.substr(second_colon + 2);
            strncpy(event.details, details.c_str(), sizeof(event.details) - 1);
            event.details[sizeof(event.details) - 1] = '\0';
        }
    }

    return event.timestamp_ns > 0;
}

bool ParseSchedSwitch(const std::string& line, SchedEvent& event) {
    // Format: sched_switch: prev_comm=X prev_pid=N prev_prio=N prev_state=S ==>
    //         next_comm=Y next_pid=N next_prio=N
    event = SchedEvent();
    event.timestamp_ns = ExtractTimestamp(line);

    int cpu = ExtractCPU(line);
    if (cpu >= 0) event.cpu = static_cast<uint32_t>(cpu);

    auto extract_field = [&](const std::string& field_name) -> std::string {
        auto pos = line.find(field_name + "=");
        if (pos == std::string::npos) return "";
        pos += field_name.size() + 1;
        auto end = line.find(' ', pos);
        if (end == std::string::npos) end = line.size();
        return line.substr(pos, end - pos);
    };

    std::string prev_comm = extract_field("prev_comm");
    strncpy(event.prev_comm, prev_comm.c_str(), sizeof(event.prev_comm) - 1);
    event.prev_comm[sizeof(event.prev_comm) - 1] = '\0';

    std::string next_comm = extract_field("next_comm");
    strncpy(event.next_comm, next_comm.c_str(), sizeof(event.next_comm) - 1);
    event.next_comm[sizeof(event.next_comm) - 1] = '\0';

    std::string prev_pid_str = extract_field("prev_pid");
    if (!prev_pid_str.empty()) event.prev_pid = std::atoi(prev_pid_str.c_str());

    std::string next_pid_str = extract_field("next_pid");
    if (!next_pid_str.empty()) event.next_pid = std::atoi(next_pid_str.c_str());

    std::string prev_prio_str = extract_field("prev_prio");
    if (!prev_prio_str.empty()) event.prev_prio = std::atoi(prev_prio_str.c_str());

    std::string next_prio_str = extract_field("next_prio");
    if (!next_prio_str.empty()) event.next_prio = std::atoi(next_prio_str.c_str());

    return event.timestamp_ns > 0;
}

std::vector<TraceEvent> ParseFtraceOutput(const std::vector<std::string>& lines) {
    std::vector<TraceEvent> events;
    events.reserve(lines.size());

    for (const auto& line : lines) {
        TraceEvent event;
        if (ParseFtraceLine(line, event)) {
            events.push_back(event);
        }
    }

    return events;
}

}  // namespace tracing
}  // namespace rtdiag
