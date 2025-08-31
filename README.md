# gint

gint (g = *Giant*) brings wide integers to C++11 with a lightweight, performance-first design. It is **header-only** for frictionless adoption, and guarantees **bit-accurate** semantics: the width you declare (e.g., 128/256/512…) is exactly the width you get at runtime.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

**Highlights**

* **C++11 compatible** across diverse toolchains.
* **Header-only** for painless integration.
* **High performance** by design.
* **Bit-accurate width** strictly matching the definition.

## Performance

Environment: Apple Silicon, AppleClang O3, Google Benchmark (`--benchmark_min_time=0.2s`). Numbers are ns/op (lower is better). Absolute values vary by machine; relative trends are stable.

Arithmetic — 256‑bit

| Case                   | gint | ClickHouse | Boost |
| ---------------------- | ---: | ---------: | ----: |
| Add/NoCarry            | 1.17 |       1.71 |  5.48 |
| Add/FullCarry          | 1.17 |       1.52 |  2.11 |
| Sub/NoBorrow           | 1.66 |       1.48 |  5.42 |
| Sub/FullBorrow         | 1.67 |       1.58 |  2.36 |
| Mul/U64xU64            | 1.78 |       2.61 |  2.23 |
| Mul/HighxHigh          | 1.80 |       2.62 |  9.76 |
| Div/SmallDivisor32     | 10.8 |       13.5 |  19.6 |
| Div/Pow2Divisor        | 7.60 |        277 |  62.3 |
| Div/SimilarMagnitude   | 17.7 |        212 |  63.8 |

Highlights
- Add/Sub: ~1.1–1.5× faster vs ClickHouse; ~2.5–4× vs Boost.
- Mul: ~1.4–1.5× faster vs ClickHouse; competitive vs Boost (much faster on high×high).
- Div: Large wins for power-of-two and similar magnitude; strong on 32/64-bit small divisors as well.

Full matrices and methodology: see `docs/BENCHMARKS.md`.


## Quick Start

```cpp
#include <gint/gint.h>

int main() {
    gint::integer<256, unsigned> a = 1;
    gint::integer<256, unsigned> b = 2;
    auto c = (a << 128) + b;
    auto low = static_cast<std::uint64_t>(c);
    // ...
}
```

## Building Tests

```bash
make test
```

## Building Benchmarks

```bash
make bench
```

## Code Coverage

To generate a coverage report locally:

```bash
make coverage
```

This builds the project with coverage flags, runs the tests, and
produces a coverage report under the `build-coverage` directory.

## Documentation

- Technical spec: `docs/TECH_SPEC.md`
- Benchmarks: `docs/BENCHMARKS.md`

## Development Environment

A dedicated Dockerfile sets up a CentOS 8 environment with all build
dependencies preinstalled. The repository's `Makefile` provides a target
to build this image:

```bash
make image
```

The resulting `gint:centos8` image can be used as a consistent
development container.
