#pragma once

/// @file tracer.h
/// @brief Public API for ftrace-based real-time tracing.

#include "rtdiag/types.h"
#include "rtdiag/ring_buffer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtdiag {

/// Callback invoked for each trace event.
using EventCallback = std::function<void(const TraceEvent&)>;

/// FtraceTracer captures kernel trace events via ftrace.
///
/// On systems without ftrace access (non-root or non-Linux), it provides
/// a simulated fallback mode that generates synthetic events for testing.
class FtraceTracer {
public:
    explicit FtraceTracer(const TraceConfig& config = TraceConfig());
    ~FtraceTracer();

    // Non-copyable, movable
    FtraceTracer(const FtraceTracer&) = delete;
    FtraceTracer& operator=(const FtraceTracer&) = delete;
    FtraceTracer(FtraceTracer&&) noexcept;
    FtraceTracer& operator=(FtraceTracer&&) noexcept;

    /// Start tracing. Returns true on success.
    bool Start();

    /// Stop tracing.
    void Stop();

    /// Whether the tracer is currently active.
    bool IsRunning() const;

    /// Read available events into the provided vector.
    /// Returns the number of events read.
    size_t ReadEvents(std::vector<TraceEvent>& events, size_t max_events = 1024);

    /// Register a callback for real-time event processing.
    void SetEventCallback(EventCallback callback);

    /// Get the number of events captured so far.
    uint64_t EventCount() const;

    /// Get the number of events dropped due to overflow.
    uint64_t DroppedEvents() const;

    /// Whether running in simulated mode (no real ftrace access).
    bool IsSimulated() const;

    /// Generate synthetic events for testing/benchmarking.
    /// @param count  Number of events to generate
    void GenerateSyntheticEvents(size_t count);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rtdiag
