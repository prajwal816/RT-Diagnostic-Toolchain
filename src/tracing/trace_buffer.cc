#include "trace_buffer.h"

namespace rtdiag {
namespace tracing {

TraceBuffer::TraceBuffer(size_t capacity, OverflowPolicy policy)
    : ring_(capacity, policy) {}

bool TraceBuffer::Push(const TraceEvent& event) {
    return ring_.Push(event);
}

bool TraceBuffer::Pop(TraceEvent& event) {
    return ring_.Pop(event);
}

size_t TraceBuffer::Drain(std::vector<TraceEvent>& out, size_t max_count) {
    return ring_.Drain(out, max_count);
}

size_t TraceBuffer::DrainFiltered(std::vector<TraceEvent>& out, size_t max_count,
                                   EventFilter filter) {
    size_t drained = 0;
    TraceEvent event;
    while (drained < max_count && ring_.Pop(event)) {
        if (filter(event)) {
            out.push_back(event);
            ++drained;
        }
    }
    return drained;
}

size_t TraceBuffer::Size() const {
    return ring_.Size();
}

bool TraceBuffer::Empty() const {
    return ring_.Empty();
}

uint64_t TraceBuffer::OverflowCount() const {
    return ring_.OverflowCount();
}

void TraceBuffer::Reset() {
    ring_.Reset();
}

}  // namespace tracing
}  // namespace rtdiag
