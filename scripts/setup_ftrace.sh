#!/bin/bash
# setup_ftrace.sh — Configure ftrace tracers for RT diagnostics
#
# Usage: sudo ./scripts/setup_ftrace.sh [tracer]
#   tracer: function_graph | irqsoff | preemptoff | wakeup_rt (default: function_graph)

set -euo pipefail

TRACE_DIR="/sys/kernel/debug/tracing"
TRACER="${1:-function_graph}"

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: This script must be run as root (sudo)." >&2
    exit 1
fi

if [[ ! -d "$TRACE_DIR" ]]; then
    echo "ERROR: ftrace directory not found at $TRACE_DIR" >&2
    echo "Try: mount -t debugfs debugfs /sys/kernel/debug" >&2
    exit 1
fi

echo "=== RT Diagnostic Toolchain — ftrace Setup ==="
echo ""

# Stop any existing tracing
echo 0 > "$TRACE_DIR/tracing_on"
echo "Tracing stopped."

# Set the tracer
AVAILABLE=$(cat "$TRACE_DIR/available_tracers")
echo "Available tracers: $AVAILABLE"

if ! echo "$AVAILABLE" | grep -qw "$TRACER"; then
    echo "ERROR: Tracer '$TRACER' not available." >&2
    echo "Available: $AVAILABLE" >&2
    exit 1
fi

echo "$TRACER" > "$TRACE_DIR/current_tracer"
echo "Tracer set to: $TRACER"

# Configure buffer size (4 MB per CPU)
BUFFER_SIZE_KB=4096
echo "$BUFFER_SIZE_KB" > "$TRACE_DIR/buffer_size_kb"
echo "Buffer size: ${BUFFER_SIZE_KB} KB/CPU"

# Enable relevant events
echo 1 > "$TRACE_DIR/events/sched/sched_switch/enable" 2>/dev/null || true
echo 1 > "$TRACE_DIR/events/sched/sched_wakeup/enable" 2>/dev/null || true
echo 1 > "$TRACE_DIR/events/irq/irq_handler_entry/enable" 2>/dev/null || true
echo 1 > "$TRACE_DIR/events/irq/irq_handler_exit/enable" 2>/dev/null || true
echo "Enabled: sched_switch, sched_wakeup, irq_handler_entry/exit"

# Set clock to mono_raw for best accuracy
echo "mono_raw" > "$TRACE_DIR/trace_clock" 2>/dev/null || \
    echo "mono" > "$TRACE_DIR/trace_clock" 2>/dev/null || true
CLOCK=$(cat "$TRACE_DIR/trace_clock" | grep -o '\[.*\]' | tr -d '[]')
echo "Clock source: $CLOCK"

# Clear the trace buffer
echo > "$TRACE_DIR/trace"
echo "Trace buffer cleared."

# Start tracing
echo 1 > "$TRACE_DIR/tracing_on"
echo ""
echo "✓ Tracing is now ACTIVE with tracer: $TRACER"
echo "  Read events: cat $TRACE_DIR/trace_pipe"
echo "  Stop tracing: echo 0 > $TRACE_DIR/tracing_on"
echo ""
