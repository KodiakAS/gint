# Performance benchmarks

This directory contains several microbenchmarks for `gint`. Build them
with:

```bash
cmake -S . -B build-release-bench -DWI_BUILD_TESTS=OFF -DWI_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-release-bench --config Release -j$(nproc)
```

## perf

Exercises the header for basic arithmetic on unsigned integers of 256,
512 and 1024 bits.

To run:

```bash
./build-release-bench/perf --benchmark_min_time=0.01s
```

Sample output:

```text
BM_Addition<256>              3.29 ns         3.29 ns      4646943
BM_Addition<512>              3.70 ns         3.70 ns      3052343
BM_Addition<1024>             7.92 ns         7.92 ns      1799859
BM_Subtraction<256>           3.31 ns         3.31 ns      4322734
BM_Subtraction<512>           5.27 ns         5.27 ns      2665971
BM_Subtraction<1024>          12.0 ns         12.0 ns      1247072
BM_Multiplication<256>        6.06 ns         6.06 ns      2488839
BM_Multiplication<512>        47.6 ns         47.6 ns       306609
BM_Multiplication<1024>        177 ns          177 ns        80376
BM_Division<256>              3.46 ns         3.46 ns      4485783
BM_Division<512>              11.6 ns         11.6 ns      1219197
BM_Division<1024>             30.1 ns         30.1 ns       475090
BM_ToString<256>              2249 ns         2250 ns         6356
BM_ToString<512>              7183 ns         7184 ns         1923
BM_ToString<1024>            31816 ns        31819 ns          365
```

## perf_compare_int256

Compares the implementation of `gint::integer<256, signed>` against
Boost.Multiprecision's `int256_t` across different operand sizes and sign
combinations.

To run:

```bash
./build-release-bench/perf_compare_int256 --benchmark_min_time=0.01s
```

Sample output:

```text
Add/Mixed/Wide        2.77 ns         2.77 ns      4713990
Add/Mixed/Boost       8.53 ns         8.53 ns      1547597
Sub/Small/Wide        4.38 ns         4.38 ns      3229836
Sub/Small/Boost       4.15 ns         4.15 ns      3483821
Sub/Large/Wide        4.24 ns         4.24 ns      3280684
Sub/Large/Boost       9.37 ns         9.37 ns      1562950
Sub/Mixed/Wide        4.17 ns         4.17 ns      3170684
Sub/Mixed/Boost       7.46 ns         7.45 ns      1766918
Mul/Small/Wide        4.10 ns         4.10 ns      3498340
Mul/Small/Boost       3.65 ns         3.65 ns      5048993
Mul/Large/Wide        7.91 ns         7.91 ns      1717055
Mul/Large/Boost       21.1 ns         21.1 ns       730990
Mul/Mixed/Wide        7.85 ns         7.85 ns      1694435
Mul/Mixed/Boost       25.1 ns         25.1 ns       713768
Div/Small/Wide        4.95 ns         4.95 ns      3097557
Div/Small/Boost       15.5 ns         15.5 ns       945465
Div/Large/Wide        52.0 ns         52.0 ns       269106
Div/Large/Boost       87.7 ns         87.7 ns       159911
Div/Mixed/Wide        24.4 ns         24.4 ns       480030
Div/Mixed/Boost        111 ns          111 ns       159834
```

`gint` is faster than Boost for most large or mixed 256‑bit operations,
while Boost can still win on small-input multiplication and subtraction.

