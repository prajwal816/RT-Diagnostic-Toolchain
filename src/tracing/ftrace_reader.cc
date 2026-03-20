#include "ftrace_reader.h"
#include "event_parser.h"
#include "src/utils/logger.h"
#include "src/utils/timestamp.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <random>

namespace rtdiag {
namespace tracing {

FtraceReader::FtraceReader(const FtraceReaderConfig& config)
    : config_(config),
      running_(false),
      event_count_(0),
      simulated_(config.use_simulated) {
    buffer_ = std::make_unique<RingBuffer<TraceEvent>>(
        config.buffer_capacity, OverflowPolicy::kDrop);

    // Auto-detect: if we can't open trace_pipe, switch to simulated
    if (!simulated_) {
        std::ifstream test_file(config_.trace_pipe_path);
        if (!test_file.good()) {
            RTDIAG_LOG_WARN("Cannot access %s, using simulated mode",
                           config_.trace_pipe_path.c_str());
            simulated_ = true;
        }
    }
}

FtraceReader::~FtraceReader() {
    Stop();
}

bool FtraceReader::Start() {
    if (running_.load()) return false;
    running_.store(true);

    if (simulated_) {
        reader_thread_ = std::thread(&FtraceReader::SimulatedLoop, this);
    } else {
        reader_thread_ = std::thread(&FtraceReader::ReaderLoop, this);
    }

    RTDIAG_LOG_INFO("FtraceReader started (%s mode)",
                    simulated_ ? "simulated" : "live");
    return true;
}

void FtraceReader::Stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    RTDIAG_LOG_INFO("FtraceReader stopped. Total events: %lu",
                    event_count_.load());
}

bool FtraceReader::IsRunning() const {
    return running_.load();
}

size_t FtraceReader::ReadEvents(std::vector<TraceEvent>& events, size_t max_count) {
    return buffer_->Drain(events, max_count);
}

void FtraceReader::SetCallback(ReaderCallback callback) {
    callback_ = std::move(callback);
}

void FtraceReader::GenerateSyntheticEvents(size_t count) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> cpu_dist(0, 7);
    std::uniform_int_distribution<int32_t> pid_dist(100, 9999);
    std::uniform_int_distribution<int32_t> prio_dist(1, 99);
    std::normal_distribution<double> latency_dist(5000.0, 2000.0);

    const char* comms[] = {"worker", "rt_task", "irq_handler", "softirq",
                           "kthread", "main", "sensor", "actuator"};
    const int num_comms = sizeof(comms) / sizeof(comms[0]);

    uint64_t base_ns = utils::MonotonicNanos();

    for (size_t i = 0; i < count; ++i) {
        TraceEvent event;
        event.timestamp_ns = base_ns + static_cast<uint64_t>(i * 1000);
        event.cpu = cpu_dist(rng);
        event.pid = pid_dist(rng);
        event.tid = event.pid;
        event.priority = prio_dist(rng);

        // Rotate through event types
        event.type = static_cast<EventType>(i % 6);

        const char* comm = comms[i % num_comms];
        strncpy(event.comm, comm, sizeof(event.comm) - 1);
        event.comm[sizeof(event.comm) - 1] = '\0';

        snprintf(event.details, sizeof(event.details),
                "latency=%.0fns cpu=%u", std::abs(latency_dist(rng)), event.cpu);

        buffer_->Push(event);
        event_count_.fetch_add(1, std::memory_order_relaxed);

        if (callback_) {
            callback_(event);
        }
    }
}

uint64_t FtraceReader::EventCount() const {
    return event_count_.load(std::memory_order_relaxed);
}

uint64_t FtraceReader::DroppedEvents() const {
    return buffer_->OverflowCount();
}

void FtraceReader::ReaderLoop() {
    std::ifstream trace_pipe(config_.trace_pipe_path);
    if (!trace_pipe.is_open()) {
        RTDIAG_LOG_ERROR("Failed to open %s", config_.trace_pipe_path.c_str());
        running_.store(false);
        return;
    }

    std::string line;
    while (running_.load() && std::getline(trace_pipe, line)) {
        TraceEvent event;
        if (ParseFtraceLine(line, event)) {
            buffer_->Push(event);
            event_count_.fetch_add(1, std::memory_order_relaxed);

            if (callback_) {
                callback_(event);
            }
        }
    }
}

void FtraceReader::SimulatedLoop() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> cpu_dist(0, 7);
    std::uniform_int_distribution<int32_t> pid_dist(100, 9999);
    std::normal_distribution<double> interval_us(10.0, 3.0);

    uint64_t ts = utils::MonotonicNanos();

    while (running_.load()) {
        TraceEvent event;
        event.timestamp_ns = ts;
        event.cpu = cpu_dist(rng);
        event.pid = pid_dist(rng);
        event.tid = event.pid;
        event.priority = static_cast<int32_t>(rng() % 99 + 1);
        event.type = static_cast<EventType>(rng() % 6);
        strncpy(event.comm, "sim_task", sizeof(event.comm) - 1);

        buffer_->Push(event);
        event_count_.fetch_add(1, std::memory_order_relaxed);

        if (callback_) {
            callback_(event);
        }

        // Advance by a random interval
        double interval = std::max(1.0, interval_us(rng));
        ts += static_cast<uint64_t>(interval * 1000.0);

        // Small sleep to avoid burning CPU in simulated mode
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

}  // namespace tracing
}  // namespace rtdiag
