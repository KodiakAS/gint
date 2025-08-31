# 基准测试

本文档将基准测试划分为两大模块：
- 核心算子（Core Ops）：单库内的基本算子性能（加/减/乘/除/ToString 等）。
- 对比测试（Comparison）：gint 对比 ClickHouse 宽整数与 Boost.Multiprecision 的 256 位算术。

测试平台
- Apple Silicon（macOS）
  - 基准报告的核心数：12
  - 工具链：AppleClang 17.x，Google Benchmark
  - 编译：Release（`-O3 -DNDEBUG`）
  - 备注：macOS 上可能无法设置线程亲和性，Google Benchmark 的警告不影响相对对比。
- x86_64 Linux（预留）
  - CPU/OS/工具链：TBD
  - 结果：TBD

## 通用提示
- 最短运行时间：建议加入 `--benchmark_min_time=0.2s`（单位必须为秒）。
- 过滤子集：可用 `--benchmark_filter`（示例：`--benchmark_filter=^Div/SmallDivisor64/`）。

## 核心算子（Core Ops）

### 目的
- 观察同一库在不同位宽与路径下的纵向表现，非库间对比。

### 运行
- 构建：`make bench`（会生成 `build-bench/perf`）
- 执行：`build-bench/perf --benchmark_min_time=0.2s`

### 示例结果（ns/op，越小越好）

| 操作           | 256  | 512  | 1024 |
| -------------- | ---: | ---: | ---: |
| Addition       | 2.04 | 2.06 | 2.17 |
| Subtraction    | 2.07 | 2.50 | 6.12 |
| Multiplication | 3.86 | 9.74 | 62.30 |
| Division       | 2.69 | 4.06 | 10.4 |
| ToString       | 219  | 582  | 1778 |

### 说明
- Core Ops 仅用于纵向观察，具体数值会因硬件/编译器不同而变化。

## 对比测试（Comparison）

### 目的
- 在代表性细分场景下，对比 256 位算术在不同实现（gint/ClickHouse/Boost）间的性能差异。

### 用法
- 标准矩阵（默认，较少但代表性强）：`make bench BENCH_ARGS="--benchmark_min_time=0.2s"`
- 完整矩阵（覆盖更广，本文性能指标以此为准）：
  - `make bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
  - 或 `build-bench/perf_compare_int256 --gint_full --benchmark_min_time=0.2s`

### 方法学
- 每个用例 256 对确定性样本（固定种子），循环中轮询；
- 三方库共享相同输入；
- 使用 `benchmark::DoNotOptimize` 防止常量折叠与分支偏置；
- 倾向覆盖常见“快/慢”路径与真实工作负载热点。

### 结果（完整矩阵，ns/op）

#### 加法（Addition）

**设计**
- NoCarry：低位进位概率低，覆盖“无跨 limb 进位”的最快路径。
- FullCarry：-1 + 1，覆盖“全链条进位”最坏路径。
- CarryChain64：仅低 64 位形成进位链，隔离低层 carry 传播。

| 用例            | gint | ClickHouse | Boost |
| --------------- | ---: | ---------: | ----: |
| NoCarry         | 1.19 |       1.93 |  5.36 |
| FullCarry       | 0.48 |       1.45 |  2.04 |
| CarryChain64    | 1.24 |       2.09 |  5.54 |

#### 减法（Subtraction）

**设计**
- NoBorrow：避免跨 limb 借位，覆盖最快路径。
- FullBorrow：0 − 1，覆盖“全链条借位”最坏路径。
- BorrowChain64：仅低 64 位形成借位链，隔离低层 borrow 传播。

| 用例            | gint | ClickHouse | Boost |
| --------------- | ---: | ---------: | ----: |
| NoBorrow        | 1.50 |       1.90 |  5.36 |
| FullBorrow      | 0.81 |       1.55 |  2.29 |
| BorrowChain64   | 1.50 |       2.09 |  5.39 |

#### 乘法（Multiplication）

**设计**
- U64xU64：小×小，验证单 limb 快路径。
- HighxHigh：高位有值，多 limb 学校乘法路径，评估对角线累加与进位管理。
- U32xWide：32 位 × 宽整数，覆盖“标量×宽整数”的常见热点。

| 用例        | gint | ClickHouse | Boost |
| ----------- | ---: | ---------: | ----: |
| U64xU64     | 1.87 |       2.41 |  4.33 |
| HighxHigh   | 2.02 |       3.28 | 10.9  |
| U32xWide    | 2.23 |       2.79 |  4.55 |

#### 除法（Division）

**设计**
- SmallDivisor32：触发 2^32 基数长除法快路径（gint 专门优化）。
- SmallDivisor64：触发 128/64 的“倒数乘法 + 一次修正”路径（gint 专门优化）。
- Pow2Divisor：除数为 2 的幂，对应移位路径（极端快路径）。
- SimilarMagnitude：被除数与除数数量级相近，触发多 limb 重路径（对算法 D/规范化有代表性）。
- LargeDivisor128：两 limb 除数，直击多 limb 除法的规范化与 qhat 修正质量。
- SimilarMagnitude2：与 SimilarMagnitude 不同分布，用于验证分布对分支预测与规范化的影响。

| 用例                       | gint | ClickHouse | Boost |
| -------------------------- | ---: | ---------: | ----: |
| SmallDivisor32（32 位）    | 11.6 |       14.1 |  20.3 |
| SmallDivisor64（64 位）    | 12.9 |       13.5 |  24.2 |
| Pow2Divisor（2 的幂）      | 8.21 |        403 |  63.2 |
| SimilarMagnitude           | 17.6 |        209 |  61.8 |
| LargeDivisor128（两 limb） | 21.4 |        455 |  34.6 |
| SimilarMagnitude2          | 14.9 |        229 |  17.9 |
