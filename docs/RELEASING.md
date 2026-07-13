# 发布流程

本文是维护者发布 0.9.x 的检查清单。支持矩阵、环境准备和性能方法分别由
[支持策略](SUPPORT.md)、[验证环境](VALIDATION_ENVIRONMENTS.md)和
[基准测试](BENCHMARKS.md)定义，本文件不复制其参数。

## 版本同步

- 版本格式为 `MAJOR.MINOR.PATCH`，Git tag 为 `vMAJOR.MINOR.PATCH`。
- 同时更新 `CMakeLists.txt` 的 `project(... VERSION ...)` 与
  `include/gint/gint.h` 的 `GINT_VERSION_MAJOR/MINOR/PATCH`。
- `GINT_VERSION` 保持 `major * 10000 + minor * 100 + patch` 编码。
- package version 继续使用 `SameMinorVersion`；改变该规则必须同步支持策略、
  升级指南和变更记录。

## 发布候选检查

1. 更新版本、[CHANGELOG](../CHANGELOG.md)和本次需要的
   [升级说明](UPGRADING.md)。
2. 在[正式支持矩阵](SUPPORT.md#平台矩阵)上完成 C++11 Debug、Release/O3 和
   consumer 验证，并记录每个环境的实际 OS、架构和编译器版本。
3. 运行 AlmaLinux 8/GCC 8.5 完整 C++11 矩阵和固定的 Linux x86_64/GCC 4.8.5
   lane；同时在适用的现代 GCC、Clang 或 AppleClang lane 上保留 sanitizer、
   differential/fuzz 与 codegen 证据。
4. 验证 CMake 3.13 package contract、public header 独立包含、`core.h` 单独包含
   和 `core.h -> gint.h` 两阶段包含。
5. 若修改 hot path，按[基准测试](BENCHMARKS.md)完成受影响架构的 codegen
   contract 和同环境前后采样；未经解释的退化不能进入发布。
6. 检查安装清单与下一节完全一致，且没有 `.a/.so/.dylib/.dll/.lib`。
7. 等待目标提交的全部 CI 与 review 通过，确认本地提交、PR head 和验证对象一致。

所有本地产物都应位于 `runs/`。最低 CMake 3.13 不支持 `cmake --install`；手工
验证安装时使用 `cmake --build <build-dir> --target install`，或运行生成的
`cmake_install.cmake`。

## 安装清单

标准安装必须且只能产生：

```text
<prefix>/include/gint/core.h
<prefix>/include/gint/gint.h
<prefix>/<libdir>/cmake/gint/gintConfig.cmake
<prefix>/<libdir>/cmake/gint/gintConfigVersion.cmake
<prefix>/<libdir>/cmake/gint/gintCompilerContract.cmake
<prefix>/<libdir>/cmake/gint/gintTargets.cmake
<prefix>/<datadir>/gint/LICENSE
```

`<libdir>` 与 `<datadir>` 分别由 `GNUInstallDirs` 的
`CMAKE_INSTALL_LIBDIR`、`CMAKE_INSTALL_DATADIR` 决定。`gint::gint` 和
`gint::checked` 必须都是 `INTERFACE_LIBRARY`。

机器检查由
[`tests/cmake/run_consumer.cmake`](../tests/cmake/run_consumer.cmake)维护；修改安装
规则时先更新该 contract，再同步本清单。

## Tag 与 GitHub Release

1. 从已通过门禁的提交创建 annotated tag `vMAJOR.MINOR.PATCH`。
2. 推送 tag，确认远端 tag 仍指向同一提交。
3. 创建 GitHub Release，摘要列出 correctness、performance、integration 和
   compatibility 变化，并链接升级说明。
4. 使用 GitHub source archive 或额外源码归档交付；header-only 边界未改变时
   不附加预编译库。
5. 发布后将 changelog 条目标记为发布日期，并建立下一开发版本的 Unreleased
   条目。
