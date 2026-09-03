#!/bin/sh
# Statement coverage of the flight sources under the unit test suite.
#
# Every test binary is linked against one shared set of instrumented objects,
# so the counters accumulate across the whole suite and the report describes
# the coverage achieved by the suite rather than by one test file.
#
# Usage: tools/coverage.sh [minimum percentage]

set -eu

CC=${CC:-cc}
MIN=${1:-90}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/cov"
CFLAGS="-std=c99 -O0 -g --coverage -Iinclude -Itests/framework"

cd "$ROOT"
rm -rf "$OUT"
mkdir -p "$OUT/src" "$OUT/bin"

echo "== instrumenting flight sources =="
for source in src/*.c tests/framework/test_support.c; do
    $CC $CFLAGS -c "$source" -o "$OUT/src/$(basename "$source" .c).o"
done

echo "== building and running the unit tests =="
for test_source in tests/unit/test_*.c; do
    name=$(basename "$test_source" .c)
    $CC $CFLAGS -c "$test_source" -o "$OUT/bin/$name.o"
    $CC --coverage -o "$OUT/bin/$name" "$OUT/bin/$name.o" "$OUT"/src/*.o
    "$OUT/bin/$name" > "$OUT/bin/$name.log" || {
        echo "FAILED: $name"; cat "$OUT/bin/$name.log"; exit 1; }
done

if command -v gcov > /dev/null 2>&1; then
    GCOV="gcov"
elif xcrun --find llvm-cov > /dev/null 2>&1; then
    GCOV="xcrun llvm-cov gcov"
else
    echo "no gcov available - install gcc or the LLVM tools to get a report"
    exit 0
fi

echo "== coverage of src/ =="
# Run from the repository root so that gcov resolves the relative source
# paths recorded at compile time and can emit annotated listings.
$GCOV -o "$OUT/src" "$OUT/src"/*.gcda > "$OUT/gcov.log" 2>&1 || true
mv ./*.gcov "$OUT/" 2>/dev/null || true
python3 "$ROOT/tools/coverage_report.py" "$OUT/gcov.log" "$MIN"
echo "Annotated listings: build/cov/*.gcov"
