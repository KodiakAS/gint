# AGENTS.md

This project is a **high-performance wide-integer library**. It aims to provide a fixed-width, strictly-defined, and easy-to-embed implementation that integrates cleanly into other C++ projects.

---

## Key Features

1. **C++11-first**
   Written against the C++11 standard to maximize compatibility.

2. **Header-only**
   No separate build step required—just include the headers.

3. **Strict bit-width**
   The actual width equals the template parameter. For example, `gint::integer<256, signed>` (aka `gint::Int256`) is exactly 256 bits with no hidden padding.

4. **Interop with native integers**
   Supports arithmetic with and conversions to/from standard C++ integer types.

---

## Project Layout

* `include/` — library headers
* `tests/` — unit tests (GoogleTest)
* `bench/` — benchmarks (Google Benchmark)
* `docs/` — documents

---

## Code Style

Run `clang-format` after editing. The configuration is `.clang-format` at the repository root and applies to C++ sources only.

---

## Pull Request Guidelines

1. **Branch names** must be in English.
2. **Every feature** must come with unit tests.
3. **Every performance optimization** must include before/after benchmark results.
