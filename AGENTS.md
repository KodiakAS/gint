# AGENTS.md

gint 是严格固定位宽的 C++11 header-only 宽整数库。完成变更意味着同时守住行为
语义、单头文件集成、支持矩阵和相关性能门禁，不能只证明“能够编译”。

## 产品契约

- `gint::integer<Bits, Signed>` 的模板位宽就是实际位宽；保持固定宽度回绕、补码
  signed/unsigned 语义及文档规定的原生整数互操作。
- 公共 API 最低为 C++11；不得把 C++14/17/20 语法引入公共头文件或必要构建路径。
- 支持范围只以 [`docs/SUPPORT.md`](docs/SUPPORT.md) 为准；实现依赖 `__int128`
  和 GCC/Clang builtin，不扩展到 MSVC、32-bit 或 big-endian。
- `include/gint/gint.h` 是可单独复制的完整入口，`include/gint/core.h` 是可选算术
  入口。CMake 只导出 `gint::gint`、`gint::checked` 两个 `INTERFACE_LIBRARY`，
  安装不得产生二进制库。
- 当前开发版本线为 `0.9.x`。修改版本时同步 `CMakeLists.txt` 的 `project()` 与
  `src/gint/prelude.hpp` 的 `GINT_VERSION_*`，再生成并核对
  `include/gint/gint.h`。

## 仓库地图与权威来源

- `include/gint/`：公开生成头；`src/gint/`：可由语言服务直接解析的内部 `.hpp`
  源图；`tests/`：unit、consumer/package、differential、fuzz 和性能工具测试；
  `bench/`：gint benchmark 与三方 comparison。
- `cmake/`、`scripts/`、`.github/`：集成、验证脚本与 CI；`docs/`：长期契约；
  `third_party/` 默认只读，除非任务明确要求更新依赖。
- 文档职责由 [`docs/README.md`](docs/README.md) 定义。公共语义看
  [`docs/TECH_SPEC.md`](docs/TECH_SPEC.md)，集成看
  [`docs/INTEGRATION.md`](docs/INTEGRATION.md)，内部算法看
  [`docs/INTERNALS.md`](docs/INTERNALS.md)，支持与版本策略看
  [`docs/SUPPORT.md`](docs/SUPPORT.md)。
- 性能方法与命令以 [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) 为准；环境固化以
  [`docs/VALIDATION_ENVIRONMENTS.md`](docs/VALIDATION_ENVIRONMENTS.md) 为准；发布和
  精确安装清单以 [`docs/RELEASING.md`](docs/RELEASING.md) 为准。
- 用户可见变化同步 [`CHANGELOG.md`](CHANGELOG.md)，必要时同步
  [`docs/UPGRADING.md`](docs/UPGRADING.md)；公共贡献和 PR 约定见
  [`CONTRIBUTING.md`](CONTRIBUTING.md)。实现、测试和权威文档不一致就是缺陷。

## 授权与修改边界

- 回答、解释、审阅、诊断或规划任务只检查材料并报告结果；除非用户同时要求修改，
  不实现修复、不改变仓库或远端状态。
- 修改、修复或构建任务可直接完成范围内的本地编辑，并运行相关非破坏性验证。
- 外部写入、提交、推送、创建或合并 PR、发布、破坏性操作、购买，以及实质扩展
  任务范围，都需要用户明确授权；已有明确要求即视为该项授权。
- 保留所有无关工作区修改和现有 worktree。构建、依赖、缓存、日志、benchmark
  结果及临时文件统一放入 `runs/<scope>/`；不同 OS、架构或编译器不得复用 CMake
  cache。不要清理与当前任务无关的产物。

## 实施与验证

先确认预期契约和受影响入口，再选择最小充分门禁：

- 所有变更至少运行 `git diff --check`。仅文档变更检查链接、路径和命令即可，
  不要求完整测试。
- 修改 `src/gint/*.hpp` 后运行 `make amalgamate`，并以
  `make internal-headers-check amalgamate-check` 验证内部头和已提交生成头同步。
- 内部头必须保持可独立解析；生成器只接受受限、fail-closed 的预处理方言：本地
  quoted include 位于顶层无条件上下文，路径必须是规范的非空相对路径，且不得
  逃出源树或穿越缺失/符号链接组件；禁止条件/宏/内部 angle include、文件搜索
  operator、module/import 控制行、块注释、raw string、pragma operator、trigraph、
  digraph 和非规范续行。普通模块必须以唯一的 `#pragma once` 开始；只有 manifest
  分类的 definition-pass lifecycle fragment 可使用规定 marker 并重复展开。完整
  输入、角色和依赖方向契约见 [`docs/INTERNALS.md`](docs/INTERNALS.md)。
- C++ 先运行能覆盖变更的精确测试，再运行 `make test`。correctness bug 先保存
  最小复现，并增加能在旧实现失败的回归测试。
- 公共头文件、CMake 或安装变更必须覆盖 C++11 `-Wall -Wextra -Werror`、
  `gint.h` 独立包含、`core.h` 独立及 `core.h -> gint.h` 两阶段包含、
  consumer/package contract、CMake 3.13 和精确安装清单。header、consumer、
  package 与安装 contract 由 `CMakeLists.txt` 和 `tests/cmake/` 维护并随
  `make test` 执行；最低 CMake 版本使用对应的 3.13 lane 验证。
- 除法、移位、解析、浮点或 signed 边界除主测试外，运行 sanitizer 和独立
  differential；适用时运行 `scripts/run-fuzz.sh`。确定性 differential 的标准入口是
  `CXX=c++ GINT_DIFFERENTIAL_BUILD_DIR=runs/local/differential scripts/run-differential.sh`。
- 性能工具变更运行
  `python3 -m unittest discover -s tests/perf -p 'test_*.py'` 和
  `bash scripts/check-codegen-contract.sh c++ runs/local/codegen`。
- 修改 C++ 时按仓库 `.clang-format` 格式化受影响文件；修改 shell 时运行
  `bash -n`，可用时运行 `shellcheck`；修改 workflow 时可用 `actionlint` 检查。
- 跨平台或发布候选按 `docs/SUPPORT.md` 和 `docs/RELEASING.md` 扩大矩阵，并逐环境
  报告；不得把一个平台、架构或编译器的结果外推到其他支持组合。

## 性能变更

hot path 修改前先建立目标用例 baseline。先用 `--benchmark_list_tests` 发现当前
用例，先通过受影响架构的 codegen contract，再在同一机器、工具链、选项、源码、
过滤器和参数下串行采集 baseline/result；优先比较多次重复的 median 并检查 CV。
共享 runner 和 GCC 4.8.5 lane 不用于 wall-clock 性能结论。没有同环境前后数据，
不得声称性能提升或无回归。

## 完成条件

- 请求的行为已实现，产品契约、测试和权威文档一致；没有夹带范围外修改。
- 所选门禁全部通过；无法运行的检查要说明原因、影响和下一项最有效的验证。
- 最终报告列出修改文件、关键 correctness/compatibility/performance 证据、未验证项
  和当前 worktree 状态。仅在用户明确要求交付时提交或操作远端；PR 使用
  `.github/PULL_REQUEST_TEMPLATE/` 中匹配模板，标题前缀遵循 `CONTRIBUTING.md`。
