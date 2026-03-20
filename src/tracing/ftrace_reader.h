#pragma once

/// @file ftrace_reader.h
/// @brief Reads events from the ftrace trace_pipe or simulated sources.

#include "rtdiag/types.h"
#include "rtdiag/ring_buffer.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace rtdiag {
namespace tracing {

/// Callback for each event read from ftrace.
using ReaderCallback = std::function<void(const TraceEvent&)>;

/// Configuration for the ftrace reader.
struct FtraceReaderConfig {
    std::string trace_pipe_path = "/sys/kernel/debug/tracing/trace_pipe";
    uint32_t    buffer_capacity = 1 << 20;  ///< Ring buffer slots
    bool        use_simulated = false;       ///< Force simulated mode
};

/// Reads from the kernel ftrace interface or provides simulated events.
class FtraceReader {
public:
    explicit FtraceReader(const FtraceReaderConfig& config = FtraceReaderConfig());
    ~FtraceReader();

    // Non-copyable
    FtraceReader(const FtraceReader&) = delete;
    FtraceReader& operator=(const FtraceReader&) = delete;

    /// Start reading events in a background thread.
    bool Start();

    /// Stop the reader thread.
    void Stop();

    /// Whether the reader is currently active.
    bool IsRunning() const;

    /// Whether running in simulated mode.
    bool IsSimulated() const { return simulated_; }

    /// Read available events from the internal buffer.
    size_t ReadEvents(std::vector<TraceEvent>& events, size_t max_count = 1024);

    /// Set a callback for real-time event processing.
    void SetCallback(ReaderCallback callback);

    /// Generate synthetic events for testing.
    void GenerateSyntheticEvents(size_t count);

    /// Total events captured.
    uint64_t EventCount() const;

    /// Events dropped due to buffer overflow.
    uint64_t DroppedEvents() const;

private:
    void ReaderLoop();
    void SimulatedLoop();

    FtraceReaderConfig config_;
    std::unique_ptr<RingBuffer<TraceEvent>> buffer_;
    ReaderCallback callback_;
    std::thread reader_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> event_count_;
    bool simulated_;
};

}  // namespace tracing
}  // namespace rtdiag
