# gint

gint (g = *Giant*) brings wide integers to C++11 with a lightweight, performance-first design. It is **header-only** for frictionless adoption, and guarantees **bit-accurate** semantics: the supported width you declare (`64/128/256/512/1024`) is exactly the width you get at runtime.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

**Highlights**

* **C++11 compatible** across diverse toolchains.
* **Header-only** for painless integration.
* **High performance** by design.
* **Bit-accurate width** strictly matching the definition.

## Performance

Local AppleClang microbenchmark sample: Apple M4 Pro, macOS 26.4.1, AppleClang 21.0.0, Release/O3, Google Benchmark (`--benchmark_min_time=0.2s`). Numbers are `real_time` ns/op (lower is better) for hot, in-cache operator throughput on fixed 256-bit inputs. Use them for same-toolchain regression tracking; Docker/GCC and real workload results should be reported separately.

Arithmetic & ToString — 256‑bit

| Case                   | gint | ClickHouse | Boost |
| ---------------------- | ---: | ---------: | ----: |
| Add/NoCarry            | 0.664 |       1.78 |  4.80 |
| Add/FullCarry          | 0.669 |       1.78 |  2.30 |
| Sub/NoBorrow           | 0.680 |       1.79 |  4.88 |
| Sub/FullBorrow         | 0.669 |       1.83 |  2.56 |
| Mul/U64xU64            | 1.79 |       2.54 |  2.28 |
| Mul/HighxHigh          | 1.83 |       2.55 | 10.9 |
| Div/SmallDivisor32     | 11.5 |       14.5 | 20.6 |
| Div/Pow2Divisor        | 6.82 |        314 | 68.0 |
| Div/SimilarMagnitude   | 16.2 |        232 | 68.0 |
| ToString/Base10        |  129 |        294 |  144 |

Highlights
- Add/Sub: ~2.4-2.7x faster vs ClickHouse; ~3.4-7.2x vs Boost.
- Mul: faster than ClickHouse and Boost on the listed 256-bit cases, with the largest gain on high x high.
- Div: large wins for power-of-two and similar-magnitude divisors; still faster on 32-bit small divisors.
- ToString: ~1.1x faster vs Boost; ~2.3x vs ClickHouse.

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

## Documentation

- Technical spec: `docs/TECH_SPEC.md`
- Benchmarks: `docs/BENCHMARKS.md`

## Development Environment

A dedicated Dockerfile sets up a CentOS 8 Linux/GCC validation environment
with all build dependencies preinstalled. It also installs `clang`/`clangd`
for development tooling, but unless `CC`/`CXX` is explicitly overridden, CMake
uses GCC/g++ inside the container. The repository's `Makefile` provides a
target to build this image:

```bash
make image
```

The resulting `gint:centos8` image is intended for GCC/Linux test and
benchmark validation. Host AppleClang artifacts use `runs/local/`; Docker/GCC
artifacts should use `runs/docker/`.
