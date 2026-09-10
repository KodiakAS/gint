# 基准测试

本文定义性能验证的方法、命令和结果解释。环境准备见
[验证环境](VALIDATION_ENVIRONMENTS.md)，实现选择见[实现说明](INTERNALS.md)。

## 三类证据

- **Benchmark**：只运行 gint，用于同环境回归比较。
- **Comparison**：用相同场景比较 gint、ClickHouse wide integers 与
  Boost.Multiprecision。
- **Codegen contract**：检查关键探针的内联、调用、循环和指令预算，作为 PR 上
  的确定性结构门禁。

wall-clock 数字不能替代 correctness 测试，codegen contract 也不能证明实际
吞吐量。

## 测量契约

有效的前后结论必须保持以下条件一致：

- 同一机器、OS、架构和编译器；
- 相同构建选项、源码口径、位宽、过滤器和 benchmark 参数；
- baseline 与 result 串行采集，避免同一主机上的并行负载互相污染；
- 使用[验证环境](VALIDATION_ENVIRONMENTS.md)固化的 Release Google Benchmark，
  不用发行版 Debug 构建；
- 修改 hot path 前建立 baseline；比较多次重复的中位数（median），同时检查
  变异系数（CV）和异常值。

`DivMod/SimilarMagnitude` 中 gint 调用 `gint::divmod`，竞品分别调用 `/` 与 `%`，
两种模式均消费商和余数。启用竞品宏不得改变 gint 的被测 API。

Comparison 的 bit-pattern 场景统一使用 unsigned 固定位宽类型，使三方输入表示
同一非负数学值。各库保留自身对象布局；结果衡量 operator 吞吐，不代表内存
布局已归一化。

这些用例通常使用热、小数据集，主要反映 in-cache 独立操作吞吐，不等同于冷
数据、依赖链延迟或端到端业务性能。

## 发现与运行目标

Makefile 默认使用 `BENCH_BITS=256`，可改为 `128/256/512/1024`：

```sh
make bench
make bench-full
make bench-compare
make bench-compare-full
make BENCH_BITS=512 bench-full
```

不要依赖手写用例清单，先从当前二进制发现：

```sh
runs/local/build-bench/perf_benchmark_int256 --benchmark_list_tests
runs/local/build-bench/perf_compare_int256 --gint_full --benchmark_list_tests
```

过滤器示例：

```text
^Add/
^(Add/NoCarry|Add/FullCarry)(/|$)
^FromString(CStr)?/(Short)?Base(2|8|10|16)/gint$
```

第一个适合运算级目标，第二个固定精确场景。第三个应分别用于 256-bit 和
1024-bit benchmark binary，以覆盖 String/CStr、满位宽/短输入和
Base2/8/10/16 parser 组合。

## 固定除数复用

`perf_prepared_divisor256` 在同一二进制内比较普通 `divmod` 与
`prepared_divisor<Int256/UInt256>`。先加载上文的 compiler env，再构建和发现用例：

```sh
cmake -S . -B runs/local/build-prepared-divisor \
  -DGINT_BUILD_TESTS=OFF -DGINT_BUILD_BENCHMARKS=ON \
  -DCMAKE_CXX_COMPILER="$CMAKE_CXX_COMPILER"
cmake --build runs/local/build-prepared-divisor --target perf_prepared_divisor256 -j4
runs/local/build-prepared-divisor/perf_prepared_divisor256 --benchmark_list_tests
```

名称依次包含 `Ordinary/Prepared`、操作、输入十进制位数 `P`、除数 `10^S` 的
scale，以及正数、负数、混合符号或 unsigned。`DivMod` 和 `Round` 在计时前构造
缓存；`Round` 消费商余并按绝对值四舍五入。`Setup1/16/256` 在每次计时迭代内
重新构造，并复用 1、16、256 次；其时间对应整个批次，除以批次大小才是每项成本。
`items_per_second` 已按实际运算数计算。

例如，比较 65 位混合符号输入除以 `10^20`、`10^30` 和 `10^38`：

```sh
runs/local/build-prepared-divisor/perf_prepared_divisor256 \
  --benchmark_filter='^PreparedDivisor/(Ordinary|Prepared)/(DivMod|Round)/P65/S(20|30|38)/Mixed$' \
  --benchmark_min_time=0.2s --benchmark_repetitions=7 \
  --benchmark_enable_random_interleaving=true
```

采样还应覆盖短被除数、单 limb 与 2 的幂除数，以及计入构造成本的用例。固定
两 limb 除数的复用收益不能外推到每行构造或其他除数形态。做版本前后比较时，
可将同一基准源码分别与两个版本的头编译；旧版本增加
`-DGINT_BENCH_PREPARED_BASELINE`，使 `Prepared` 名称下也调用普通 `divmod`。

## 结论采样

推荐参数：

```sh
make bench-full \
  BENCH_ARGS='--benchmark_min_time=0.2s --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true'
```

需要保存证据时再增加：

```text
--benchmark_report_aggregates_only=true
--benchmark_out_format=json
--benchmark_out=<path-under-runs>
```

怀疑频率、代码布局或虚拟化噪声时，应交错重复 baseline/result，并结合
cycles、instructions 和汇编解释，不能只报告一次百分比。

## Codegen contract

本地运行：

```sh
python3 -m unittest discover -s tests/perf -p 'test_*.py'
bash scripts/check-codegen-contract.sh c++ runs/local/codegen-contract
```

机器可读阈值的唯一来源是
[`tests/perf/codegen_contract.json`](../tests/perf/codegen_contract.json)，探针源码是
[`tests/perf/codegen_contract.cpp`](../tests/perf/codegen_contract.cpp)。本文不复制
指令预算，避免门禁参数与说明漂移。

contract 失败后应先检查生成汇编，并在同编译器上运行受影响用例的前后采样。
只有确认新代码形态合理且没有不可接受退化后，才能调整预算。

## 自动化

[`Performance` workflow](../.github/workflows/performance.yml) 在 PR 上运行现代
GCC/Clang/AppleClang codegen contract；定时和手工任务保存 gint-only、三方
comparison 与宽 parser 的原始及规范化 JSON。

共享 runner 的绝对时间不阻断 PR。自动化样本用于发现趋势；真正的性能回归结论
必须回到固定主机复测。GCC 4.8.5 lane 只验证 correctness/integration，不构建
Google Benchmark，也不用于性能宣传。

## 结果记录

性能数字随报告或发布证据保存，至少记录：

- commit、日期、机器、OS、架构和编译器完整版本；
- Google Benchmark 版本、构建选项、位宽、过滤器和全部参数；
- 每个用例的 median、重复次数和 CV；
- baseline/result 是否来自同一环境，以及 comparison 的值域定义。

不同工具链分别呈现。没有同环境前后证据时，只能描述代码形态或历史样本，不能
声称性能提升或退化。
