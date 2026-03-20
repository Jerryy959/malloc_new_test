#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
VTUNE="${VTUNE:-vtune}"
ALLOCATOR_PRELOAD="${ALLOCATOR_PRELOAD:-}"
RESULT_ROOT="${RESULT_ROOT:-vtune-results}"

mkdir -p "$RESULT_ROOT"

run_case() {
  local name="$1"
  shift
  echo "==> ${name}"
  if [[ -n "$ALLOCATOR_PRELOAD" ]]; then
    env LD_PRELOAD="$ALLOCATOR_PRELOAD" "$VTUNE" -collect hotspots -result-dir "$RESULT_ROOT/$name" -- "$@"
  else
    "$VTUNE" -collect hotspots -result-dir "$RESULT_ROOT/$name" -- "$@"
  fi
}

run_case latency_fixed "$BUILD_DIR/latency_fixed" --iterations 200000 --min-size 64
run_case latency_size_sweep "$BUILD_DIR/latency_size_sweep" --iterations 80000 --min-size 16 --max-size 4096 --step 16
run_case remote_free "$BUILD_DIR/remote_free" --iterations 200000 --min-size 64 --queue-depth 4096
run_case burst_contention "$BUILD_DIR/burst_contention" --iterations 50000 --threads 8 --batch 128 --min-size 64
run_case large_alloc "$BUILD_DIR/large_alloc" --iterations 20000 --max-size 1048576 --no-touch
