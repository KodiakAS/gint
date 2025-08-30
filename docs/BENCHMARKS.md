# Benchmarks

Aggregated performance results for gint (migrated from `bench/RESULTS.md`).

Test Platforms
- Apple Silicon (macOS)
  - Cores reported by benchmark: 12
  - Toolchain: AppleClang 17.x, Google Benchmark
  - Flags: Release (`-O3 -DNDEBUG`)
  - Note: Thread affinity may be unavailable on macOS; benchmark warns this does not affect relative comparisons.
- x86_64 Linux (reserved)
  - CPU/OS/Toolchain: TBD
  - Results: TBD

## perf — core ops on 256/512/1024 (ns)

Command: `make bench && build-bench/perf`

Operation results (ns, lower is better):

| Operation      | 256  | 512  | 1024  |
| -------------- | ----:| ----:| -----:|
| Addition       | 2.04 | 2.06 |  2.17 |
| Subtraction    | 2.07 | 2.50 |  6.12 |
| Multiplication | 3.86 | 9.74 | 62.30 |
| Division       | 2.70 | 5.20 | 13.70 |
| ToString       | 715  | 2457 | 15326 |

## perf_compare_int256 — vs Boost.Multiprecision (ns)

Command: `make bench && build-bench/perf_compare_int256`

Mixed operands (ns):

| Case  | gint | Boost |
| ----- | ---: | ----: |
| Add   | 1.16 |  3.20 |
| Sub   | 1.48 |  3.65 |
| Mul   | 1.69 |  8.74 |
| Div   | 5.69 |   192 |

Small operands (ns):

| Case  | gint | Boost |
| ----- | ---: | ----: |
| Add   | 1.14 |  1.44 |
| Sub   | 1.19 |  1.71 |
| Mul   | 1.22 |  2.45 |
| Div   | 3.08 |  5.87 |

Large operands (ns):

| Case  | gint | Boost |
| ----- | ---: | ----: |
| Add   | 1.15 |  3.41 |
| Sub   | 1.41 |  3.66 |
| Mul   | 1.64 |  7.96 |
| Div   | 23.2 |   189 |

Notes
- Absolute numbers vary by machine; relative trends are stable across runs.
- 256‑bit gint is consistently faster than Boost for mixed and large operands; Boost may be closer on some small‑operand cases.
