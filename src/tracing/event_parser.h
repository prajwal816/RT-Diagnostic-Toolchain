#pragma once

/// @file event_parser.h
/// @brief Parser for raw ftrace text output into structured TraceEvent records.

#include "rtdiag/types.h"

#include <string>
#include <vector>

namespace rtdiag {
namespace tracing {

/// Parse a single line of ftrace output into a TraceEvent.
/// @param line     Raw ftrace text line
/// @param[out] event  Parsed event
/// @return true if parsing succeeded
bool ParseFtraceLine(const std::string& line, TraceEvent& event);

/// Parse a sched_switch line into a SchedEvent.
/// @param line     Raw ftrace sched_switch line
/// @param[out] event  Parsed scheduling event
/// @return true if parsing succeeded
bool ParseSchedSwitch(const std::string& line, SchedEvent& event);

/// Parse multiple ftrace lines.
/// @param lines  Vector of raw ftrace lines
/// @return Vector of successfully parsed events
std::vector<TraceEvent> ParseFtraceOutput(const std::vector<std::string>& lines);

/// Classify an ftrace line into its EventType.
EventType ClassifyEvent(const std::string& line);

}  // namespace tracing
}  // namespace rtdiag
