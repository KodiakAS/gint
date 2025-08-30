# gint

gint (g = *Giant*) brings wide integers to C++11 with a lightweight, performance-first design. It is **header-only** for frictionless adoption, and guarantees **bit-accurate** semantics: the width you declare (e.g., 128/256/512…) is exactly the width you get at runtime.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

**Highlights**

* **C++11 compatible** across diverse toolchains.
* **Header-only** for painless integration.
* **High performance** by design.
* **Bit-accurate width** strictly matching the definition.

## Performance

- 256‑bit vs Boost (mixed operands):

  | Operation       | `gint` | Boost.Multiprecision |
  | --------------- | -----: | -------------------: |
  | Addition        |  1.16  |                3.20 |
  | Subtraction     |  1.48  |                3.65 |
  | Multiplication  |  1.69  |                8.74 |
  | Division        |  5.69  |                 192 |

Full results and instructions: see `docs/BENCHMARKS.md`.


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
