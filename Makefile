# ---------------------------------------------------------------------------
# ecss-pus-obsw - build, test and verification entry points
#
#   make            build the flight sources and the host runner
#   make test       build and run every unit test
#   make integration  run the Python functional test suite against obc_sim
#   make trace      regenerate and check the requirements traceability matrix
#   make coverage   run the unit tests with instrumentation and report
#   make static     run cppcheck if it is installed
#   make verify     everything above, in the order the CI job uses
# ---------------------------------------------------------------------------

CC       ?= cc
PYTHON   ?= python3
BUILD    := build

# Statement coverage floor enforced by "make coverage" and by the CI job.
COVERAGE_FLOOR ?= 95

# Flags applied to the flight sources. -Werror is deliberate: a warning in
# flight code is a finding, not a note (see CODING_STANDARD.md rule R-GEN-02).
CSTD     := -std=c99
WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wmissing-prototypes -Wold-style-definition -Wundef \
            -Wpointer-arith -Wcast-align -Wwrite-strings -Wswitch-default \
            -Wconversion -Wsign-conversion -Werror
OPT      ?= -O2 -g
INCLUDES := -Iinclude -Itests/framework
CFLAGS   := $(CSTD) $(WARN) $(OPT) $(INCLUDES)

SRC        := $(wildcard src/*.c)
OBJ        := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(SRC))
TEST_SRC   := $(wildcard tests/unit/test_*.c)
TEST_BIN   := $(patsubst tests/unit/%.c,$(BUILD)/tests/%,$(TEST_SRC))
SUPPORT    := tests/framework/test_support.c

.PHONY: all test integration trace coverage static verify clean sim

all: $(BUILD)/obc_sim

$(BUILD)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obc_sim: $(OBJ) sim/obc_sim.c
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(OPT) $(INCLUDES) -Wall -Wextra -o $@ sim/obc_sim.c $(OBJ)

$(BUILD)/tests/%: tests/unit/%.c $(SRC) $(SUPPORT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(COVFLAGS) -o $@ $< $(SUPPORT) $(SRC)

test: $(TEST_BIN)
	@echo ""
	@fail=0; \
	for t in $(TEST_BIN); do \
	    ./$$t || fail=1; \
	    echo ""; \
	done; \
	if [ $$fail -ne 0 ]; then echo "UNIT TESTS FAILED"; exit 1; fi; \
	echo "All unit tests passed."

integration: $(BUILD)/obc_sim
	$(PYTHON) -m pytest tests/integration -v

trace:
	$(PYTHON) tools/trace_matrix.py --check --output docs/traceability.md

coverage:
	@./tools/coverage.sh $(COVERAGE_FLOOR)

# SWREQ-TMQ-030 is verified by analysis: no dynamic allocation may appear
# anywhere in the flight sources.
static:
	@echo "== checking for dynamic memory allocation =="
	@if grep -nE '\b(malloc|calloc|realloc|free|alloca|strdup)\s*\(' \
	        $(SRC) $(wildcard include/pus/*.h); then \
	    echo "FAIL: dynamic allocation found in the flight sources"; exit 1; \
	else \
	    echo "OK: no dynamic allocation in the flight sources"; \
	fi
	@echo "== cppcheck =="
	@if command -v cppcheck >/dev/null 2>&1; then \
	  cppcheck --quiet --enable=warning,style,performance,portability \
	           --std=c99 --error-exitcode=1 --inline-suppr \
	           --suppress=missingIncludeSystem -Iinclude src sim; \
	  echo "OK: cppcheck clean"; \
	else \
	  echo "cppcheck not installed - skipping (the CI job runs it)"; \
	fi

verify: test static trace integration coverage
	@echo ""
	@echo "Verification complete."

sim: $(BUILD)/obc_sim
	./$(BUILD)/obc_sim

# docs/traceability.md is a committed deliverable, not a build artefact: it is
# what a reviewer reads on the repository page without running anything.
# "make trace" regenerates it in place; clean must not remove it.
clean:
	rm -rf $(BUILD) .pytest_cache tests/integration/.pytest_cache
	find . -name '__pycache__' -type d -exec rm -rf {} + 2>/dev/null || true
