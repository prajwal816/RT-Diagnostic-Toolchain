#!/bin/bash
# check_rt_config.sh — Validate Linux PREEMPT_RT configuration
#
# Checks kernel version, RT patches, scheduler tunables, and permissions.

set -euo pipefail

PASS=0
WARN=0
FAIL=0

check_pass()  { echo "  ✓ PASS: $1"; ((PASS++)); }
check_warn()  { echo "  ⚠ WARN: $1"; ((WARN++)); }
check_fail()  { echo "  ✗ FAIL: $1"; ((FAIL++)); }

echo "=== RT Diagnostic Toolchain — System Check ==="
echo ""

# --- Kernel Version ---
echo "--- Kernel ---"
KERNEL=$(uname -r)
echo "  Kernel: $KERNEL"

if uname -r | grep -qi "rt\|preempt"; then
    check_pass "PREEMPT_RT kernel detected"
else
    check_warn "Not a PREEMPT_RT kernel (some features will use simulation)"
fi

if [[ -f /proc/version ]]; then
    if grep -qi "PREEMPT" /proc/version; then
        check_pass "PREEMPT enabled in /proc/version"
    else
        check_warn "PREEMPT not found in /proc/version"
    fi
fi
echo ""

# --- Scheduling ---
echo "--- Scheduler Tunables ---"

if [[ -f /proc/sys/kernel/sched_rt_runtime_us ]]; then
    RT_RUNTIME=$(cat /proc/sys/kernel/sched_rt_runtime_us)
    echo "  sched_rt_runtime_us: $RT_RUNTIME"
    if [[ "$RT_RUNTIME" == "-1" ]]; then
        check_pass "RT throttling disabled (unlimited)"
    else
        check_warn "RT throttling at ${RT_RUNTIME}µs (consider -1 for full RT)"
    fi
else
    check_fail "sched_rt_runtime_us not found"
fi

if [[ -f /proc/sys/kernel/sched_rt_period_us ]]; then
    echo "  sched_rt_period_us: $(cat /proc/sys/kernel/sched_rt_period_us)"
fi
echo ""

# --- ftrace ---
echo "--- ftrace ---"
TRACE_DIR="/sys/kernel/debug/tracing"

if [[ -d "$TRACE_DIR" ]]; then
    check_pass "ftrace directory found at $TRACE_DIR"

    if [[ -r "$TRACE_DIR/trace_pipe" ]]; then
        check_pass "trace_pipe is readable"
    else
        check_warn "trace_pipe not readable (need root?)"
    fi

    TRACERS=$(cat "$TRACE_DIR/available_tracers" 2>/dev/null || echo "")
    echo "  Available tracers: $TRACERS"

    for T in function_graph irqsoff preemptoff wakeup_rt; do
        if echo "$TRACERS" | grep -qw "$T"; then
            check_pass "Tracer '$T' available"
        else
            check_warn "Tracer '$T' not available"
        fi
    done
else
    check_fail "ftrace directory not found"
    echo "  Try: mount -t debugfs debugfs /sys/kernel/debug"
fi
echo ""

# --- CPU ---
echo "--- CPU ---"
NPROC=$(nproc)
echo "  CPUs: $NPROC"

if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
    GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
    echo "  CPU governor: $GOV"
    if [[ "$GOV" == "performance" ]]; then
        check_pass "CPU governor set to performance"
    else
        check_warn "CPU governor is '$GOV' (use 'performance' for RT)"
    fi
fi

# Check for isolated CPUs
if [[ -f /sys/devices/system/cpu/isolated ]]; then
    ISOLATED=$(cat /sys/devices/system/cpu/isolated)
    if [[ -n "$ISOLATED" ]]; then
        echo "  Isolated CPUs: $ISOLATED"
        check_pass "CPU isolation configured"
    else
        check_warn "No isolated CPUs (consider isolating cores for RT tasks)"
    fi
fi
echo ""

# --- Memory ---
echo "--- Memory ---"
if [[ -f /proc/sys/vm/swappiness ]]; then
    SWAP=$(cat /proc/sys/vm/swappiness)
    echo "  vm.swappiness: $SWAP"
    if [[ "$SWAP" -le 10 ]]; then
        check_pass "Low swappiness ($SWAP)"
    else
        check_warn "High swappiness ($SWAP) — consider lowering for RT"
    fi
fi
echo ""

# --- Summary ---
echo "=== Summary ==="
echo "  PASS: $PASS | WARN: $WARN | FAIL: $FAIL"

if [[ $FAIL -gt 0 ]]; then
    echo "  ✗ System has issues that may prevent RT operation."
    exit 1
elif [[ $WARN -gt 0 ]]; then
    echo "  ⚠ System functional but not optimally configured for RT."
    exit 0
else
    echo "  ✓ System is well-configured for real-time operation."
    exit 0
fi
