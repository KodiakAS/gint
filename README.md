# gint

gint (g = *Giant*) brings wide integers to C++11 with a lightweight, performance-first design. It is **header-only** for frictionless adoption, and guarantees **bit-accurate** semantics: the supported width you declare (`64/128/256/512/1024`) is exactly the width you get at runtime.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

**Highlights**

* **C++11 compatible** across diverse toolchains.
* **Header-only** for painless integration.
* **High performance** by design.
* **Bit-accurate width** strictly matching the definition.

## Performance

Local AppleClang microbenchmark sample (commit `3649c33b`, collected 2026-07-10): Apple M4 Pro, macOS 26.5.2, AppleClang 21.0.0, Release/O3, Google Benchmark v1.9.5 (`BENCH_BITS=256 --gint_full --benchmark_min_time=0.2s --benchmark_repetitions=3`). Numbers are median `real_time` ns/op (lower is better) for hot, in-cache operator throughput on fixed 256-bit inputs. The comparison matrix uses unsigned fixed-width types so every generated bit pattern maps to the same non-negative mathematical value in gint, ClickHouse, and Boost; native object layout remains library-specific. Use the results for same-toolchain regression tracking; Docker/GCC and real workload results should be reported separately.

Arithmetic & ToString — 256-bit

| Case                   | gint | ClickHouse | Boost |
| ---------------------- | ---: | ---------: | ----: |
| Add/NoCarry            | 0.672 |       1.02 |  3.05 |
| Add/FullCarry          | 0.699 |       1.60 |  3.56 |
| Sub/NoBorrow           | 0.697 |      0.960 |  4.40 |
| Sub/FullBorrow         | 0.694 |       1.64 |  3.80 |
| Mul/U64xU64            |  1.86 |       1.62 |  1.91 |
| Mul/HighxHigh          |  1.82 |       1.60 |  10.7 |
| Div/SmallDivisor32     |  8.61 |       11.4 |  19.0 |
| Div/Pow2Divisor        |  3.14 |        316 |  64.3 |
| Div/SimilarMagnitude   |  12.7 |        222 |  64.9 |
| ToString/Base10        |   107 |        297 |   146 |

Highlights
- Add/Sub: faster than ClickHouse on the listed cases by ~1.4-2.4x, and faster than Boost by ~4.5-6.3x.
- Mul: within ~2-15% of ClickHouse on the listed cases, and ~1.0-5.9x faster than Boost.
- Div: large wins for power-of-two and similar-magnitude divisors; still faster on 32-bit small divisors.
- ToString: ~1.4x faster vs Boost; ~2.8x vs ClickHouse.

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

This uses the host toolchain (AppleClang for the sample above) and writes to
`runs/local/`. Use the Docker image when you need Linux/GCC validation.

## Code Coverage

To generate a coverage report on the host AppleClang environment:

```bash
make coverage
```

This builds the project with coverage flags, runs the tests, and
produces a coverage report under the `runs/local/build-coverage` directory.
For Linux/GCC coverage of GCC-tuned paths, run the same target in the Docker
image with `COVERAGE_DIR=runs/docker/build-coverage`.

## Documentation

- Technical spec: `docs/TECH_SPEC.md`
- Benchmarks: `docs/BENCHMARKS.md`
- Validation environments: `docs/VALIDATION_ENVIRONMENTS.md`

## License

Apache License 2.0. See `LICENSE`.

## Development Environment

A dedicated Dockerfile sets up a CentOS 8 Linux/GCC validation environment
with all build dependencies preinstalled. It also installs `clang`/`clangd`
for development tooling and Google Benchmark v1.9.5 for benchmark binaries.
Unless `CC`/`CXX` is explicitly overridden, CMake uses GCC/g++ inside the
container. The repository's `Makefile` provides a target to build this image:

```bash
make image
```

The resulting `gint:centos8` image is intended for GCC/Linux test and
benchmark validation. Host AppleClang artifacts use `runs/local/`; Docker/GCC
artifacts should use `runs/docker/`.
