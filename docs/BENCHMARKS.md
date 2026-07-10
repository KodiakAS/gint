# 基准测试

本文档介绍性能测试结构：
- **基准测试（Benchmark）**：仅编译并运行 gint 的用例，用于观察绝对性能。
- **对比测试（Comparison）**：在同一套用例上，按所选 `BENCH_BITS` 对比 gint、ClickHouse 宽整数与 Boost.Multiprecision；覆盖算术、取模、位运算、移位与字符串转换等用例。

## 环境口径

- 默认本机（local）：指当前 macOS + AppleClang 工具链，构建与输出目录使用 `runs/local/`。
- 其他环境或编译器：使用独立 `runs/<scope>/` 和独立 `BENCH_BUILD_DIR`，例如本机 GCC、Linux GCC、Linux Clang 等。
- 仓库内命令只假定已经位于当前执行环境；具体在哪里运行由外部编排决定。
- 不同架构、OS、编译器结果应分别呈现，不直接混合横向比较。
- 三方 comparison 矩阵统一使用 unsigned 固定位宽类型，使相同输入 bit pattern 在 gint、ClickHouse 与 Boost 中表示相同的非负数学值；benchmark 启动时会对最高位和全 1 边界值做三方一致性校验。
- 各库保持自身原生对象布局，comparison 衡量的是实际库类型的 operator 吞吐，并不声称对象大小或内部表示已经归一化。
- 可复现验证环境的初始化方式见 [验证环境固化](VALIDATION_ENVIRONMENTS.md)。该文档只固化当前环境依赖，不固定具体测试项目、过滤器或位宽。

## 本机 AppleClang 样本（commit `3649c33b`）
- 采集时间：2026-07-10
- 采集平台：Apple M4 Pro，macOS 26.5.2，arm64
- 核心数：12
- 工具链：AppleClang 21.0.0，Google Benchmark v1.9.5
- 编译：Release（`-O3 -DNDEBUG`）
- 参数：`BENCH_BITS=256 --gint_full --benchmark_min_time=0.2s --benchmark_repetitions=3`
- 统计口径：3 次重复的 median `real_time`
- 备注：macOS 上可能无法设置线程亲和性，Google Benchmark 的警告不影响相对对比。

## 通用提示
- 本机 AppleClang 口径下，Makefile 默认构建目录位于 `runs/local/`：测试 `runs/local/build`，基准 `runs/local/build-bench`，覆盖率 `runs/local/build-coverage`。
- 非默认环境验证需显式指定独立 `BENCH_BUILD_DIR`，产物放在对应 `runs/<scope>/`。
- 最短运行时间：建议加入 `--benchmark_min_time=0.2s`（单位必须为秒）。
- 过滤子集：可用 `--benchmark_filter`（示例：`--benchmark_filter=^Div/SmallDivisor64/`）。
- 完整矩阵：使用 `bench-full` 或 `bench-compare-full`，等价于给可执行传入 `--gint_full`。
- 位宽选择：Makefile 默认 `BENCH_BITS=256`；可设为 `128/256/512/1024`，例如 `make BENCH_BITS=512 bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`。

## 基准测试（Benchmark）

### 目的
- 仅运行 gint 自身的用例，评估绝对性能。

### 用法
- 本机 AppleClang 标准矩阵：`make bench BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机 AppleClang 完整矩阵：`make bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 其他环境完整矩阵：`make BENCH_BUILD_DIR=runs/<scope>/build-bench bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机直接运行：`runs/local/build-bench/perf_benchmark_int256 --benchmark_min_time=0.2s`
- 其他位宽：`make BENCH_BITS=512 bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`

### 常用参数
- `--benchmark_min_time=0.2s`：设置最短运行时间以降低抖动。
- `--benchmark_filter=<regex>`：只运行匹配的用例。

## 对比测试（Comparison）

### 目的
- 在代表性细分场景下，对比所选位宽在不同实现（gint/ClickHouse/Boost）间的性能差异；默认位宽为 256，可通过 `BENCH_BITS` 切换到 `128/512/1024`。

### 用法
- 本机 AppleClang 标准矩阵：`make bench-compare BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机 AppleClang 完整矩阵：`make bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 其他环境完整矩阵：`make BENCH_BUILD_DIR=runs/<scope>/build-bench bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机直接运行：`runs/local/build-bench/perf_compare_int256 --gint_full --benchmark_min_time=0.2s`
- 其他位宽：`make BENCH_BITS=1024 bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`

### 方法学
- 每个用例 256 对确定性样本（固定种子），循环中轮询；
- 三方库共享相同输入；
- 启动时校验三方输入在最高位和全 1 边界值上的数学值一致；计时循环不逐项比较运算结果，gint 正确性由单元测试覆盖；
- 预生成数据集，循环内仅取引用与计算，不做拷贝/构造；
- 对循环不变操作数与表达式使用 `benchmark::DoNotOptimize`，防止常量折叠或被外提；
- 倾向覆盖常见“快/慢”路径与真实工作负载热点。
- 当前结果是热数据、独立操作的 microbenchmark 吞吐；数据集很小，通常驻留在 L1/L2 cache 中，不代表冷数据访问、依赖链延迟或端到端业务性能。跨工具链或跨机器比较应重新采集并单独报告。

### 结果（完整矩阵，real_time ns/op）

数值越低越好。以下数据来自 `bench-compare-full` 的本机 AppleClang 256-bit 样本（commit `3649c33b`），取 3 次重复的 median `real_time`；如需复现，应按上文命令重新采集当前工作区的本地结果。

#### 加法（Addition）

**设计**
- NoCarry：低位进位概率低，覆盖“无跨 limb 进位”的最快路径。
- FullCarry：全 1 位模式 + 1，覆盖“全链条进位”最坏路径。
- CarryChain64：仅低 64 位形成进位链，隔离低层 carry 传播。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| NoCarry      | 0.672 |       1.02 |  3.05 |
| FullCarry    | 0.699 |       1.60 |  3.56 |
| CarryChain64 | 0.699 |       1.17 |  3.37 |

#### 减法（Subtraction）

**设计**
- NoBorrow：避免跨 limb 借位，覆盖最快路径。
- FullBorrow：0 - 1，覆盖“全链条借位”最坏路径。
- BorrowChain64：仅低 64 位形成借位链，隔离低层 borrow 传播。

| 用例          | gint  | ClickHouse | Boost |
| ------------- | ----: | ---------: | ----: |
| NoBorrow      | 0.697 |      0.960 |  4.40 |
| FullBorrow    | 0.694 |       1.64 |  3.80 |
| BorrowChain64 | 0.697 |       1.17 |  4.24 |

#### 乘法（Multiplication）

**设计**
- U64xU64：小 x 小，验证单 limb 快路径。
- HighxHigh：高位有值，多 limb 学校乘法路径，评估对角线累加与进位管理。
- WideTimesU64：完整 256 位 x 64 位的混合乘法，对三方实现进行公平对比（被乘数由四个 64 位随机 limb 组成，避免单肢退化）。
- U32xWide：32 位 x 宽整数，覆盖“标量 x 宽整数”的常见热点。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| U64xU64      | 1.86 |       1.62 |  1.91 |
| HighxHigh    | 1.82 |       1.60 |  10.7 |
| WideTimesU64 | 0.916 |      0.892 |  1.82 |
| U32xWide     | 1.83 |       1.60 |  2.84 |

#### 除法（Division）

**设计**
- SmallDivisor32：被除数为随机 256 位，除数取 32 位范围，覆盖“基数 2^32 的小除数场景”。
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖“128/64 估商的小除数场景”。
- Pow2Divisor：除数为 2 的幂，覆盖“移位等价”的极端快路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖 256 位满宽除数的单 quotient-limb 快路径，考察规范化与估商修正。
- LargeDivisor128：两 limb 除数，覆盖“多 limb 路径在 2-limb 情况的热点”。
- SimilarMagnitude2：与上述相似量级不同分布，同样覆盖满宽除数快路径，用于验证分布差异对分支与规范化的影响。

| 用例                       | gint | ClickHouse | Boost |
| -------------------------- | ---: | ---------: | ----: |
| SmallDivisor32（32 位）    | 8.61 |       11.4 |  19.0 |
| SmallDivisor64（64 位）    | 12.6 |       13.1 |  21.1 |
| Pow2Divisor（2 的幂）      | 3.14 |        316 |  64.3 |
| SimilarMagnitude           | 12.7 |        222 |  64.9 |
| LargeDivisor128（两 limb） | 16.5 |        547 |  34.2 |
| SimilarMagnitude2          | 9.72 |        206 |  80.0 |

#### 取模（Modulo）

**设计**
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖小除数取模路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖满宽除数取模路径。

| 用例             | gint | ClickHouse | Boost |
| ---------------- | ---: | ---------: | ----: |
| SmallDivisor64   | 12.0 |       14.8 |  20.1 |
| SimilarMagnitude | 16.6 |        226 |  67.1 |

#### 位运算（Bitwise）

**设计**
- And：随机 256 位值按位与。
- Xor：随机 256 位值按位异或。

| 用例 | gint  | ClickHouse | Boost |
| ---- | ----: | ---------: | ----: |
| And  | 0.370 |      0.384 |  5.14 |
| Xor  | 0.371 |      0.385 |  4.92 |

#### 移位（Shift）

**设计**
- LeftVariable：随机 256 位值按变量位移量左移。
- RightVariable：随机 256 位值按变量位移量右移。

| 用例          | gint | ClickHouse | Boost |
| ------------- | ---: | ---------: | ----: |
| LeftVariable  | 1.44 |       3.34 |  6.50 |
| RightVariable | 1.39 |       1.87 |  2.52 |

#### 字符串转换（ToString）

**设计**
- Base10：高位为 1 的逐步递增数，评估十进制字符串转换效率。

| 用例   | gint | ClickHouse | Boost |
| ------ | ---: | ---------: | ----: |
| Base10 |  107 |        297 |   146 |
