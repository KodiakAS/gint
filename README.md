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
| Add/NoCarry            | 1.20 |       1.96 |  5.24 |
| Add/FullCarry          | 0.48 |       1.50 |  2.26 |
| Sub/NoBorrow           | 1.43 |       1.93 |  5.56 |
| Sub/FullBorrow         | 0.82 |       1.58 |  2.46 |
| Mul/U64xU64            | 1.88 |       2.26 |  4.26 |
| Mul/HighxHigh          | 2.07 |       3.38 | 11.3  |
| Div/SmallDivisor(32)   | 11.9 |       14.9 |  20.5 |
| Div/Pow2Divisor        | 8.30 |        274 |  62.7 |
| Div/SimilarMagnitude   | 17.6 |        214 |  63.2 |

Highlights
- Add/Sub: ~1.3–3× faster than ClickHouse; ~3–6× than Boost.
- Mul: ~1.4–1.6× faster than ClickHouse; ~2–5× than Boost.
- Div: Strong wins on power‑of‑two and similar‑magnitude; small‑divisors lead across 32‑bit and 64‑bit.

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
