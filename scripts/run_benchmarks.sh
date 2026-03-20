#!/bin/bash
# run_benchmarks.sh — Build and run all RT Diagnostic benchmarks
#
# Usage: ./scripts/run_benchmarks.sh

set -euo pipefail

echo "=== RT Diagnostic Toolchain — Benchmark Suite ==="
echo ""

# Build benchmarks
echo "[1/3] Building benchmarks..."
bazel build //benchmarks/...
echo "      Build complete."
echo ""

# Run throughput benchmark
echo "[2/3] Running throughput benchmark..."
echo "───────────────────────────────────────"
bazel run //benchmarks:throughput_bench
echo ""

# Run latency benchmark
echo "[3/3] Running latency benchmark..."
echo "───────────────────────────────────────"
bazel run //benchmarks:latency_bench
echo ""

echo "=== All benchmarks complete ==="
