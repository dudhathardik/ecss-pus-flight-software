#!/usr/bin/env python3
"""Summarise gcov output for the flight sources and enforce a floor.

ECSS-Q-ST-80C asks for statement coverage evidence at unit level; this turns
the raw gcov log into a table that can be pasted into a review and into an
exit code that CI can act on.

Usage: coverage_report.py <gcov log> [minimum percentage]
"""

import re
import sys

FILE_RE = re.compile(r"^File '(?P<path>[^']+)'")
LINES_RE = re.compile(r"^Lines executed:(?P<pct>[0-9.]+)% of (?P<total>\d+)")


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: coverage_report.py <gcov log> [minimum percentage]")

    log_path = sys.argv[1]
    minimum = float(sys.argv[2]) if len(sys.argv) > 2 else 90.0

    results = {}
    current = None
    with open(log_path) as handle:
        for line in handle:
            match = FILE_RE.match(line)
            if match:
                current = match.group("path")
                continue
            match = LINES_RE.match(line)
            if match and current:
                if "/src/" in current or current.startswith("src/"):
                    percent = float(match.group("pct"))
                    total = int(match.group("total"))
                    results[current] = (percent, total)
                current = None

    if not results:
        print("no coverage data for src/ found in {}".format(log_path))
        return 1

    covered_lines = 0
    total_lines = 0
    print("")
    print("{:<44} {:>8} {:>9}".format("file", "lines", "covered"))
    print("-" * 63)
    for path in sorted(results):
        percent, total = results[path]
        covered_lines += percent * total / 100.0
        total_lines += total
        print("{:<44} {:>8} {:>8.1f}%".format(path.split("/")[-1], total, percent))

    overall = 100.0 * covered_lines / total_lines if total_lines else 0.0
    print("-" * 63)
    print("{:<44} {:>8} {:>8.1f}%".format("TOTAL", total_lines, overall))
    print("")

    if overall < minimum:
        print("FAIL: statement coverage {:.1f}% is below the {:.0f}% floor"
              .format(overall, minimum))
        return 1
    print("OK: statement coverage {:.1f}% meets the {:.0f}% floor"
          .format(overall, minimum))
    return 0


if __name__ == "__main__":
    sys.exit(main())
