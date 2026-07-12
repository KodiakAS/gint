# gint

[![CI](https://github.com/KodiakAS/gint/actions/workflows/ci.yml/badge.svg)](https://github.com/KodiakAS/gint/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

`gint` is a performance-first, header-only C++11 library for exact-width wide
integers. It provides signed and unsigned 64-, 128-, 256-, 512-, and 1024-bit
types with two's-complement representation, wraparound at the declared width,
and optimized arithmetic for GCC and Clang toolchains.

Co-maintained by me and **OpenAI Codex** — with a little inspiration from a hippo 🦛.

## Highlights

- Exact-width storage and modulo-2^N arithmetic.
- Header-only integration with no library to build or link.
- A C++11 public API with native integer and `__int128` interoperability.
- Optional arithmetic-only and checked-division interfaces.
- Specialized hot paths backed by reproducible code-generation and performance
  validation.

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
the simplest integration. No generated header or link-time dependency is
required.

The committed `gint.h` is deterministically generated from the normal C++
header graph rooted at `src/gint/gint.hpp`. The generator recursively expands
repository-local quoted includes, following the established amalgamation model
used by mature header-only libraries such as
[nlohmann/json](https://github.com/nlohmann/json/tree/develop/tools/amalgamate):
normal headers are the development source of truth, while a committed flattened
header is the distribution artifact. Each internal `.hpp` remains directly
understandable by clangd, IDEs, and static-analysis tools. This is a maintainer
workflow, not a consumer build step: source checkouts, `FetchContent`, installed
packages, and single-header copies all use the already generated header without
Python. Contributors run `make amalgamate` and verify both the internal headers
and synchronization with `make internal-headers-check amalgamate-check`. Test
configurations also build `gint_internal_header_graph`, so the exported
`compile_commands.json` contains a real C++11 translation unit rooted in the
internal graph as a canonical language-service context. clangd still chooses
commands for headers heuristically, so this improves command inference without
claiming that every editor session must select that target.

For an installed CMake package:

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

Use `gint::checked` to enable checked division by zero. Arithmetic-only
translation units may vendor both public headers and include
`<gint/core.h>`. See the [integration guide](docs/INTEGRATION.md) for source
tree consumption, optional `fmt` support, and exception-free builds.

## Support

The public API requires C++11. Official targets are 64-bit little-endian Linux
x86_64/AArch64 with GCC or LLVM Clang, and macOS arm64 with AppleClang. MSVC
and `clang-cl` are not supported. GCC 4.8.5 is the compatibility floor only
for Linux x86_64. See the [support policy](docs/SUPPORT.md) for the maintained
matrix.

## Performance

Performance is a primary design goal. In a local 256-bit sample from source
commit `1cd05a1` on Apple M4 Pro with AppleClang 21 (seven repetitions, median
`real_time`), gint was faster than Boost in all 22 default comparison scenarios
and faster than ClickHouse in 17 of 22. Its strongest leads were in add/sub,
wide division and modulo, shifts, and decimal conversion.

Results are machine- and toolchain-specific. See the
[benchmark guide](docs/BENCHMARKS.md) for the measurement contract,
reproducible commands, and comparison caveats.

## Documentation

| Document | Purpose |
| --- | --- |
| [Documentation index](docs/README.md) | Guide to the documentation set |
| [Integration](docs/INTEGRATION.md) | Headers, CMake targets, configuration, and dependencies |
| [Technical specification](docs/TECH_SPEC.md) | Public semantics and edge cases |
| [Support policy](docs/SUPPORT.md) | Compilers, platforms, and version policy |
| [Benchmarks](docs/BENCHMARKS.md) | Methodology, commands, and result interpretation |
| [Upgrading](docs/UPGRADING.md) | Consumer migration notes |
| [Changelog](CHANGELOG.md) | User-visible changes |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow and
validation expectations.

## License

Licensed under the [Apache License 2.0](LICENSE).
