# 当前环境验证固化

本文只固化“当前执行环境”里的依赖与目录约定，不规定命令应该在哪里执行。外部编排负责选择运行位置；进入目标环境后，仓库内脚本和 `Makefile` 只负责在当前环境跑测试。

本文也不固定具体测试项目。单元测试、基准测试、对比测试、过滤器、位宽和输出文件都由调用方按任务传入。

## 边界

- 仓库内：准备当前环境依赖、生成当前环境的 compiler env、执行调用方指定的 test/benchmark 目标。
- 仓库外：决定执行位置、生命周期管理、代码同步和结果回收方式。
- 报告时：不同架构、OS、编译器结果应分开呈现，不直接混合成一个横向排名。

## 推荐环境形态

推荐外部编排覆盖这些环境形态，但仓库命令只面向已经进入的当前环境：

| 形态 | 主要用途 |
| --- | --- |
| macOS arm64 + AppleClang | 快速正确性、真实 macOS 集成、本地 arm 编译器覆盖 |
| Linux arm64 + GCC/Clang | 可重复 arm 编译器覆盖 |
| Linux x86_64 + GCC/Clang | 合并前关键性能确认，覆盖 x86 代码生成 |

## 目录约定

所有环境构建、依赖源码、依赖安装、benchmark JSON/CSV 和临时输出都放在 `runs/` 下：

- 调用方通过 `--scope <name>` 或 `*_BUILD_DIR` 决定当前环境输出 scope；
- 常用 scope 示例：`local`、`linux-arm64`、`linux-x86_64`、`ci`；
- scope 只是目录命名，不表达机器位置。

环境脚本会在对应 scope 下生成：

- `runs/<scope>/deps/`：固定版本依赖源码、构建目录和安装目录
- `runs/<scope>/env/`：可 `source` 的编译器环境文件

env 文件名由编译器完整路径安全化生成。例如 `/usr/bin/g++` 对应 `usr_bin_gxx.env`；这样同一环境里多个 basename 相同但路径不同的编译器不会互相覆盖。
env 文件还会导出同名的 `GINT_COMPILER_ID`，用于为该编译器选择独立 build 目录。

## 当前环境初始化

进入当前环境后运行：

```bash
SCOPE=linux-x86_64
scripts/bootstrap-validation-env.sh --scope "${SCOPE}"
```

脚本做三件事：

- 在 Ubuntu/Debian 上安装基础包，其他系统可用 `--skip-packages` 跳过包安装；
- 在 `runs/<scope>/deps/` 下为每个编译器构建 Release 版 Google Benchmark（默认 v1.9.5，可用 `--benchmark-version` 覆盖）；
- 在 `runs/<scope>/env/` 下生成 env 文件。

脚本不会运行任何项目测试或 benchmark。

### Ubuntu/Debian 包

当前环境是 Ubuntu/Debian 时，脚本会安装以下包：

```text
build-essential
ca-certificates
clang
cmake
curl
git
libboost-dev
libfmt-dev
libgtest-dev
ninja-build
pkg-config
rsync
tar
```

不要依赖发行版 `libbenchmark-dev` 做性能结论；本项目用脚本在 `runs/` 下构建固定版本 Release `google-benchmark`，避免发行版包构建类型污染结果。Google Benchmark v1.9.5 本身按 C++17 工具链构建；这只影响 benchmark 工具二进制，不改变库本体和单元测试的 C++11 兼容性目标。

非 Ubuntu/Debian 且依赖已安装的环境可显式跳过包安装，只构建当前编译器对应的 `google-benchmark`：

```bash
SCOPE=current-gcc
scripts/bootstrap-validation-env.sh --scope "${SCOPE}" --skip-packages --compiler g++
```

### 使用生成的编译器环境

```bash
SCOPE=linux-x86_64
source "runs/${SCOPE}/env/usr_bin_gxx.env"
cmake -S . -B "runs/${SCOPE}/build-${GINT_COMPILER_ID}-custom" \
  -DGINT_BUILD_TESTS=ON \
  -DGINT_BUILD_BENCHMARKS=OFF \
  -DCMAKE_CXX_COMPILER="$CMAKE_CXX_COMPILER"
```

Clang 环境通常使用对应路径的安全化文件名，例如 `usr_bin_clangxx.env`。脚本结束时会打印当前环境可直接使用的 `source` 示例。

## 测试项目由任务决定

环境固化不绑定测试目标。调用方可以选择：

- `ctest`；
- `make bench` / `bench-full`；
- `make bench-compare` / `bench-compare-full`；
- 直接运行某个位宽的 benchmark 可执行；
- 任意 `BENCH_ARGS`、`BENCH_BITS`、`--benchmark_filter`、`--benchmark_repetitions` 和 `--benchmark_out`。

示例只表达调用形状，不代表固定矩阵：

```bash
SCOPE=linux-x86_64
source "runs/${SCOPE}/env/usr_bin_gxx.env"
make BENCH_BUILD_DIR="runs/${SCOPE}/build-bench-${GINT_COMPILER_ID}" \
  BENCH_BITS=256 \
  bench-full \
  BENCH_ARGS="<caller-provided benchmark args>"
```

同一个 `SCOPE` 下切换编译器时必须使用不同 `BENCH_BUILD_DIR` 或清理旧构建目录；否则 CMake cache 会继续使用第一次配置时的编译器。

严格性能回归判断必须在同一环境、同一编译器、同一源码口径、同一参数下建立 baseline 和 result；x86 有噪声时需要重复运行。
