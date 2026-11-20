#!/usr/bin/env bash
# ===============================================
# Static analysis of our own code with cppcheck (needs: apt install cppcheck)
# Usage: bash tools/ci/static-analysis.sh
# Report: apps/desktop/build/reports/cppcheck.xml
# Fails only on "error" severity findings (likely bugs); other findings are reported
# ===============================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REPORT_DIR="$ROOT/apps/desktop/build/reports"
REPORT="$REPORT_DIR/cppcheck.xml"

mkdir -p "$REPORT_DIR"

echo "==============================================="
echo "  Atelier3D static analysis (cppcheck)"
echo "==============================================="
cppcheck --version

# Only our code under apps/ is analysed; third-party code (apps/desktop/libraries/) and build output are skipped
#   -i: do not analyse those folders;  --suppress=*:<libraries>/*: no findings in library headers our code includes
#   -UCreateWindow: the "#ifdef CreateWindow / #undef" guards make cppcheck try a config where
#                   CreateWindow is a macro, which never happens in a real build (false syntaxError)
#   functionStatic: "method can be static" is noise for engine APIs designed as instance methods
CPPCHECK_ARGS=(
    # everything except "information" (which is mostly about cppcheck's own config)
    --enable=warning,style,performance,portability
    --std=c++20 --language=c++
    --inline-suppr
    -i "$ROOT/apps/desktop/libraries"
    -i "$ROOT/apps/desktop/build"
    --suppress="*:$ROOT/apps/desktop/libraries/*"
    --suppress=missingIncludeSystem
    --suppress=unmatchedSuppression
    --suppress=functionStatic
    -UCreateWindow
    -I "$ROOT/apps/desktop/projects/atelier_engine/include"
    -I "$ROOT/apps/desktop/projects/atelier_editor/include"
    -j "$(nproc)"
    --quiet
    "$ROOT/apps"
)

# The files in .clang-format-ignore are kept exactly as their authors wrote them, so their findings (which could only be
# fixed by changing them) are not reported; they are still analysed as part of the code that includes them
while IFS= read -r keep; do
    CPPCHECK_ARGS+=(--suppress="*:$ROOT/$keep")
done < <(grep -vE '^[[:space:]]*(#|$)' "$ROOT/.clang-format-ignore" | tr -d '\r')

# Readable findings in the console log
cppcheck "${CPPCHECK_ARGS[@]}" --template='{file}:{line}: {severity}: {message} [{id}]'

# XML report that Jenkins displays
cppcheck "${CPPCHECK_ARGS[@]}" --xml --output-file="$REPORT"

# Each finding is an <error> element in the XML, whatever its severity
TOTAL=$(grep -c '<error ' "$REPORT" || true)
ERRORS=$(grep -c 'severity="error"' "$REPORT" || true)
echo "cppcheck findings: $TOTAL total, $ERRORS with severity 'error'"
echo "Report: $REPORT"

if [ "$ERRORS" -gt 0 ]; then
    echo "ERROR: cppcheck found likely bugs (severity 'error')" >&2
    exit 1
fi
