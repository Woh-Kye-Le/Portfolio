#!/usr/bin/env bash
# ===============================================
# Runs the unit tests in parallel, then the asset tool's tests, and writes JUnit-style reports for Jenkins
# Usage: bash tools/ci/test.sh [Debug|Release] [jobs]   (run build.sh <config> linux first)
#        jobs defaults to the number of CPU cores
# Reports: apps/desktop/build/reports/tests-<shard>.xml and tests-tool.xml
# ===============================================
set -euo pipefail

CONFIG="${1:-Debug}"
JOBS="${2:-$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/apps/desktop/build/ci-$CONFIG"
REPORT_DIR="$ROOT/apps/desktop/build/reports"

mkdir -p "$REPORT_DIR"
# Delete the old reports first, so Jenkins never shows results from an earlier run if the tests crash
rm -f "$REPORT_DIR"/tests*.xml "$REPORT_DIR"/tests*.log

echo "==============================================="
echo "  Atelier3D unit tests - $CONFIG ($JOBS shards)"
echo "==============================================="

# GoogleTest sharding: shard i runs every JOBS-th test, so together the shards run each test exactly once.
# Each shard writes its own XML report (grouped by test suite in Jenkins) and its own log, printed once it is done
# so the output of different shards is not interleaved
pids=()
for ((shard = 0; shard < JOBS; shard++)); do
    GTEST_TOTAL_SHARDS="$JOBS" GTEST_SHARD_INDEX="$shard" \
        "$BUILD_DIR/bin/AtelierTests" --gtest_output="xml:$REPORT_DIR/tests-$shard.xml" \
        > "$REPORT_DIR/tests-$shard.log" 2>&1 &
    pids+=("$!")
done

# Any failing shard makes the exit code non-zero, so the stage fails
failed=0
for ((shard = 0; shard < JOBS; shard++)); do
    if ! wait "${pids[$shard]}"; then
        failed=1
    fi
    echo "----- shard $shard -----"
    cat "$REPORT_DIR/tests-$shard.log"
done

# The asset tool's tests: few and quick, so not sharded. There are none in a build without the asset tool.
TOOL_TESTS="$BUILD_DIR/bin/AtelierToolTests"
if [ -f "$TOOL_TESTS" ]; then
    if ! "$TOOL_TESTS" --gtest_output="xml:$REPORT_DIR/tests-tool.xml" > "$REPORT_DIR/tests-tool.log" 2>&1; then
        failed=1
    fi
    echo "----- asset tool -----"
    cat "$REPORT_DIR/tests-tool.log"
fi

if ((failed)); then
    echo "Tests FAILED (see the logs above)"
fi
exit "$failed"
