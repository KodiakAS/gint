# gint

[![CI](https://github.com/KodiakAS/gint/actions/workflows/ci.yml/badge.svg)](https://github.com/KodiakAS/gint/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/KodiakAS/gint/branch/main/graph/badge.svg)](https://app.codecov.io/gh/KodiakAS/gint)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

`gint` is a performance-first, header-only C++11 library for exact-width wide
integers. It provides signed and unsigned 64-, 128-, 256-, 512-, and 1024-bit
types with two's-complement representation, wraparound at the declared width,
and optimized arithmetic for GCC and Clang toolchains.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

## Highlights

- **High performance**: optimized wide-integer arithmetic for GCC and Clang.
- **C++11**: use the full public API without requiring a newer language standard.
- **Header-only**: copy a single header, with no library to build or link.
- **Exact width**: 64-, 128-, 256-, 512-, and 1024-bit integers with wraparound at the declared width.

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
the simplest integration. No generation step or link-time dependency is
required.

For an installed CMake package:

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

Use `gint::checked` to enable checked division by zero. The single public
header always includes string and stream support. See the
[integration guide](docs/INTEGRATION.md) for source
tree consumption, optional `fmt` support, and exception-free builds.

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
