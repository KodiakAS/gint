TEST_BUILD_DIR ?= build
BENCH_BUILD_DIR ?= build-bench
COVERAGE_DIR ?= build-coverage

# Parallel build jobs (auto-detect CPU cores; overridable: `make JOBS=8`)
JOBS ?= $(shell \
  (command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu) \
  || (command -v nproc >/dev/null 2>&1 && nproc) \
  || (command -v getconf >/dev/null 2>&1 && getconf _NPROCESSORS_ONLN) \
  || echo 1)

# Detect if current CMake supports --parallel (>= 3.12)
CMAKE ?= cmake
CMAKE_VER_MM := $(shell $(CMAKE) --version 2>/dev/null | awk 'NR==1{print $$NF}' | cut -d. -f1,2)
CMAKE_MAJOR := $(word 1,$(subst ., ,$(CMAKE_VER_MM)))
CMAKE_MINOR := $(word 2,$(subst ., ,$(CMAKE_VER_MM)))
CMAKE_GE_3 := $(shell if [ -n "$(CMAKE_MAJOR)" ] && [ $(CMAKE_MAJOR) -gt 3 ]; then echo yes; fi)
CMAKE_EQ_3 := $(shell if [ "$(CMAKE_MAJOR)" = 3 ]; then echo yes; fi)
CMAKE_MIN_GE_12 := $(shell if [ -n "$(CMAKE_MINOR)" ] && [ $(CMAKE_MINOR) -ge 12 ]; then echo yes; fi)
CMAKE_PARALLEL_SUPPORTED := $(or $(CMAKE_GE_3),$(and $(CMAKE_EQ_3),$(CMAKE_MIN_GE_12)))
CMAKE_PARALLEL_FLAG := $(if $(CMAKE_PARALLEL_SUPPORTED),--parallel $(JOBS),)

# Tools
GCOVR_BIN := $(shell command -v gcovr 2>/dev/null)
GCOVR ?= $(if $(GCOVR_BIN),$(GCOVR_BIN),python3 -m gcovr)
LLVM_COV ?= $(shell xcrun -f llvm-cov 2>/dev/null || command -v llvm-cov 2>/dev/null)

.PHONY: test bench coverage coverage-gcovr coverage-lcov clean image

$(TEST_BUILD_DIR)/Makefile:
	cmake -S . -B $(TEST_BUILD_DIR) -DGINT_BUILD_TESTS=ON -DGINT_BUILD_BENCHMARKS=OFF

$(BENCH_BUILD_DIR)/Makefile:
	cmake -S . -B $(BENCH_BUILD_DIR) -DGINT_BUILD_TESTS=OFF -DGINT_BUILD_BENCHMARKS=ON

$(COVERAGE_DIR)/Makefile:
	cmake -S . -B $(COVERAGE_DIR) -DENABLE_COVERAGE=ON -DGINT_BUILD_TESTS=ON -DGINT_BUILD_BENCHMARKS=OFF

LCOV_IGNORE_MISMATCH := $(shell geninfo --help 2>&1 | grep -q 'mismatch' && echo '--ignore-errors mismatch')

# Build and run unit tests
test: $(TEST_BUILD_DIR)/Makefile
	cmake --build $(TEST_BUILD_DIR) $(CMAKE_PARALLEL_FLAG)
	cd $(TEST_BUILD_DIR) && ctest --output-on-failure

# Build and run benchmarks

bench: $(BENCH_BUILD_DIR)/Makefile
	cmake --build $(BENCH_BUILD_DIR) $(CMAKE_PARALLEL_FLAG)
	$(BENCH_BUILD_DIR)/perf
	$(BENCH_BUILD_DIR)/perf_compare_int256

# Build, test and generate coverage report
coverage: coverage-lcov

# Build, test and generate coverage via gcovr


coverage-gcovr: $(COVERAGE_DIR)/Makefile
	@echo "[coverage] Building tests with coverage flags..."
	cmake --build $(COVERAGE_DIR) --config Debug $(CMAKE_PARALLEL_FLAG)
	@echo "[coverage] Running unit tests..."
	cd $(COVERAGE_DIR) && ctest --output-on-failure
	@# Verify gcovr is available (either binary or python module)
	@$(GCOVR) --version >/dev/null 2>&1 || { \
		printf "\nERROR: gcovr not found. Install with one of:\n  pipx install gcovr\n  pip3 install --user gcovr\n  brew install gcovr\n\n"; \
		exit 2; \
	}
	@echo "[coverage] Generating gcovr reports (console, HTML, XML, JSON)..."
	$(GCOVR) \
		--gcov-executable "$(LLVM_COV) gcov" \
		--root . \
		--object-directory $(COVERAGE_DIR) \
		--filter 'include/' \
		--exclude 'tests' \
		--exclude '.*googletest.*' \
		--exclude '/usr/.*' \
		--print-summary \
		--html-details $(COVERAGE_DIR)/coverage.html \
		--xml $(COVERAGE_DIR)/coverage.xml \
		--json $(COVERAGE_DIR)/coverage.json
	@echo "[coverage] HTML report: $(COVERAGE_DIR)/coverage.html"

# lcov-based coverage

coverage-lcov: $(COVERAGE_DIR)/Makefile
	cmake --build $(COVERAGE_DIR) --config Debug $(CMAKE_PARALLEL_FLAG)
	cd $(COVERAGE_DIR) && ctest --output-on-failure
	lcov --capture --directory $(COVERAGE_DIR) --output-file $(COVERAGE_DIR)/coverage.info $(LCOV_IGNORE_MISMATCH)
	lcov --remove $(COVERAGE_DIR)/coverage.info '/usr/*' '*/tests/*' --output-file $(COVERAGE_DIR)/coverage.info
	lcov --list $(COVERAGE_DIR)/coverage.info

# Remove build directories
clean:
	rm -rf $(TEST_BUILD_DIR) $(BENCH_BUILD_DIR) $(COVERAGE_DIR)

# Build Docker image
image:
	docker build -t gint:centos8 .
