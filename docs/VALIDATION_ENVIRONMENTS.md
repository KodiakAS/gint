# 验证环境

本文只负责依赖固化、compiler env 和输出目录。测试范围由变更风险决定，见
[贡献指南](../CONTRIBUTING.md)；正式支持边界见[支持策略](SUPPORT.md)。

## 边界

- 仓库脚本准备当前环境并运行调用方指定的目标。
- 外部编排负责执行位置、生命周期、代码同步和结果回收。
- 不同 OS、架构和编译器使用独立构建目录并分别报告。

常见环境形态：

| 环境 | 主要用途 |
| --- | --- |
| macOS arm64 + AppleClang | 本机正确性、sanitizer、真实 macOS 集成 |
| Linux AArch64 + GCC/Clang | AArch64 正确性与代码生成 |
| Linux x86_64 + GCC/Clang | x86 正确性、代码生成与固定主机性能 |
| AlmaLinux 8 x86_64 + GCC 8.5 | 日常 Linux/GCC 完整 C++11 矩阵 |
| Linux x86_64 + GCC 4.8.5 | 最低编译器 correctness 和 integration |

## 目录约定

所有构建、依赖源码、依赖安装、日志和 benchmark 结果都位于 `runs/`：

```text
runs/<scope>/deps/   # 固定版本依赖的源码、构建和安装
runs/<scope>/env/    # 可 source 的 compiler env
runs/<scope>/...     # 当前任务的构建与结果
```

scope 只用于隔离产物，不表示机器位置。同一 scope 切换编译器时仍必须使用独立
build 目录，避免复用旧 CMake cache。

## 初始化当前环境

```sh
scripts/bootstrap-validation-env.sh --scope linux-x86_64
```

脚本会：

1. 在 Ubuntu/Debian 安装基础依赖；其他系统可传 `--skip-packages`；
2. 在 scope 下为所选编译器构建 Release Google Benchmark v1.9.5；
3. 生成包含 `CMAKE_CXX_COMPILER`、`CMAKE_PREFIX_PATH` 和
   `GINT_COMPILER_ID` 的 env 文件。

包清单和参数以
[`scripts/bootstrap-validation-env.sh`](../scripts/bootstrap-validation-env.sh)
为准，不在文档复制。覆盖默认 benchmark 版本时还必须提供与下载归档匹配的
`--benchmark-sha256`。

不要使用发行版 `libbenchmark-dev` 产出性能结论。Google Benchmark 本身使用
C++17 构建，不改变 gint 公共 API 和 correctness 测试的 C++11 契约。

## 使用 compiler env

env 文件名由编译器绝对路径安全化生成，例如 `/usr/bin/g++` 对应
`usr_bin_gxx.env`。脚本结束时会打印实际文件和 `source` 示例。

```sh
source runs/linux-x86_64/env/usr_bin_gxx.env
cmake -S . \
  -B "runs/linux-x86_64/build-${GINT_COMPILER_ID}" \
  -DGINT_BUILD_TESTS=ON \
  -DGINT_BUILD_BENCHMARKS=OFF \
  -DCMAKE_CXX_COMPILER="$CMAKE_CXX_COMPILER"
```

benchmark 目标和采样参数见[基准测试](BENCHMARKS.md)。

## Linux/GCC 开发镜像

`make image` 构建 AlmaLinux 8 / GCC 8.5 开发镜像。为兼容既有自动化，本地 tag
仍为 `gint:centos8`，但基础系统不是已停止维护的 CentOS 8。该镜像用于日常
Linux/GCC C++11 测试和固定工具链 benchmark，与下一节最低编译器镜像职责不同。

## GCC 4.8.5 legacy 环境

最低编译器门禁使用固定的 CentOS 7.9 x86_64 镜像，包含 GCC/G++ 4.8.5、
CMake 3.13.5、fmt 9.1.0 与 GoogleTest 1.10.0：

```sh
docker build --platform linux/amd64 --file Dockerfile.gcc48 --tag gint:gcc48 .
docker run --rm --platform linux/amd64 \
  --volume "$PWD:/work" --workdir /work \
  gint:gcc48 scripts/run-gcc48-validation.sh
```

该 lane 运行 C++11 Debug、Release/O3、header/consumer/package contract 与
differential。它不构建 C++17 benchmark，不承担 sanitizer、现代 codegen 或
性能结论；这些证据由现代工具链环境提供。默认输出在 `runs/gcc48/`，可调参数以
[`scripts/run-gcc48-validation.sh`](../scripts/run-gcc48-validation.sh)为准。
