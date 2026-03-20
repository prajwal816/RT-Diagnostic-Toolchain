#pragma once

/// @file sched_analyzer.h
/// @brief Public API for deadline monitoring and priority inversion detection.

#include "rtdiag/types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rtdiag {

/// Monitors task execution against configured deadlines.
class DeadlineMonitor {
public:
    DeadlineMonitor();
    ~DeadlineMonitor();

    /// Set the deadline for a specific task (by PID).
    /// @param pid          Process ID to monitor
    /// @param deadline_ns  Deadline in nanoseconds
    void SetDeadline(int32_t pid, uint64_t deadline_ns);

    /// Set a global default deadline for all tasks.
    void SetDefaultDeadline(uint64_t deadline_ns);

    /// Process a pair of events (start/end) to check deadline compliance.
    /// @param start  Event marking the start of a timed section
    /// @param end    Event marking the end
    /// @return true if a violation was detected
    bool CheckDeadline(const TraceEvent& start, const TraceEvent& end);

    /// Process a list of events and detect all deadline violations.
    std::vector<DeadlineViolation> AnalyzeEvents(
        const std::vector<TraceEvent>& events);

    /// Get all recorded violations.
    const std::vector<DeadlineViolation>& Violations() const;

    /// Generate a summary report of all violations.
    std::string RenderReport() const;

    /// Reset all state.
    void Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Detects priority inversion scenarios in scheduling data.
class PriorityInversionDetector {
public:
    PriorityInversionDetector();
    ~PriorityInversionDetector();

    /// Analyze a sequence of scheduling events for inversions.
    std::vector<PriorityInversion> Analyze(
        const std::vector<SchedEvent>& events);

    /// Analyze raw trace events (will extract sched events internally).
    std::vector<PriorityInversion> AnalyzeTraceEvents(
        const std::vector<TraceEvent>& events);

    /// Get all detected inversions.
    const std::vector<PriorityInversion>& Inversions() const;

    /// Generate a summary report.
    std::string RenderReport() const;

    /// Reset all state.
    void Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rtdiag
