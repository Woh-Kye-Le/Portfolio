#!/usr/bin/env bash
# ===============================================
# Lints our own code under apps/ (needs: apt install clang-format clang-tidy)
# Third-party code (apps/desktop/libraries/) and build output (apps/desktop/build/) are never checked
# Usage: bash tools/ci/lint.sh [Debug|Release] [base commit]
#        (run build.sh <config> linux first; clang-tidy reads its compile_commands.json)
#        With a base commit, clang-tidy only checks what changed since it, and every file that includes a change
#        (tools/ci/lint-targets.py); without one it checks everything. clang-format always checks everything.
# Reports: apps/desktop/build/reports/clang-format.txt, apps/desktop/build/reports/clang-tidy.txt
#          (clang-tidy-full.txt: clang-tidy's unfiltered output, for debugging)
#   clang-format - fails if any file does not match .clang-format
#                  (fix locally with: clang-format -i <file>)
#   clang-tidy   - fails on bugprone-* / clang-analyzer-* findings (likely bugs, see .clang-tidy);
#                  other findings are reported
# Both checks always run so one report shows every problem
# ===============================================
set -euo pipefail

CONFIG="${1:-Debug}"
BASE_COMMIT="${2:-}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/apps/desktop/build/ci-$CONFIG"
REPORT_DIR="$ROOT/apps/desktop/build/reports"

mkdir -p "$REPORT_DIR"

echo "==============================================="
echo "  Atelier3D lint (clang-format, clang-tidy)"
echo "==============================================="
clang-format --version
clang-tidy --version

# Remember a failure instead of exiting straight away, so both checks always run
FAILED=0

# Only our code under apps/ is linted; third-party code under apps/desktop/libraries/ is skipped
mapfile -t FILES < <(cd "$ROOT" && git ls-files -- 'apps/*.cpp' 'apps/*.hpp' 'apps/*.h' \
                                                   ':(exclude)apps/desktop/libraries/*' ':(exclude)apps/desktop/build/*')

echo "[1/2] clang-format (${#FILES[@]} files)..."
# --dry-run only checks (doesn't change files), --Werror makes any difference count as a failure
if (cd "$ROOT" && clang-format --dry-run --Werror "${FILES[@]}") > "$REPORT_DIR/clang-format.txt" 2>&1; then
    echo "clang-format: all files formatted"
else
    # File names only: the full diagnostics are in the report (Jenkins shows them on the "clang-format" page)
    grep -oE '^[^:]+:[0-9]+' "$REPORT_DIR/clang-format.txt" | cut -d: -f1 | sort -u | sed 's/^/  needs formatting: /'
    echo "ERROR: files above do not match .clang-format (run: clang-format -i <file>)" >&2
    FAILED=1
fi

echo "[2/2] clang-tidy..."
# clang-tidy needs the exact compiler flags of every file, which CMake writes to compile_commands.json
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "ERROR: $BUILD_DIR/compile_commands.json not found (run: bash tools/ci/build.sh $CONFIG linux)" >&2
    exit 1
fi

# The regex picks our .cpp files out of the compile database: everything under apps/ except libraries/ and build/
# (headers are covered via HeaderFilterRegex in .clang-tidy)
TIDY_FILES=("^$ROOT/apps/(?!desktop/(libraries|build)/).*\\.cpp$")
# Anything going wrong in lint-targets.py leaves the full list in place, so a problem there can only make lint slower
if [ -n "$BASE_COMMIT" ] && CHANGED=$(python3 "$ROOT/tools/ci/lint-targets.py" "$ROOT" "$BUILD_DIR" "$BASE_COMMIT"); then
    TIDY_FILES=()
    if [ -n "$CHANGED" ]; then
        mapfile -t TIDY_FILES <<< "$CHANGED"
    fi
fi

TIDY_OK=1
: > "$REPORT_DIR/clang-tidy-full.txt"
if [ "${#TIDY_FILES[@]}" -eq 0 ]; then
    echo "clang-tidy: no files to check"
elif ! run-clang-tidy -p "$BUILD_DIR" -quiet -j "$(nproc)" "${TIDY_FILES[@]}" \
        > "$REPORT_DIR/clang-tidy-full.txt" 2>&1; then
    TIDY_OK=0
fi

# Keep only findings in our code: clang-analyzer findings ignore HeaderFilterRegex, so a library header that our code
# includes (e.g. libraries/include/OpenFBX/ofbx.h) can still show up in clang-tidy's output
# The files in .clang-format-ignore are kept exactly as their authors wrote them, so their findings (which could only be
# fixed by changing them) are left out too
# (one "<root>/<file>:" per line, matched as plain text against the start of each finding)
grep -vE '^[[:space:]]*(#|$)' "$ROOT/.clang-format-ignore" | tr -d '\r' | sed "s#^#$ROOT/#; s#\$#:#" \
    > "$REPORT_DIR/keep-as-written.txt"
grep -E ': (warning|error): ' "$REPORT_DIR/clang-tidy-full.txt" | grep -E "^$ROOT/apps/" \
    | grep -vE "^$ROOT/apps/desktop/(libraries|build)/" | grep -vF -f "$REPORT_DIR/keep-as-written.txt" \
    > "$REPORT_DIR/clang-tidy.txt" || true

# Readable findings in the console log, as "file:line [severity] message [check]"
# (not in compiler format, so the GCC warnings parser does not count them again; Jenkins reads the report file)
grep -E ': (warning|error): ' "$REPORT_DIR/clang-tidy.txt" | sort -u \
    | sed -E -e "s#^$ROOT/##" -e 's#^([^:]+:[0-9]+):[0-9]+: (warning|error): #  \1 [\2] #' || true

# sort -u because the same header warning shows up once for every .cpp that includes it
WARNINGS=$(grep -E ': warning: ' "$REPORT_DIR/clang-tidy.txt" | sort -u | wc -l || true)
ERRORS=$(grep -E ': error: ' "$REPORT_DIR/clang-tidy.txt" | sort -u | wc -l || true)
echo "clang-tidy findings: $WARNINGS warnings, $ERRORS errors"

if [ "$ERRORS" -gt 0 ]; then
    echo "ERROR: clang-tidy found likely bugs (see .clang-tidy WarningsAsErrors)" >&2
    FAILED=1
elif [ "$TIDY_OK" -ne 1 ] && ! grep -qE ': error: ' "$REPORT_DIR/clang-tidy-full.txt"; then
    # Non-zero exit without any error line means clang-tidy itself failed; show the full log
    # (errors only in third-party code also give a non-zero exit, and are ignored)
    cat "$REPORT_DIR/clang-tidy-full.txt"
    echo "ERROR: clang-tidy did not run" >&2
    FAILED=1
fi

echo "Reports: $REPORT_DIR/clang-format.txt, $REPORT_DIR/clang-tidy.txt"
exit "$FAILED"
