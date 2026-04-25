# 基准测试

本文档介绍性能测试结构：
- **基准测试（Benchmark）**：仅编译并运行 gint 的用例，用于观察绝对性能。
- **对比测试（Comparison）**：在同一套用例上，对比 ClickHouse 宽整数与 Boost.Multiprecision 的 256 位算术。

## 环境口径

- 本机（local）：指宿主机 macOS + AppleClang 工具链，构建与输出目录使用 `runs/local/`。
- Docker/GCC：指 `gint:centos8` 容器内的 CentOS 8 + GCC/g++ 工具链，构建与输出目录使用 `runs/docker/`。Dockerfile 也安装 `clang`/`clangd` 作为开发辅助工具，但未显式设置 `CC`/`CXX` 时，CMake 默认使用 GCC/g++。
- 两套结果用于验证不同 OS/编译器组合；性能报告应分别呈现，不直接混合横向比较。

## 当前本机 AppleClang 样本
- 平台：Apple M4 Pro，macOS 26.4.1，arm64
- 核心数：12
- 工具链：AppleClang 21.0.0，Google Benchmark v1.9.5
- 编译：Release（`-O3 -DNDEBUG`）
- 参数：`--benchmark_min_time=0.2s`
- 备注：macOS 上可能无法设置线程亲和性，Google Benchmark 的警告不影响相对对比。

## 通用提示
- 本机 AppleClang 口径下，Makefile 默认构建目录位于 `runs/local/`：测试 `runs/local/build`，基准 `runs/local/build-bench`，覆盖率 `runs/local/build-coverage`。
- Docker/GCC 验证需显式通过 `docker run ... gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench ...` 执行，产物放在 `runs/docker/`。
- 最短运行时间：建议加入 `--benchmark_min_time=0.2s`（单位必须为秒）。
- 过滤子集：可用 `--benchmark_filter`（示例：`--benchmark_filter=^Div/SmallDivisor64/`）。
- 完整矩阵：使用 `bench-full` 或 `bench-compare-full`，等价于给可执行传入 `--gint_full`。

## 基准测试（Benchmark）

### 目的
- 仅运行 gint 自身的用例，评估绝对性能。

### 用法
- 本机 AppleClang 标准矩阵：`make bench BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机 AppleClang 完整矩阵：`make bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- Docker/GCC 完整矩阵：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机直接运行：`runs/local/build-bench/perf_benchmark_int256 --benchmark_min_time=0.2s`

### 常用参数
- `--benchmark_min_time=0.2s`：设置最短运行时间以降低抖动。
- `--benchmark_filter=<regex>`：只运行匹配的用例。

## 对比测试（Comparison）

### 目的
- 在代表性细分场景下，对比 256 位算术在不同实现（gint/ClickHouse/Boost）间的性能差异。

### 用法
- 本机 AppleClang 标准矩阵：`make bench-compare BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机 AppleClang 完整矩阵：`make bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- Docker/GCC 完整矩阵：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
- 本机直接运行：`runs/local/build-bench/perf_compare_int256 --gint_full --benchmark_min_time=0.2s`

### 方法学
- 每个用例 256 对确定性样本（固定种子），循环中轮询；
- 三方库共享相同输入；
- 对比测试只衡量吞吐，不校验三方结果等价；gint 正确性由单元测试覆盖；
- 预生成数据集，循环内仅取引用与计算，不做拷贝/构造；
- 对循环不变操作数与表达式使用 `benchmark::DoNotOptimize`，防止常量折叠或被外提；
- 倾向覆盖常见“快/慢”路径与真实工作负载热点。
- 当前结果是热数据、独立操作的 microbenchmark 吞吐；数据集很小，通常驻留在 L1/L2 cache 中，不代表冷数据访问、依赖链延迟或端到端业务性能。跨工具链或跨机器比较应重新采集并单独报告。

### 结果（完整矩阵，real_time ns/op）

数值越低越好。以下数据来自 `bench-compare-full` 的本机 AppleClang 样本。

#### 加法（Addition）

**设计**
- NoCarry：低位进位概率低，覆盖“无跨 limb 进位”的最快路径。
- FullCarry：-1 + 1，覆盖“全链条进位”最坏路径。
- CarryChain64：仅低 64 位形成进位链，隔离低层 carry 传播。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| NoCarry      | 0.666 |       1.94 |  4.72 |
| FullCarry    | 0.661 |       1.84 |  1.77 |
| CarryChain64 | 0.659 |       1.74 |  4.60 |

#### 减法（Subtraction）

**设计**
- NoBorrow：避免跨 limb 借位，覆盖最快路径。
- FullBorrow：0 - 1，覆盖“全链条借位”最坏路径。
- BorrowChain64：仅低 64 位形成借位链，隔离低层 borrow 传播。

| 用例          | gint  | ClickHouse | Boost |
| ------------- | ----: | ---------: | ----: |
| NoBorrow      | 0.664 |       1.59 |  4.81 |
| FullBorrow    | 0.658 |       1.76 |  2.04 |
| BorrowChain64 | 0.659 |       1.68 |  4.54 |

#### 乘法（Multiplication）

**设计**
- U64xU64：小 x 小，验证单 limb 快路径。
- HighxHigh：高位有值，多 limb 学校乘法路径，评估对角线累加与进位管理。
- WideTimesU64：完整 256 位 x 64 位的混合乘法，对三方实现进行公平对比（被乘数由四个 64 位随机 limb 组成，避免单肢退化）。
- U32xWide：32 位 x 宽整数，覆盖“标量 x 宽整数”的常见热点。

| 用例         | gint  | ClickHouse | Boost |
| ------------ | ----: | ---------: | ----: |
| U64xU64      |  1.77 |       2.78 |  2.23 |
| HighxHigh    |  1.75 |       2.75 |  10.4 |
| WideTimesU64 | 0.889 |       3.22 |  2.51 |
| U32xWide     |  1.78 |       3.64 |  4.23 |

#### 除法（Division）

**设计**
- SmallDivisor32：被除数为随机 256 位，除数取 32 位范围，覆盖“基数 2^32 的小除数场景”。
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖“128/64 估商的小除数场景”。
- Pow2Divisor：除数为 2 的幂，覆盖“移位等价”的极端快路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖“Knuth D 多 limb 重路径”，考察规范化与估商修正。
- LargeDivisor128：两 limb 除数，覆盖“多 limb 路径在 2-limb 情况的热点”。
- SimilarMagnitude2：与上述相似量级不同分布，用于验证分布差异对分支与规范化的影响。

| 用例                       | gint | ClickHouse | Boost |
| -------------------------- | ---: | ---------: | ----: |
| SmallDivisor32（32 位）    | 11.1 |       13.7 |  19.7 |
| SmallDivisor64（64 位）    | 13.7 |       13.7 |  22.8 |
| Pow2Divisor（2 的幂）      | 6.68 |        310 |  66.6 |
| SimilarMagnitude           | 15.8 |        227 |  65.4 |
| LargeDivisor128（两 limb） | 17.0 |        524 |  36.1 |
| SimilarMagnitude2          | 14.7 |        205 |  77.6 |

#### 取模（Modulo）

**设计**
- SmallDivisor64：被除数为随机 256 位，除数取 64 位范围，覆盖小除数取模路径。
- SimilarMagnitude：被除数与除数数量级相近，覆盖多 limb 取模路径。

| 用例             | gint | ClickHouse | Boost |
| ---------------- | ---: | ---------: | ----: |
| SmallDivisor64   | 20.5 |       15.3 |  19.0 |
| SimilarMagnitude | 18.4 |        227 |  65.2 |

#### 位运算（Bitwise）

**设计**
- And：随机 256 位值按位与。
- Xor：随机 256 位值按位异或。

| 用例 | gint  | ClickHouse | Boost |
| ---- | ----: | ---------: | ----: |
| And  | 0.756 |      0.750 |  7.84 |
| Xor  | 0.750 |      0.354 |  6.96 |

#### 移位（Shift）

**设计**
- LeftVariable：随机 256 位值按变量位移量左移。
- RightVariable：随机 256 位值按变量位移量右移。

| 用例          | gint | ClickHouse | Boost |
| ------------- | ---: | ---------: | ----: |
| LeftVariable  | 4.31 |       3.20 |  5.65 |
| RightVariable | 2.78 |       2.99 |  3.73 |

#### 字符串转换（ToString）

**设计**
- Base10：高位为 1 的逐步递增数，评估十进制字符串转换效率。

| 用例   | gint | ClickHouse | Boost |
| ------ | ---: | ---------: | ----: |
| Base10 |  125 |        284 |   142 |
