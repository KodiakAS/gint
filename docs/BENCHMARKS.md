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
- 优先比较多次重复的 median，同时检查 CV 和异常值。

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

## 结论采样

推荐参数：

```sh
BENCH_ARGS='--benchmark_min_time=0.2s \
--benchmark_repetitions=7 \
--benchmark_enable_random_interleaving=true' \
make bench-full
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

README 可以保留一条标明 commit、主机、工具链和统计口径的定位性摘要；长期文档
不维护某个历史 commit 的完整数字表。性能报告或发布证据至少记录：

- commit、日期、机器、OS、架构和编译器完整版本；
- Google Benchmark 版本、构建选项、位宽、过滤器和全部参数；
- 每个用例的 median、重复次数和 CV；
- baseline/result 是否来自同一环境，以及 comparison 的值域定义。

不同工具链分别呈现。没有同环境前后证据时，只能描述代码形态或历史样本，不能
声称性能提升或退化。
