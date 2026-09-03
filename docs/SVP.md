# Software Verification Plan

**Issue:** 1.0
**Applicable standards:** ECSS-E-ST-40C clause 5.6 (verification),
ECSS-Q-ST-80C clause 6.3 (software product assurance)

---

## 1. Purpose

This plan states how the requirements in [SRS.md](SRS.md) are verified, what
evidence each level produces, and what makes a build acceptable. It is short
on purpose: everything it describes is executable, and the numbers it quotes
come from running it rather than from a review meeting.

## 2. Verification levels

| Level | Object under test | Interface used | Where |
|---|---|---|---|
| Unit | One module, linked against the real ones below it | The C API of the module | `tests/unit/` |
| Integration (SW/SW) | The whole on-board software | The TC/TM socket of `obc_sim` | `tests/integration/` |
| Analysis | The sources | Automated checks | `make static`, `make coverage` |
| Inspection | The sources | Peer review against [CODING_STANDARD.md](CODING_STANDARD.md) | Pull request |

The functional level deliberately reaches the software only through the
telecommand link. It cannot inspect a variable, so a requirement that cannot
be observed from the ground is a requirement the ground cannot operate — and
that is exactly the property being tested.

## 3. Test case identification

| Level | Identifier | Example |
|---|---|---|
| Unit | The C function name | `test_heater_switches_on_at_the_lower_threshold` |
| Integration | `ITC-<area>-<number>` in the Python function name | `test_ITC_008_060_over_temperature_drops_the_unit_into_safe_mode` |

Every test case carries one or more `@verifies SWREQ-...` tags.
`tools/trace_matrix.py` collects them into [traceability.md](traceability.md)
and fails if a requirement has no test, or if a tag names a requirement that
no longer exists. Deleting a requirement without deleting its test, or the
other way round, breaks the build.

## 4. Unit test approach

The suite has no external dependency: `tests/framework/minunity.h` is a
150-line assertion core that reproduces the macro names and semantics of
ThrowTheSwitch/Unity, which is the framework used on the target. Migrating a
test file to real Unity under Ceedling means replacing the include; the test
bodies do not change.

Test design techniques used, and where:

| Technique | Applied to |
|---|---|
| Equivalence partitioning | Telecommand acceptance: one case per rejection path |
| Boundary value analysis | Thermal thresholds: T-1, T and T+1 at each of the three set points, approached from both directions |
| State transition testing | SAFE / NOMINAL transitions, including the autonomous one |
| Error guessing / fault injection | Corrupted CRC, runt packets, malformed lists, queue saturation |
| Defensive path coverage | `tests/unit/test_defensive.c`, one case per null-pointer guard |

Time is derived from the minor cycle counter rather than the wall clock
(`SWREQ-HAL-010`), so a given command sequence produces byte-identical
telemetry on every run. There are no sleeps in the unit suite.

## 5. Coverage

`make coverage` links every unit test binary against one shared set of
instrumented objects, so the counters accumulate over the whole suite.

| Metric | Floor | Achieved |
|---|---|---|
| Statement coverage of `src/` | 95% | 99.5% |

The uncovered statements are listed below. Each is a defensive return that
cannot be reached by construction; they are kept rather than deleted because
they document the contract of the call, and they are listed here rather than
excluded from the metric.

| Location | Why it cannot be reached |
|---|---|
| `src/pus_tc.c`, `ccsds_unpack_primary` error return | The buffer length is checked before the call, which is the only way that function can fail |
| `src/pus_tm.c`, `ccsds_pack_primary` error return | The capacity is checked before the call |
| `src/svc03_housekeeping.c`, one-shot generation error break | Every structure identifier in the list is validated before the loop runs |

Branch coverage is not measured. On a real project it would be, with a
condition/decision floor agreed with product assurance at the criticality
category of the software; it is left out here because the tooling for it is
not part of a base toolchain.

## 6. Static analysis

`make static` performs two checks:

1. No dynamic allocation anywhere in the flight sources. This is the
   automated verification of `SWREQ-TMQ-030`.
2. `cppcheck` at `warning,style,performance,portability`, treated as an
   error.

The compiler itself is part of the analysis: the flight sources build with
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`,
so an implicit narrowing conversion fails the build rather than producing a
note nobody reads.

## 7. Acceptance criteria for a build

A build is acceptable when all of the following hold, which is exactly what
`make verify` and the CI job run:

1. The flight sources compile with no warning at the flag set above.
2. Every unit test passes.
3. Every functional test passes.
4. Statement coverage of `src/` is at or above 95%.
5. The traceability matrix has no requirement without code, no requirement
   whose method is *Test* without a test, and no dangling tag.
6. `make static` reports no finding.

## 8. Regression policy

Every defect found after the first green build is reproduced by a new test
case at the level at which it could have been caught, and that test case is
tagged with the requirement it belongs to. If no requirement covers it, the
requirement was missing and the SRS is updated first.

## 9. What this plan does not cover

Out of scope for the demonstrator, and named here so the gap is deliberate:
worst case execution time analysis, stack usage analysis, memory partitioning
evidence, target hardware test campaigns, radiation-induced fault injection,
and independent software validation by a party other than the developer.
