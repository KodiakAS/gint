# gint

[![CI](https://github.com/KodiakAS/gint/actions/workflows/ci.yml/badge.svg)](https://github.com/KodiakAS/gint/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/KodiakAS/gint/branch/main/graph/badge.svg)](https://app.codecov.io/gh/KodiakAS/gint)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

`gint` is a C++11 header-only library for fast, exact-width signed and unsigned
wide integers.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

## Highlights

- **Exact width**: 64-, 128-, 256-, 512-, and 1024-bit signed and unsigned types with two's-complement wraparound.
- **High performance**: architecture-tuned arithmetic for GCC and Clang, backed by codegen contracts and benchmarks.
- **Single-header drop-in**: copy [`include/gint/gint.h`](include/gint/gint.h) with no build step or link-time dependency.
- **C++11 public API**: full functionality on C++11, with optional `fmt` formatting and `-fno-exceptions` support.

## Quick start

```cpp
#include <gint/gint.h>

int main()
{
    gint::UInt256 high = gint::UInt256(1) << 200;
    gint::UInt256 value = high + 42;
    gint::UInt256 divisor = 7;
    gint::divmod_result<gint::UInt256> result = gint::divmod(value, divisor);

    return result.remainder == gint::UInt256(4) ? 0 : 1;
}
```

Copy [`include/gint/gint.h`](include/gint/gint.h) into your include path for
the simplest integration—no generation step or link-time dependency required.

For an installed CMake package:

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

Link `gint::checked` instead to turn division or modulo by zero into an error
path. The single public header includes string and stream support out of the
box; see the [integration guide](docs/INTEGRATION.md) for source-tree
consumption, optional `fmt` support, and `-fno-exceptions` builds.

## Support and documentation

The public API requires C++11 and a supported GCC or Clang toolchain.
See the [support policy](docs/SUPPORT.md) for compiler and platform requirements.

- [Integration](docs/INTEGRATION.md): CMake, configuration, and optional dependencies.
- [Technical specification](docs/TECH_SPEC.md): types, operations, and edge cases.
- [Upgrading](docs/UPGRADING.md) and [changelog](CHANGELOG.md): migration and release changes.
- [Benchmarks](docs/BENCHMARKS.md): reproducible performance measurements.
- [Contributing](CONTRIBUTING.md): source editing, validation, and pull requests.

The [documentation index](docs/README.md) covers all maintainer references.

## License

Licensed under the [Apache License 2.0](LICENSE).
