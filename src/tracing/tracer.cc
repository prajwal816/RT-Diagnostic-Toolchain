#include "rtdiag/tracer.h"
#include "src/tracing/ftrace_reader.h"
#include "src/utils/logger.h"

namespace rtdiag {

struct FtraceTracer::Impl {
    std::unique_ptr<tracing::FtraceReader> reader;
    TraceConfig config;
    uint64_t event_count = 0;

    explicit Impl(const TraceConfig& cfg)
        : config(cfg) {
        tracing::FtraceReaderConfig rcfg;
        rcfg.buffer_capacity = cfg.ring_buffer_capacity;
        reader = std::make_unique<tracing::FtraceReader>(rcfg);
    }
};

FtraceTracer::FtraceTracer(const TraceConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

FtraceTracer::~FtraceTracer() = default;

FtraceTracer::FtraceTracer(FtraceTracer&&) noexcept = default;
FtraceTracer& FtraceTracer::operator=(FtraceTracer&&) noexcept = default;

bool FtraceTracer::Start() {
    return impl_->reader->Start();
}

void FtraceTracer::Stop() {
    impl_->reader->Stop();
}

bool FtraceTracer::IsRunning() const {
    return impl_->reader->IsRunning();
}

size_t FtraceTracer::ReadEvents(std::vector<TraceEvent>& events, size_t max_events) {
    size_t count = impl_->reader->ReadEvents(events, max_events);
    impl_->event_count += count;
    return count;
}

void FtraceTracer::SetEventCallback(EventCallback callback) {
    impl_->reader->SetCallback(std::move(callback));
}

uint64_t FtraceTracer::EventCount() const {
    return impl_->reader->EventCount();
}

uint64_t FtraceTracer::DroppedEvents() const {
    return impl_->reader->DroppedEvents();
}

bool FtraceTracer::IsSimulated() const {
    return impl_->reader->IsSimulated();
}

void FtraceTracer::GenerateSyntheticEvents(size_t count) {
    impl_->reader->GenerateSyntheticEvents(count);
}

}  // namespace rtdiag
