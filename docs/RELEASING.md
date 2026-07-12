# 发布流程

gint 是 header-only 库。发布物的核心是源码、头文件和 CMake package metadata，
不是预编译的 `.a`、`.so`、`.dylib`、`.dll` 或 `.lib`。

## 版本规则

- 版本采用 `MAJOR.MINOR.PATCH` 数字格式，Git tag 使用 `vMAJOR.MINOR.PATCH`。
- 1.0 之前，同一 minor 内的 patch 发布必须兼容；minor 发布允许有明确记录的
  破坏性变化。
- `CMakeLists.txt` 的 `project(... VERSION ...)` 与 `gint.h` 的
  `GINT_VERSION_MAJOR`、`GINT_VERSION_MINOR`、`GINT_VERSION_PATCH` 必须同时更新。
  `GINT_VERSION` 使用 `major * 10000 + minor * 100 + patch` 编码。
- 安装 package 的 `gintConfigVersion.cmake` 使用 `SameMinorVersion`；改变兼容策略
  属于发布策略变更，必须在 changelog 中说明。

## 发布候选检查

1. 更新 CMake 版本、头文件版本宏和 `CHANGELOG.md`，并确认
   `docs/UPGRADING.md` 覆盖本次升级注意事项。
2. 在 C++11 模式完成 Debug、Release/O3、sanitizer 和 consumer 测试。高标准
   编译测试可以补充，但不能替代 C++11 门禁。
3. 按 `docs/SUPPORT.md` 与 `docs/VALIDATION_ENVIRONMENTS.md` 完成 Linux x86_64、
   Linux AArch64 和 macOS arm64 的 GCC/Clang/AppleClang 验证。
4. 运行安装 consumer 回归，确认：
   - 无版本、兼容版本和 `EXACT` 版本请求成功；
   - 更新的 patch 请求及其他 0.x minor 请求被拒绝；
   - CMake version、安装 config 与头文件版本宏一致；
   - 安装清单只有头文件、CMake metadata 和许可证，没有二进制库。
5. 若变更触及 hot path，按 `AGENTS.md` 的固定过滤器在同一机器、同一工具链、
   相同参数下采集前后基准。性能退化必须解决或在发布说明中明确批准。
6. 检查 public headers 能够在 `-std=c++11 -Wall -Wextra -Werror` 下独立编译，
   并确认单独复制 `gint.h` 的 smoke test 通过。
7. 等待 pull request 的 review 与全部远端 checks 通过后再创建 tag。

所有本地构建、安装前缀、日志和基准结果必须放在仓库的 `runs/` 下。

## 安装清单

一次标准 `cmake --install` 应产生：

```text
<prefix>/include/gint/core.h
<prefix>/include/gint/gint.h
<prefix>/<libdir>/cmake/gint/gintConfig.cmake
<prefix>/<libdir>/cmake/gint/gintConfigVersion.cmake
<prefix>/<libdir>/cmake/gint/gintCompilerContract.cmake
<prefix>/<libdir>/cmake/gint/gintTargets.cmake
<prefix>/<datadir>/gint/LICENSE
```

其中 `<libdir>` 和 `<datadir>` 分别由 `GNUInstallDirs` 的
`CMAKE_INSTALL_LIBDIR`、`CMAKE_INSTALL_DATADIR` 决定。`gint::gint` 与
`gint::checked` 都必须导出为 `INTERFACE_LIBRARY`。

## Tag 与发布说明

1. 从已通过门禁的提交创建 annotated tag `vMAJOR.MINOR.PATCH`。
2. 推送 tag，并确认远端 tag 指向与已验证提交完全一致。
3. 创建 GitHub Release；摘要列出 correctness、performance、integration 和
   compatibility 变化，并链接升级说明。
4. 使用 GitHub source archive 或额外的源码归档作为交付物；不要附加预编译库，
   除非未来通过独立提案改变 header-only 产品边界。
5. 发布后将 changelog 中对应版本从 `Unreleased` 改为发布日期，并开始下一个
   开发版本条目。
