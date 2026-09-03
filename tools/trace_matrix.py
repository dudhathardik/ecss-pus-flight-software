#!/usr/bin/env python3
"""Build the requirements traceability matrix from the sources and tests.

ECSS-E-ST-40C asks for bidirectional traceability between the software
requirements, the design and the verification evidence. Keeping that matrix in
a spreadsheet is how it goes stale, so this script derives it from the only
three places that cannot lie: the requirement list in docs/SRS.md, the
``@implements`` tags in the code, and the ``@verifies`` tags in the tests.

Run with --check in CI: a requirement that loses its implementation or its
test, or a tag pointing at a requirement that no longer exists, fails the
build the same way a compilation error would.

Usage:
    python3 tools/trace_matrix.py [--check] [--output docs/traceability.md]
"""

import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRS = os.path.join(REPO, "docs", "SRS.md")
IMPLEMENTATION_DIRS = ["include", "src", "sim"]
VERIFICATION_DIRS = ["tests"]
SOURCE_SUFFIXES = (".c", ".h", ".py")

REQUIREMENT_RE = re.compile(
    r"^-\s+\*\*(?P<id>SWREQ-[A-Z0-9]+-\d+)\*\*\s+"
    r"\((?P<method>Test|Analysis|Inspection|Review)\)\s+(?P<text>.+?)\s*$")
IMPLEMENTS_RE = re.compile(r"@implements\s+(SWREQ-[A-Z0-9]+-\d+)")
VERIFIES_RE = re.compile(r"@verifies\s+(SWREQ-[A-Z0-9]+-\d+)")
C_FUNCTION_RE = re.compile(r"^[A-Za-z_][\w \t\*]*\b(?P<name>\w+)\s*\(")
PY_FUNCTION_RE = re.compile(r"^def\s+(?P<name>\w+)\s*\(")


class Requirement(object):
    def __init__(self, identifier, method, text, section):
        self.id = identifier
        self.method = method
        self.text = text
        self.section = section
        self.implemented_by = []   # list of "path:line"
        self.verified_by = []      # list of "path::case"

    @property
    def needs_test(self):
        return self.method == "Test"

    @property
    def status(self):
        if not self.implemented_by:
            return "NO CODE"
        if self.needs_test and not self.verified_by:
            return "NO TEST"
        return "OK"


def walk_sources(directories):
    for directory in directories:
        root_dir = os.path.join(REPO, directory)
        for base, _, files in os.walk(root_dir):
            if "__pycache__" in base or os.sep + "build" in base:
                continue
            for name in sorted(files):
                if name.endswith(SOURCE_SUFFIXES):
                    yield os.path.join(base, name)


def relative(path):
    return os.path.relpath(path, REPO)


def parse_requirements():
    """Read the requirement list out of the software requirements document."""
    if not os.path.exists(SRS):
        sys.exit("cannot find {}".format(relative(SRS)))

    requirements = {}
    order = []
    section = "(no section)"

    with open(SRS) as handle:
        for line in handle:
            if line.startswith("## "):
                section = line[3:].strip()
                continue
            match = REQUIREMENT_RE.match(line.rstrip("\n"))
            if not match:
                continue
            identifier = match.group("id")
            if identifier in requirements:
                sys.exit("duplicate requirement {} in {}"
                         .format(identifier, relative(SRS)))
            requirements[identifier] = Requirement(
                identifier, match.group("method"), match.group("text"), section)
            order.append(identifier)

    if not requirements:
        sys.exit("no requirements found in {}".format(relative(SRS)))
    return requirements, order


def enclosing_c_function(lines, index):
    """Name of the function the tag block at ``index`` documents."""
    for offset in range(index, min(index + 12, len(lines))):
        candidate = lines[offset]
        if candidate.startswith((" ", "\t", "*", "/", "#")) or not candidate.strip():
            continue
        match = C_FUNCTION_RE.match(candidate)
        if match:
            return match.group("name")
    return None


def enclosing_py_function(lines, index):
    """Name of the test function whose docstring holds the tag."""
    for offset in range(index, -1, -1):
        match = PY_FUNCTION_RE.match(lines[offset])
        if match:
            return match.group("name")
    return None


def collect_tags(requirements, unknown):
    """Attach implementation and verification evidence to the requirements."""
    for path in walk_sources(IMPLEMENTATION_DIRS):
        with open(path) as handle:
            for number, line in enumerate(handle, start=1):
                for identifier in IMPLEMENTS_RE.findall(line):
                    if identifier not in requirements:
                        unknown.append((identifier, relative(path), number))
                        continue
                    requirements[identifier].implemented_by.append(
                        "{}:{}".format(relative(path), number))

    for path in walk_sources(VERIFICATION_DIRS):
        with open(path) as handle:
            lines = handle.read().splitlines()
        is_python = path.endswith(".py")
        for index, line in enumerate(lines):
            identifiers = VERIFIES_RE.findall(line)
            if not identifiers:
                continue
            if is_python:
                case = enclosing_py_function(lines, index)
            else:
                case = enclosing_c_function(lines, index)
            for identifier in identifiers:
                if identifier not in requirements:
                    unknown.append((identifier, relative(path), index + 1))
                    continue
                requirements[identifier].verified_by.append(
                    "{}::{}".format(relative(path), case or "?"))


def render(requirements, order, unknown):
    total = len(order)
    implemented = sum(1 for i in order if requirements[i].implemented_by)
    verified = sum(1 for i in order
                   if requirements[i].verified_by or not requirements[i].needs_test)
    cases = sorted({case for i in order for case in requirements[i].verified_by})

    out = []
    out.append("# Requirements traceability matrix")
    out.append("")
    out.append("Generated by `tools/trace_matrix.py` - do not edit by hand.")
    out.append("Evidence comes from `@implements` tags in the sources and")
    out.append("`@verifies` tags in the tests; `make trace` regenerates it and")
    out.append("fails if any requirement has lost its code or its test.")
    out.append("")
    out.append("| Metric | Value |")
    out.append("|---|---|")
    out.append("| Requirements | {} |".format(total))
    out.append("| Implemented | {} ({:.0f}%) |".format(
        implemented, 100.0 * implemented / total))
    out.append("| Verified | {} ({:.0f}%) |".format(
        verified, 100.0 * verified / total))
    out.append("| Distinct test cases referenced | {} |".format(len(cases)))
    out.append("")

    current_section = None
    for identifier in order:
        requirement = requirements[identifier]
        if requirement.section != current_section:
            current_section = requirement.section
            out.append("")
            out.append("## {}".format(current_section))
            out.append("")
            out.append("| Requirement | Method | Implemented in | Verified by | Status |")
            out.append("|---|---|---|---|---|")

        implementation = "<br>".join("`{}`".format(x)
                                     for x in requirement.implemented_by) or "-"
        verification = "<br>".join("`{}`".format(x)
                                   for x in requirement.verified_by) or "-"
        out.append("| **{}** | {} | {} | {} | {} |".format(
            requirement.id, requirement.method, implementation, verification,
            requirement.status))

    if unknown:
        out.append("")
        out.append("## Dangling tags")
        out.append("")
        out.append("| Tag | Location |")
        out.append("|---|---|")
        for identifier, path, number in unknown:
            out.append("| {} | `{}:{}` |".format(identifier, path, number))

    out.append("")
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero when the matrix has holes")
    parser.add_argument("--output", default=os.path.join("docs", "traceability.md"),
                        help="where to write the matrix")
    args = parser.parse_args()

    requirements, order = parse_requirements()
    unknown = []
    collect_tags(requirements, unknown)

    output_path = os.path.join(REPO, args.output)
    with open(output_path, "w") as handle:
        handle.write(render(requirements, order, unknown))

    missing_code = [i for i in order if not requirements[i].implemented_by]
    missing_test = [i for i in order if requirements[i].needs_test
                    and not requirements[i].verified_by]

    print("traceability: {} requirements, {} without code, {} without a test, "
          "{} dangling tag(s) -> {}".format(
              len(order), len(missing_code), len(missing_test), len(unknown),
              args.output))

    if not args.check:
        return 0

    failed = False
    for identifier in missing_code:
        print("  ERROR {}: no @implements tag in the sources".format(identifier))
        failed = True
    for identifier in missing_test:
        print("  ERROR {}: verification method is Test but no @verifies tag"
              .format(identifier))
        failed = True
    for identifier, path, number in unknown:
        print("  ERROR {}:{} refers to {}, which is not in the SRS"
              .format(path, number, identifier))
        failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
