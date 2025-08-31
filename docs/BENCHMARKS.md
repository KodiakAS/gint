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

## perf_compare_int256 — vs ClickHouse wide::integer<256> (ns)

Command: `make bench && build-bench/perf_compare_int256`

Mixed operands (ns):

| Case  | gint | ClickHouse |
| ----- | ---: | ----: |
| Add   | 2.57 |  3.63 |
| Sub   | 2.15 |  2.99 |
| Mul   | 7.28 | 11.8 |
| Div   | 35.2 |  977 |

Small operands (ns):

| Case  | gint | ClickHouse |
| ----- | ---: | ----: |
| Add   | 2.53 |  2.15 |
| Sub   | 2.10 |  2.52 |
| Mul   | 7.18 | 10.5 |
| Div   | 8.83 | 22.4 |

Large operands (ns):

| Case  | gint | ClickHouse |
| ----- | ---: | ----: |
| Add   | 2.53 |  2.50 |
| Sub   | 2.13 |  2.59 |
| Mul   | 7.19 | 10.3 |
| Div   | 54.2 |  971 |

Similar magnitude operands (ns):

| Case  | gint | ClickHouse |
| ----- | ---: | ----: |
| Div   | 38.7 |  473 |

Notes
- Absolute numbers vary by machine; relative trends are stable across runs.
- 256‑bit gint is consistently faster than ClickHouse for mixed、large、and similar‑magnitude division; ClickHouse may be closer on some small‑operand cases.
