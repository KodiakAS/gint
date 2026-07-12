# 升级指南

## CMake package 消费者

推荐在 0.9 版本线使用 minor 约束：

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

安装的 0.9.0 可以满足 `0.9` 或 `0.9.0 EXACT` 请求，不能满足 `0.9.1` 请求，
也不会被当作 0.8.x 的兼容更新。后续 0.9.x patch 可以满足不高于自身版本的
0.9 请求；跨 minor 升级需要消费者显式修改版本约束。

默认 target 是 `gint::gint`。需要除零检查时使用 `gint::checked`；不要在同一
target 的不同 translation unit 中手工混用影响语义或代码生成的配置宏。

## 直接复制头文件

- 只使用 umbrella header 的项目仍可仅复制新的 `include/gint/gint.h`，不需要
  CMake，也不需要编译或链接 gint 库。
- 使用 `<gint/core.h>` 的项目应同时更新 `core.h` 与 `gint.h`。
- `GINT_VERSION_MAJOR`、`GINT_VERSION_MINOR`、`GINT_VERSION_PATCH` 和
  `GINT_VERSION` 可用于编译期版本检查；这些宏直接位于 `gint.h`，单文件复制
  不依赖生成头或额外 metadata。
- 更新头文件后必须全量重编译所有包含 gint 的 translation unit。header-only
  实现及配置 inline namespace 都不会通过运行时动态库升级生效。

## 升级到 0.9.x

从未版本化的旧 checkout 升级到 0.9.x 时：

- 原有 `#include <gint/gint.h>` 和直接复制单头文件的方式保持有效；
- 安装产物新增 `gintConfigVersion.cmake` 与 `share/gint/LICENSE`，但不新增任何
  静态库或动态库；
- CMake 消费者可以开始声明版本要求，并继续使用 `gint::gint` 或
  `gint::checked`；
- `core.h` 是可选精简入口，不会取代 `gint.h`；
- MSVC 与 `clang-cl` 明确不属于支持范围。支持的最低语言标准仍为 C++11。

每次 minor 升级前应阅读 `CHANGELOG.md`。0.x minor 可以调整源码或语义契约，
不能仅凭“头文件仍能编译”推断行为完全兼容。
