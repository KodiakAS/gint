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
数据、依赖链延迟或端到端业务性能。单机、仿真或虚拟化结果应注明 CPU、虚拟化
方式和适用范围，不得直接外推到其他微架构；跨架构选型与特化准入原则见
[实现说明](INTERNALS.md#性能维护)。

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

`--gint_full` 在 gint-only 中还提供原生除数、标量复合乘法、有符号与分 limb 除模，
以及文本和浮点转换用例。复合乘法分别覆盖单 limb 和稠密输入，浮点转换分别覆盖
低 limb 精确值和满宽需舍入值。配对的 `DivMod/*` 用例共享确定性数据，分别使用
`gint::divmod` 和 `/` 加 `%`，均消费商和余数。
512 位和 1024 位还覆盖归一化的 5 limb 和 9 limb 长除数。

`Latency/*Chain` 将每次运算的结果传入下一次运算，左移链按批重置以保持非零输入。
`Shift/*Const1` 和 `Shift/*Limb64` 衡量独立输入的固定位移。`CompareFloat/*`
覆盖不同指数、同指数不同有效位、完全相等，以及仅低位不同。`Fmt/FormatToDec`
在启用 fmt 时复用输出缓冲区，`Hash/StdHash` 测量标准库哈希入口。

两个 benchmark 二进制都会拒绝无法识别的参数。过滤器无匹配或正则无效时，
采样、`--benchmark_list_tests` 和 `--benchmark_dry_run` 均以非零状态退出。
`--help` 仍可用于查看当前 Google Benchmark 版本支持的选项。

## 结论采样

推荐参数：

```sh
make bench-full \
  BENCH_ARGS='--benchmark_min_time=0.2s --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true'
```

需要保存证据时再增加：

```text
--benchmark_display_aggregates_only=true
--benchmark_out_format=json
--benchmark_out=<path-under-runs>
```

该参数只折叠终端输出，JSON 仍保留逐次样本。需要复核异常值或完成次数时使用逐次记录。

怀疑频率、代码布局或虚拟化噪声时，应交错重复 baseline/result，并结合
cycles、instructions 和汇编解释，不能只报告一次百分比。

### 单项回退与布局诊断

用 [比较脚本](../scripts/compare_benchmarks.py) 读取两份原始 JSON 的逐次样本。
每次对比固定一个 suite、位宽和环境。两端 sidecar 记录 `commit`、`suite`、
`bits`、`host`、`cpu`、`virtualization`、`os`、`arch`、`compiler`、`flags`、
`harness`、`filter` 和 `sampling`。除 `commit` 外各字段须一致。`bits` 为整数，
`flags` 和 `sampling` 可记录为字符串、数组或对象。

```sh
python3 scripts/compare_benchmarks.py \
  --baseline runs/perf/A.json --result runs/perf/B.json \
  --baseline-meta runs/perf/A.meta.json --result-meta runs/perf/B.meta.json \
  --markdown runs/perf/diff.md --json runs/perf/diff.json --fail-on-regression
```

默认使用墙钟时间判定，同时报告 CPU 时间，可通过 `--metric cpu_time` 切换。
阈值及默认值见脚本 `--help`，覆盖参数随报告保存。短采使用报告模式，允许省略
sidecar 并标记环境未核实。固定主机可显式启用门禁，已判定的单项回退退出 1，
输入、覆盖或环境校验失败以及仍需复测的候选退出 2，其余退出 0。
登记全部超线项，即使几何均值改善。分别比较固定主机 A/B/B/A 中两对相邻样本，
确认两对都超线且 CPU 时间同向。秩检验提示分布变化，全矩阵筛查可能产生误报，
同一进程内的重复也可能受布局和时间相关性影响。复测未确认的项保留测得范围和不确定性。

检查最终链接二进制中的热点、内联调用者和循环，记录入口和分支目标地址模 32、
instructions/op 和 cycles/op。指令序列或 helper 变化时做单改动 A/B。序列一致而
地址变化时，在两端施加相同的链接顺序或填充扰动，或分别试验工具链支持的
`-falign-functions=32` 与 `-falign-loops=32`。函数对齐改变入口和代码体积，
循环对齐还会引入填充和取指差异。保持默认 benchmark 和 INTERFACE flags，仅在
双边诊断中使用这些选项并记录工具链支持和最终地址。耗时随布局漂移且动态工作量
近似稳定时记录为布局敏感，并保留默认构建的实际代价。缺少计数器或复测矛盾时，
记录为未归因。参见 [GCC 对齐选项](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)。

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
GCC/Clang/AppleClang codegen contract；定时和手工任务保存 gint-only、256/512-bit 三方
comparison、128-bit 除模、512-bit 热点、1024-bit parser 与定向热点的原始及规范化 JSON。

共享 runner 的绝对时间不阻断 PR。自动化样本用于发现趋势；真正的性能回归结论
必须回到固定主机复测。GCC 4.8.5 lane 只验证 correctness/integration，不构建
Google Benchmark，也不用于性能宣传。

### 三级覆盖和锚点

1. PR 运行确定性的 codegen contract。
2. 固定主机先窄测目标，再用 `--gint_full` 对 128/256/512/1024 位各做一次短采，
   以 `--benchmark_list_tests` 核对覆盖，超线项转入定向 ABBA。
3. 定时任务持续观察趋势。发布前或一组 hot path 重构后，用固定 harness 重编最近
   发布 tag 或上次确认的里程碑和当前头，比较相邻版本以及锚点到当前，记录共同用例、
   缺项和环境。锚点跨 PR 保持固定，以发现累积退化。长期证据需保存可重建的 commit、
   输入、参数与原始结果。

## 结果记录

性能数字随报告或发布证据保存，至少记录：

- commit、日期、机器、OS、架构和编译器完整版本；
- Google Benchmark 版本、构建选项、位宽、过滤器和全部参数；
- 每个用例的 median、重复次数和 CV；
- baseline/result 是否来自同一环境，以及 comparison 的值域定义。

不同工具链分别呈现。没有同环境前后证据时，只能描述代码形态或历史样本，不能
声称性能提升或退化。

### 取舍字段

对每项退化记录受损的主机、位宽与编译器、绝对与相对成本、收益场景、ABBA 和
汇编证据，以及接受现有实现或重新设计的理由。显著退化需由维护者明确接受，
未归因项保留待查。
