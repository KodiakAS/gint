# 基准测试

本文档介绍性能测试结构：
- **基准测试（Benchmark）**：仅编译并运行 gint 的用例，用于观察绝对性能。
- **对比测试（Comparison）**：在同一套用例上，对比 ClickHouse 宽整数与 Boost.Multiprecision 的 256 位算术。

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

## 基准测试（Benchmark）

### 目的
- 仅运行 gint 自身的用例，评估绝对性能。

### 用法
- 构建并执行：`make bench BENCH_ARGS="--benchmark_min_time=0.2s"`
- 直接运行：`build-bench/perf_benchmark_int256 --benchmark_min_time=0.2s`

### 常用参数
- `--benchmark_min_time=0.2s`：设置最短运行时间以降低抖动。
- `--benchmark_filter=<regex>`：只运行匹配的用例。

## 对比测试（Comparison）

### 目的
- 在代表性细分场景下，对比 256 位算术在不同实现（gint/ClickHouse/Boost）间的性能差异。

### 用法
- 构建并执行：`make bench BENCH_ARGS="--benchmark_min_time=0.2s"`
- 对比测试标准矩阵：`make bench-compare BENCH_ARGS="--benchmark_min_time=0.2s"`
- 对比测试完整矩阵：`make bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
  - 或 `build-bench/perf_compare_int256 --gint_full --benchmark_min_time=0.2s`

### 方法学
- 每个用例 256 对确定性样本（固定种子），循环中轮询；
- 三方库共享相同输入；
- 预生成数据集，循环内仅取引用与计算，不做拷贝/构造；
- 对循环不变操作数与表达式使用 `benchmark::DoNotOptimize`，防止常量折叠或被外提；
- 倾向覆盖常见“快/慢”路径与真实工作负载热点。

### 结果（完整矩阵，ns/op）

#### 加法（Addition）

**设计**
- NoCarry：低位进位概率低，覆盖“无跨 limb 进位”的最快路径。
- FullCarry：-1 + 1，覆盖“全链条进位”最坏路径。
- CarryChain64：仅低 64 位形成进位链，隔离低层 carry 传播。

| 用例            | gint | ClickHouse | Boost |
| --------------- | ---: | ---------: | ----: |
| NoCarry         | 1.17 |       1.71 |  5.48 |
| FullCarry       | 1.17 |       1.52 |  2.11 |
| CarryChain64    | 1.18 |       1.67 |  5.55 |

#### 减法（Subtraction）

**设计**
- NoBorrow：避免跨 limb 借位，覆盖最快路径。
- FullBorrow：0 − 1，覆盖“全链条借位”最坏路径。
- BorrowChain64：仅低 64 位形成借位链，隔离低层 borrow 传播。

| 用例            | gint | ClickHouse | Boost |
| --------------- | ---: | ---------: | ----: |
| NoBorrow        | 1.66 |       1.48 |  5.42 |
| FullBorrow      | 1.67 |       1.58 |  2.36 |
| BorrowChain64   | 1.67 |       1.67 |  5.34 |

#### 乘法（Multiplication）

**设计**
- U64xU64：小×小，验证单 limb 快路径。
- HighxHigh：高位有值，多 limb 学校乘法路径，评估对角线累加与进位管理。
- U32xWide：32 位 × 宽整数，覆盖“标量×宽整数”的常见热点。
- WideTimesU64：完整 256 位 × 64 位的混合乘法，对三方实现进行公平对比（被乘数由四个 64 位随机 limb 组成，避免单肢退化）。

| 用例        | gint | ClickHouse | Boost |
| ----------- | ---: | ---------: | ----: |
| U64xU64     | 1.78 |       2.61 |  2.23 |
| HighxHigh   | 1.80 |       2.62 |  9.76 |
| U32xWide    | 1.79 |       3.46 |  3.99 |
| WideTimesU64| 0.86 |       3.12 |  2.40 |

#### 除法（Division）

**设计**
- SmallDivisor32：被除数为随机 256 位，除数取 32 位范围，覆盖“基数 2^32 的小除数场景”。
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖“128/64 估商的小除数场景”。
- Pow2Divisor：除数为 2 的幂，覆盖“移位等价”的极端快路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖“Knuth D 多 limb 重路径”，考察规范化与估商修正。
- LargeDivisor128：两 limb 除数，覆盖“多 limb 路径在 2‑limb 情况的热点”。
- SimilarMagnitude2：与上述相似量级不同分布，用于验证分布差异对分支与规范化的影响。

**代表性结果（ns/op，Docker 对比）**

| 用例                       | gint | ClickHouse | Boost | 备注 |
| -------------------------- | ---: | ---------: | ----: | ---- |
| SmallDivisor32（32 位）    | 20.5 |       17.4 |  19.4 | 稳定 |
| SmallDivisor64（64 位）    | 19.0 |       17.4 |  21.8 | 稳定 |
| Pow2Divisor（2 的幂）      | 15.6 |        350 |  33.3 | 快路径 |
| SimilarMagnitude           | 19.9 |        352 |  21.5 | 稳定 |
| LargeDivisor128（两 limb） | 37.4 |        808 |  33.1 | 接近 Boost |
| SimilarMagnitude2          | 20.6 |        389 |  18.3 | 明显优于初始基线 |

注：不同机器/工具链会有小幅抖动，上表用于说明量级与相对关系；请以 PR 中的 JSON 产物与同机对比为准。

#### 字符串转换（ToString）

**设计**
- Base10：高位为 1 的逐步递增数，评估十进制字符串转换效率。

| 用例    | gint | ClickHouse | Boost |
| ------- | ---: | ---------: | ----: |
| Base10  | 600  |      2050  |   418 |
