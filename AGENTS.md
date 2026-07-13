# AGENTS.md

gint 是严格固定位宽的 C++ header-only 宽整数库。代理必须同时维护 correctness、
C++11 集成、支持矩阵和可测量性能，不能只以“能够编译”作为完成标准。

## 不可破坏的产品契约

- `integer<Bits, Signed>` 的模板位宽就是实际位宽；保持固定宽度回绕、补码
  signed/unsigned 语义和原生整数互操作。
- 公共 API 最低 C++11。不得把 C++14/17/20 语法引入公共头文件或必要构建路径。
- 支持平台和编译器只以 [`docs/SUPPORT.md`](docs/SUPPORT.md) 为准；实现依赖
  `__int128` 与 GCC/Clang builtin。
- 库保持 header-only：单独复制 `include/gint/gint.h` 是一级集成，`core.h` 是
  可选入口，CMake target 只能是 `gint::gint` / `gint::checked` 的
  `INTERFACE_LIBRARY`，安装不得出现二进制库。
- 当前发布线为 `0.9.x`。修改版本时同步 `CMakeLists.txt` 和 `gint.h` 中的
  `GINT_VERSION_*`。

## 权威文档

不要在多个文件复制易漂移的矩阵、阈值或命令：

| 主题 | 权威来源 |
| --- | --- |
| 文档职责与导航 | [`docs/README.md`](docs/README.md) |
| 用户集成与配置 | [`docs/INTEGRATION.md`](docs/INTEGRATION.md) |
| 公共行为语义 | [`docs/TECH_SPEC.md`](docs/TECH_SPEC.md) |
| 内部表示与算法 | [`docs/INTERNALS.md`](docs/INTERNALS.md) |
| 平台、编译器与版本策略 | [`docs/SUPPORT.md`](docs/SUPPORT.md) |
| benchmark、comparison 与 codegen | [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) |
| 环境和依赖固化 | [`docs/VALIDATION_ENVIRONMENTS.md`](docs/VALIDATION_ENVIRONMENTS.md) |
| 发布与安装清单 | [`docs/RELEASING.md`](docs/RELEASING.md) |
| 用户变化与迁移 | [`CHANGELOG.md`](CHANGELOG.md)、[`docs/UPGRADING.md`](docs/UPGRADING.md) |
| 公共贡献流程 | [`CONTRIBUTING.md`](CONTRIBUTING.md) |

实现、测试和文档不一致视为缺陷；先确认预期契约，再在同一变更中同步修正。

## 仓库边界

- `include/`：公共头文件与实现；`tests/`：correctness、consumer、differential、
  fuzz 和 perf tooling；`bench/`：benchmark 与 comparison。
- `cmake/`、`scripts/`、`.github/`：集成与自动化；`docs/`：长期文档。
- `third_party/` 默认只读，除非任务明确要求更新依赖。
- 所有构建、缓存、依赖、日志和临时结果放在 `runs/`；不要清理用户已有 worktree
  或与当前任务无关的修改。

## 执行原则

1. 按变更范围选择最小充分验证；发布准备和跨平台变更才扩大到完整矩阵。
2. bug 先保存最小复现，再修复并增加能在旧实现失败的回归测试。
3. hot path 修改前先建立目标用例 baseline；没有同环境前后数据不得声称提升。
4. 不同 OS、架构和编译器使用独立 `runs/<scope>/`，不得复用 CMake cache。
5. 独立 correctness 检查可以并行；同一 benchmark 主机上的采样必须串行。
6. 先完成本地静态检查和目标测试，再集中提交或 push。

## 验证阶梯

所有变更至少运行 `git diff --check`。C++ 文件使用仓库 `.clang-format`；shell
脚本运行 `bash -n`，可用时运行 `shellcheck`；workflow 运行 `actionlint`。

- 普通 C++：先跑精确测试，再运行 `make test`。
- 除法、移位、解析、浮点或 signed 边界：补 sanitizer、独立 differential，
  适用时运行 libFuzzer。
- 公共头文件、CMake 或安装：验证 C++11 `-Wall -Wextra -Werror` 单头文件、
  `core.h` 两阶段包含、consumer/package contract、CMake 3.13 和精确安装清单。
- 性能工具：运行 `python3 -m unittest discover -s tests/perf -p 'test_*.py'` 和
  `scripts/check-codegen-contract.sh`。
- 跨平台或发布候选：按支持矩阵和发布清单分别报告结果，不把一个平台的证据
  外推到另一个平台。

具体命令、采样参数与环境创建只引用上表权威文档，不在本文件重复。

## 性能工作

未指定环境时使用本机 AppleClang；用户指定环境集合时严格按要求执行。架构或
编译器敏感 hot path、发布候选和全平台任务必须补对应支持矩阵。真实 wall-clock
结论不得来自共享 runner，GCC 4.8.5 lane 也不用于性能结论。

先用 `--benchmark_list_tests` 发现当前用例。baseline/result 必须保持机器、
工具链、选项、源码、过滤器和参数一致；优先比较 median 并检查 CV。先完成受影响
架构的 codegen contract，再进行串行前后采样。

## PR 与交付

- 分支名使用英文；标题严格使用匹配模板的 `feat:`、`fix:`、`perf:`、
  `refactor:`、`docs:` 或 `break:` 前缀。
- 使用 `.github/PULL_REQUEST_TEMPLATE/` 中最合适的模板，删除空段和占位符；正文
  只保留行为、风险和验证结果，不写本地 `runs/` 路径。
- 默认先创建 draft PR；全部 CI 和 review 完成后再 Ready，并处理有效意见。
- 交付前确认 worktree 状态、提交、远端 head 和验证对象一致，再报告最重要的
  correctness、compatibility 与 performance 证据。
