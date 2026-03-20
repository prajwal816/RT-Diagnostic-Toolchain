#include "rtdiag/ring_buffer.h"
#include "rtdiag/types.h"
#include "gtest/gtest.h"

#include <thread>
#include <vector>

namespace rtdiag {
namespace {

TEST(RingBufferTest, BasicPushPop) {
    RingBuffer<TraceEvent> rb(16);
    TraceEvent event;
    event.timestamp_ns = 42;
    event.pid = 100;

    EXPECT_TRUE(rb.Push(event));
    EXPECT_EQ(rb.Size(), 1u);
    EXPECT_FALSE(rb.Empty());

    TraceEvent out;
    EXPECT_TRUE(rb.Pop(out));
    EXPECT_EQ(out.timestamp_ns, 42u);
    EXPECT_EQ(out.pid, 100);
    EXPECT_TRUE(rb.Empty());
}

TEST(RingBufferTest, EmptyPop) {
    RingBuffer<TraceEvent> rb(16);
    TraceEvent out;
    EXPECT_FALSE(rb.Pop(out));
}

TEST(RingBufferTest, FillAndDrain) {
    const size_t capacity = 64;
    RingBuffer<TraceEvent> rb(capacity);

    // Fill to capacity
    for (size_t i = 0; i < capacity; ++i) {
        TraceEvent event;
        event.timestamp_ns = i;
        EXPECT_TRUE(rb.Push(event));
    }

    EXPECT_TRUE(rb.Full());

    // Drain all
    for (size_t i = 0; i < capacity; ++i) {
        TraceEvent out;
        EXPECT_TRUE(rb.Pop(out));
        EXPECT_EQ(out.timestamp_ns, i);
    }

    EXPECT_TRUE(rb.Empty());
}

TEST(RingBufferTest, DropOverflow) {
    RingBuffer<TraceEvent> rb(4, OverflowPolicy::kDrop);

    for (int i = 0; i < 4; ++i) {
        TraceEvent e;
        e.timestamp_ns = i;
        EXPECT_TRUE(rb.Push(e));
    }

    // Buffer full, should drop
    TraceEvent extra;
    extra.timestamp_ns = 999;
    EXPECT_FALSE(rb.Push(extra));
    EXPECT_EQ(rb.OverflowCount(), 1u);
}

TEST(RingBufferTest, OverwriteOverflow) {
    RingBuffer<TraceEvent> rb(4, OverflowPolicy::kOverwrite);

    for (int i = 0; i < 4; ++i) {
        TraceEvent e;
        e.timestamp_ns = i;
        EXPECT_TRUE(rb.Push(e));
    }

    // Should overwrite oldest
    TraceEvent extra;
    extra.timestamp_ns = 999;
    EXPECT_TRUE(rb.Push(extra));
    EXPECT_EQ(rb.OverflowCount(), 1u);
}

TEST(RingBufferTest, PowerOfTwoRounding) {
    RingBuffer<TraceEvent> rb(10);  // Should round to 16
    EXPECT_EQ(rb.Capacity(), 16u);
}

TEST(RingBufferTest, DrainBatch) {
    RingBuffer<TraceEvent> rb(32);

    for (int i = 0; i < 20; ++i) {
        TraceEvent e;
        e.timestamp_ns = i;
        rb.Push(e);
    }

    std::vector<TraceEvent> out;
    size_t drained = rb.Drain(out, 10);
    EXPECT_EQ(drained, 10u);
    EXPECT_EQ(out.size(), 10u);
    EXPECT_EQ(out[0].timestamp_ns, 0u);
    EXPECT_EQ(out[9].timestamp_ns, 9u);

    // 10 remaining
    EXPECT_EQ(rb.Size(), 10u);
}

TEST(RingBufferTest, Reset) {
    RingBuffer<TraceEvent> rb(16);
    TraceEvent e;
    e.timestamp_ns = 1;
    rb.Push(e);
    EXPECT_FALSE(rb.Empty());

    rb.Reset();
    EXPECT_TRUE(rb.Empty());
    EXPECT_EQ(rb.Size(), 0u);
}

TEST(RingBufferTest, SPSCConcurrency) {
    const size_t NUM = 100000;
    RingBuffer<TraceEvent> rb(1024);

    std::thread producer([&]() {
        for (size_t i = 0; i < NUM; ++i) {
            TraceEvent e;
            e.timestamp_ns = i;
            while (!rb.Push(e)) {
                std::this_thread::yield();
            }
        }
    });

    size_t consumed = 0;
    uint64_t last_ts = 0;
    bool ordered = true;

    std::thread consumer([&]() {
        while (consumed < NUM) {
            TraceEvent e;
            if (rb.Pop(e)) {
                if (consumed > 0 && e.timestamp_ns <= last_ts) {
                    ordered = false;
                }
                last_ts = e.timestamp_ns;
                ++consumed;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed, NUM);
    EXPECT_TRUE(ordered);
}

}  // namespace
}  // namespace rtdiag
