# AGENTS.md

本项目是一个高性能、严格固定位宽的 C++ 宽整数库。代理执行任务时，必须同时维护 correctness、header-only 集成、C++11 兼容和可测量性能，不能只满足“能够编译”。

## 产品与支持契约

- 当前发布线为 `0.9.x`，版本来源必须同时更新 `CMakeLists.txt` 与 `include/gint/gint.h` 的 `GINT_VERSION_*` 宏。
- 公共 API 最低标准是 C++11；不得把 C++14/17/20 能力引入公共头文件或必要构建路径。
- 正式支持 GCC、LLVM Clang 的 GNU-compatible driver 和 AppleClang；MSVC、`clang-cl` 及其他 frontend 不支持。
- 正式平台是 64-bit little-endian Linux x86_64/AArch64 与 macOS arm64；实现依赖 `__int128` 和 GCC/Clang builtin。
- 库必须保持 header-only：
  - 只复制 `include/gint/gint.h` 仍是完整且受测试的一级集成方式；
  - `include/gint/core.h` 是可选的精简算术入口；
  - CMake 只导出 `gint::gint` / `gint::checked` `INTERFACE_LIBRARY`；
  - 安装产物只有两个头文件、CMake metadata 和 `LICENSE`，不得生成或安装静态库、动态库。
- 模板位宽就是实际位宽；修改必须保持固定宽度回绕、signed/unsigned 语义和原生整数互操作契约。

## 权威文档

不要在本文件重复维护大段容易漂移的细节。修改相应契约时同步更新以下来源：

- 支持范围与版本策略：`docs/SUPPORT.md`
- benchmark、comparison、codegen 与采样口径：`docs/BENCHMARKS.md`
- 环境与依赖固化：`docs/VALIDATION_ENVIRONMENTS.md`
- 发布检查与安装清单：`docs/RELEASING.md`
- 用户可见变化与升级：`CHANGELOG.md`、`docs/UPGRADING.md`
- 实现语义：`docs/TECH_SPEC.md`

文档与实现不一致视为缺陷；先确认预期契约，再在同一变更中修正实现、测试与文档，不能只改其中一边。

## 项目结构与边界

- `include/`：公共头文件与实现
- `tests/`：GoogleTest、consumer、CMake contract、differential、fuzz、perf tooling tests
- `bench/`：Google Benchmark 自身基准和 ClickHouse/Boost comparison
- `cmake/`：安装导出与编译器契约
- `scripts/`：可复用验证、差分、fuzz、codegen、benchmark 归一化工具
- `docs/`：产品、技术、性能和发布文档
- `third_party/`：只读测试/对比依赖，除非任务明确要求更新依赖
- `runs/`：所有构建、缓存、依赖源码、日志和结果；默认不提交

禁止在 `runs/` 外创建构建目录或临时产物。保留用户已有 worktree/修改，不清理与当前任务无关的文件。

## 高效执行原则

1. 先按变更范围选择最小充分验证，不要一开始就跑所有平台。
2. 发现 bug 时先保存最小复现，再修复，并添加能在旧实现上失败的回归测试。
3. 修改 hot path 前先建立目标用例 baseline；没有同环境前后数据，不得声称性能提升。
4. 本机与容器、不同编译器、不同架构使用独立 `runs/<scope>/`，禁止复用含其他编译器的 CMake cache。
5. 可并行的独立构建/检查可以并行；同一 benchmark 主机上的性能采样必须串行，避免相互污染。
6. 先完成本地静态检查和目标测试，再集中提交/push，减少无意义的重复 CI。
7. 报告结论和证据，不在 PR 正文粘贴重复流程，也不要写本地 `runs/` 路径。

## 分层验证

以下是按风险递增的验证阶梯；只运行当前变更需要的层级，发布准备、公共头文件大改、跨平台代码或用户明确要求全平台时再跑完整矩阵。

### 所有变更

- 对 C++ 文件运行 `clang-format --style=file`；最终至少通过 CI 使用的 clang-format 版本。
- 运行 `git diff --check`。
- 新增脚本保持可执行位和 Bash 兼容；shell 脚本运行 `bash -n`，有条件时运行 `shellcheck`。

### 普通 C++ / correctness

- 先运行精确的 GoogleTest/CTest 过滤器，再运行本机 C++11 主矩阵：`make test`。
- 浮点、除法、移位、解析、signed 边界等语义修改，应补 sanitizer 和独立 differential：
  - `CXX=<compiler> GINT_DIFFERENTIAL_BUILD_DIR=runs/<scope>/differential scripts/run-differential.sh`
  - Clang/libFuzzer 使用 `GINT_FUZZ_BUILD_DIR=runs/<scope>/fuzz scripts/run-fuzz.sh <seconds>`。
- differential oracle 必须独立于被测实现；禁止用 gint 的同一算法同时生成 expected value。

### 公共头文件 / CMake / 安装导出

- 验证 C++11 `-Wall -Wextra -Werror` 的单头文件隔离复制编译。
- 验证 `core.h` 单独包含以及 `core.h` 后升级包含 `gint.h`。
- 运行 consumer 的 package version、完整 manifest、INTERFACE target、transitive definition 和 compiler rejection contract。
- 触及最低 CMake 支持路径时验证 CMake 3.13；不要使用该版本不存在的 `cmake --install` 语法。
- 安装前缀必须与 `docs/RELEASING.md` 清单精确一致，且不存在 `.a/.so/.dylib/.dll/.lib`。

### Workflow / 工具脚本

- GitHub Actions 运行 `actionlint`。
- Python 工具运行：`python3 -m unittest discover -s tests/perf -p 'test_*.py'`。
- codegen contract 运行：`bash scripts/check-codegen-contract.sh <cxx> runs/<scope>/codegen`。
- PR 上的确定性 codegen contract 是阻断门禁；共享 runner 的 wall-clock benchmark 只做定时/手工采样，不以绝对时间阻断 PR。

### 跨平台 / 发布候选

需要完整验证时分别覆盖并报告：

- macOS arm64 + AppleClang：C++11 Debug + Release/O3、sanitizer；
- Linux AArch64 + GCC/Clang：C++11 Debug + Release/O3、differential、codegen；
- Linux x86_64 + GCC/Clang：C++11 Debug + Release/O3、sanitizer、differential/fuzz、codegen；
- AlmaLinux 8 开发镜像中的 GCC 8.5 完整 C++11 矩阵；
- CMake 3.13 package/header contract。

C++17 只作为补充验证，不能替代任何 C++11 门禁。

不同 OS/架构/编译器的结果分开呈现，不能混成一个横向性能结论。外部主机的创建、同步和生命周期由执行环境编排；仓库脚本只假定已经进入目标环境。

## 性能工作流

### 概念

- Benchmark：只运行 gint，目标为 `make bench` / `make bench-full`。
- Comparison：同场景对比 gint、ClickHouse、Boost，目标为 `make bench-compare` / `make bench-compare-full`。
- comparison 的 bit-pattern 场景统一使用 unsigned 固定位宽类型；signed comparison 必须从统一数学值构造并在计时前校验相等，禁止直接混用补码与 signed-magnitude raw words。
- 普通性能任务未指定环境时默认只使用本机 AppleClang；用户指定环境集合时严格按其要求执行。架构/编译器敏感 hot path、发布候选或全平台任务必须补 Linux x86_64 固定主机及相应支持矩阵，真实 wall-clock 结论不得来自共享 runner。

### 目标选择

- 先用 `--benchmark_list_tests` 自动发现用例，不依赖过时清单。
- 运算级目标使用前缀，例如 `--benchmark_filter='^Add/'`。
- 精确场景使用稳定正则，例如 `--benchmark_filter='^(Add/NoCarry|Add/FullCarry)(/|$)'`。
- parser hot path 必须同时覆盖 UInt256/UInt1024、String/CStr、满位宽 Base2/8/10/16 与 ShortBase2/8/10/16；稳定过滤器为 `^FromString(CStr)?/(Short)?Base(2|8|10|16)/gint$`。

### 基线与结果

- baseline/result 必须使用同一机器、同一编译器与选项、同一源码口径、同一过滤器和 `BENCH_ARGS`。
- 建议结论采样使用 `--benchmark_min_time=0.2s --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true`，保存 JSON 并优先比较 median；同时检查 CV。
- 固定版本性能环境通过 `scripts/bootstrap-validation-env.sh` 准备 Google Benchmark v1.9.5；不要用发行版 `libbenchmark-dev` 产出性能结论。
- 同一物理/虚拟主机的 baseline 和 result 串行运行；怀疑 code layout 或频率噪声时交错重复，并结合汇编、cycles/instructions 解释。
- 性能结果只在同工具链内比较；AppleClang、Docker/GCC、真实 x86 分别报告。
- hot-path 修改必须先运行受影响架构的 codegen contract，再做同机、同参数的前后 benchmark。

常用构建目录：

- 本机：`BENCH_BUILD_DIR=runs/local/build-bench`
- Docker/GCC：`BENCH_BUILD_DIR=runs/docker/build-bench`
- 其他环境：`BENCH_BUILD_DIR=runs/<scope>/build-bench`

开发镜像沿用兼容 tag `gint:centos8`，实际基础系统为 AlmaLinux 8。镜像存在时复用，不存在时才运行 `make image`。

## 代码与文档风格

- 默认使用简体中文沟通、PR 正文和报告；命令、flag、文件名与 API 保留英文。
- 代码优先清晰、可内联、无隐藏分配；不要为微小 benchmark 改善引入未验证的跨平台复杂度。
- 公共头文件变更要检查 ODR、配置 inline namespace、`core.h -> gint.h` 双阶段包含和 `-fno-exceptions`。
- 只在能防止真实回归时增加测试/门禁；避免重复测试同一机制或把共享 runner 噪声变成阻断条件。
- 用户可见行为、支持边界、安装产物或性能方法变化必须同步相应权威文档和 `CHANGELOG.md`。

## PR 与交付

1. 分支名使用英文。
2. 标题严格匹配所选模板：
   - `feature.md`：`feat: <简要描述>`
   - `bugfix.md`：`fix: <简要描述>`
   - `perf.md`：`perf: <简要描述>`
   - `refactor.md`：`refactor: <简要描述>`
   - `docs.md`：`docs: <简要描述>`
   - `breaking-change.md`：`break: <简要描述>`
3. 必须使用 `.github/PULL_REQUEST_TEMPLATE/` 中最合适的模板；删除占位符、空段和不适用内容，不添加 `[codex]`、`AI` 等前后缀。
4. 通过 API/CLI/connector 创建后，运行 `gh pr view --json title,body` 核对标题和正文。
5. 默认先创建 draft PR；全部 CI 通过后再标记 Ready for review，并等待自动/人工 review。对有效意见完成修复、回归、push 和线程收尾。
6. PR 正文只保留与本次变更直接相关的结果、API/行为和风险；不包含本地 artifact 路径。

交付前确认 worktree clean、提交已推送、PR head SHA 与本地一致，并报告最重要的正确性、兼容性和性能证据。
