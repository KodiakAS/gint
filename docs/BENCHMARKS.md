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

| Operation      | 256  | 512  | 1024 |
| -------------- | ---: | ---: | ---: |
| Addition       | 2.04 | 2.06 | 2.17 |
| Subtraction    | 2.07 | 2.50 | 6.12 |
| Multiplication | 3.86 | 9.74 | 62.30 |
| Division       | 2.69 | 4.06 | 10.4 |
| ToString       | 219  | 582  | 1778 |

## perf_compare_int256 — gint vs ClickHouse vs Boost (ns)

Command: `make bench && build-bench/perf_compare_int256`

Mixed operands (ns):

| Case  | gint | ClickHouse | Boost |
| ----- | ---: | ---------: | ----: |
| Add   | 2.58 |      3.82 |  9.34 |
| Sub   | 2.42 |      2.76 |  9.57 |
| Mul   | 7.74 |      12.9 |  19.2 |
| Div   | 32.1 |       959 |   152 |

Small operands (ns):

| Case  | gint | ClickHouse | Boost |
| ----- | ---: | ---------: | ----: |
| Add   | 2.80 |      2.09 |  2.76 |
| Sub   | 2.41 |      2.82 |  5.48 |
| Mul   | 7.26 |      11.1 |  3.14 |
| Div   | 19.6 |      52.7 |  30.0 |

Large operands (ns):

| Case  | gint | ClickHouse | Boost |
| ----- | ---: | ---------: | ----: |
| Add   | 2.78 |      2.74 |  8.62 |
| Sub   | 2.43 |      2.83 |  9.43 |
| Mul   | 7.29 |      11.2 |  21.1 |
| Div   | 107  |       950 |   158 |

Similar magnitude operands (ns):

| Case  | gint | ClickHouse | Boost |
| ----- | ---: | ---------: | ----: |
| Div   | 67.5 |       483 |  80.5 |

Notes
- Absolute numbers vary by machine; relative trends are stable across runs.
- 256‑bit gint is consistently faster than ClickHouse for mixed、large、and similar‑magnitude division; ClickHouse may be closer on some small‑operand cases.
