# 基准测试

本文档介绍性能测试结构：
- **基准测试（Benchmark）**：仅编译并运行 gint 的用例，用于观察绝对性能。
- **对比测试（Comparison）**：在同一套用例上，按所选 `BENCH_BITS` 对比 gint、ClickHouse 宽整数与 Boost.Multiprecision；覆盖算术、取模、位运算、移位与字符串转换等用例。

## 环境口径

- 默认本机（local）：指当前 macOS + AppleClang 工具链，构建与输出目录使用 `runs/local/`。
- 其他环境或编译器：使用独立 `runs/<scope>/` 和独立 `BENCH_BUILD_DIR`，例如本机 GCC、Linux GCC、Linux Clang 等。
- 仓库内命令只假定已经位于当前执行环境；具体在哪里运行由外部编排决定。
- 不同架构、OS、编译器结果应分别呈现，不直接混合横向比较。
- 可复现验证环境的初始化方式见 [验证环境固化](VALIDATION_ENVIRONMENTS.md)。该文档只固化当前环境依赖，不固定具体测试项目、过滤器或位宽。

## 本机 AppleClang 样本（commit `eae8c0b`）
- 采集时间：2026-06-11
- 采集平台：Apple M4 Pro，macOS 26.5.1，arm64
- 核心数：12
- 工具链：AppleClang 21.0.0，Google Benchmark v1.9.5
- 编译：Release（`-O3 -DNDEBUG`）
- 参数：`BENCH_BITS=256 --gint_full --benchmark_min_time=0.2s`
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
- 对比测试只衡量吞吐，不校验三方结果等价；gint 正确性由单元测试覆盖；
- 预生成数据集，循环内仅取引用与计算，不做拷贝/构造；
- 对循环不变操作数与表达式使用 `benchmark::DoNotOptimize`，防止常量折叠或被外提；
- 倾向覆盖常见“快/慢”路径与真实工作负载热点。
- 当前结果是热数据、独立操作的 microbenchmark 吞吐；数据集很小，通常驻留在 L1/L2 cache 中，不代表冷数据访问、依赖链延迟或端到端业务性能。跨工具链或跨机器比较应重新采集并单独报告。

### 结果（完整矩阵，real_time ns/op）

数值越低越好。以下数据来自 `bench-compare-full` 的本机 AppleClang 256-bit 样本（commit `eae8c0b`）；如需复现，应按上文命令重新采集当前工作区的本地结果。

#### 加法（Addition）

**设计**
- NoCarry：低位进位概率低，覆盖“无跨 limb 进位”的最快路径。
- FullCarry：-1 + 1，覆盖“全链条进位”最坏路径。
- CarryChain64：仅低 64 位形成进位链，隔离低层 carry 传播。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| NoCarry      | 0.666 |       1.11 |  4.37 |
| FullCarry    | 0.695 |       1.62 |  1.88 |
| CarryChain64 | 0.688 |       1.15 |  4.33 |

#### 减法（Subtraction）

**设计**
- NoBorrow：避免跨 limb 借位，覆盖最快路径。
- FullBorrow：0 - 1，覆盖“全链条借位”最坏路径。
- BorrowChain64：仅低 64 位形成借位链，隔离低层 borrow 传播。

| 用例          | gint  | ClickHouse | Boost |
| ------------- | ----: | ---------: | ----: |
| NoBorrow      | 0.691 |      0.944 |  4.38 |
| FullBorrow    | 0.691 |       1.62 |  1.52 |
| BorrowChain64 | 0.690 |       1.18 |  4.15 |

#### 乘法（Multiplication）

**设计**
- U64xU64：小 x 小，验证单 limb 快路径。
- HighxHigh：高位有值，多 limb 学校乘法路径，评估对角线累加与进位管理。
- WideTimesU64：完整 256 位 x 64 位的混合乘法，对三方实现进行公平对比（被乘数由四个 64 位随机 limb 组成，避免单肢退化）。
- U32xWide：32 位 x 宽整数，覆盖“标量 x 宽整数”的常见热点。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| U64xU64      | 1.80 |       1.89 |  2.28 |
| HighxHigh    | 1.83 |       2.79 |  11.5 |
| WideTimesU64 | 1.53 |       3.29 |  2.52 |
| U32xWide     | 1.80 |       2.40 |  3.65 |

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
| SmallDivisor32（32 位）    | 10.8 |       14.0 |  20.2 |
| SmallDivisor64（64 位）    | 13.5 |       13.5 |  21.9 |
| Pow2Divisor（2 的幂）      | 4.85 |        310 |  67.4 |
| SimilarMagnitude           | 13.0 |        226 |  68.0 |
| LargeDivisor128（两 limb） | 18.1 |        546 |  37.1 |
| SimilarMagnitude2          | 9.91 |        208 |  81.7 |

#### 取模（Modulo）

**设计**
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖小除数取模路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖满宽除数取模路径。

| 用例             | gint | ClickHouse | Boost |
| ---------------- | ---: | ---------: | ----: |
| SmallDivisor64   | 11.7 |       15.0 |  20.5 |
| SimilarMagnitude | 16.7 |        227 |  67.5 |

#### 位运算（Bitwise）

**设计**
- And：随机 256 位值按位与。
- Xor：随机 256 位值按位异或。

| 用例 | gint  | ClickHouse | Boost |
| ---- | ----: | ---------: | ----: |
| And  | 0.364 |      0.379 |  6.57 |
| Xor  | 0.360 |      0.430 |  7.07 |

#### 移位（Shift）

**设计**
- LeftVariable：随机 256 位值按变量位移量左移。
- RightVariable：随机 256 位值按变量位移量右移。

| 用例          | gint | ClickHouse | Boost |
| ------------- | ---: | ---------: | ----: |
| LeftVariable  | 1.31 |       3.36 |  5.67 |
| RightVariable | 1.35 |       3.07 |  3.26 |

#### 字符串转换（ToString）

**设计**
- Base10：高位为 1 的逐步递增数，评估十进制字符串转换效率。

| 用例   | gint | ClickHouse | Boost |
| ------ | ---: | ---------: | ----: |
| Base10 |  106 |        294 |   146 |
