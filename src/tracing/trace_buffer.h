#pragma once

/// @file trace_buffer.h
/// @brief High-throughput trace event buffer with drain and filtering.

#include "rtdiag/types.h"
#include "rtdiag/ring_buffer.h"

#include <functional>
#include <vector>

namespace rtdiag {
namespace tracing {

/// Predicate for filtering events.
using EventFilter = std::function<bool(const TraceEvent&)>;

/// Manages a ring buffer of trace events with filtering and batch operations.
class TraceBuffer {
public:
    /// @param capacity  Number of event slots
    /// @param policy    Overflow policy
    explicit TraceBuffer(size_t capacity = 1 << 20,
                         OverflowPolicy policy = OverflowPolicy::kDrop);

    /// Push an event into the buffer.
    bool Push(const TraceEvent& event);

    /// Pop a single event.
    bool Pop(TraceEvent& event);

    /// Drain up to max_count events.
    size_t Drain(std::vector<TraceEvent>& out, size_t max_count);

    /// Drain events that match a filter.
    size_t DrainFiltered(std::vector<TraceEvent>& out, size_t max_count,
                         EventFilter filter);

    /// Current number of events in buffer.
    size_t Size() const;

    /// Whether the buffer is empty.
    bool Empty() const;

    /// Total overflow count.
    uint64_t OverflowCount() const;

    /// Reset the buffer.
    void Reset();

private:
    RingBuffer<TraceEvent> ring_;
};

}  // namespace tracing
}  // namespace rtdiag
