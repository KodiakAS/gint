# 升级指南

本文只记录消费者在版本迁移时需要采取的动作。通用接入方法见
[集成指南](INTEGRATION.md)，兼容策略见[支持策略](SUPPORT.md)，具体变化见
[CHANGELOG](../CHANGELOG.md)。

## CMake 版本要求

按[版本策略](SUPPORT.md#0x-维护策略)，升级 minor 时需要显式修改 package 请求：

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

已安装的 `0.9.0` 可以满足 `0.9` 或 `0.9.0 EXACT`，不能满足更新的 patch 请求，
也不被视为其他 minor 的兼容版本。

## 直接分发头文件

- 项目只需分发并更新 `include/gint/gint.h`。
- 更新后全量重编译所有包含 gint 的翻译单元；header-only 实现不会通过运行时
  动态库升级生效。
- 可用 `GINT_VERSION_MAJOR/MINOR/PATCH` 与 `GINT_VERSION` 做编译期版本检查。

## 升级到 0.9.x

从未版本化的旧 checkout 升级时：

- 原有 `#include <gint/gint.h>` 和单头文件方式继续有效；
- CMake 消费者可以使用版本化 package、`gint::gint` 和 `gint::checked`；
- compiler frontend、平台和最低版本以当前[支持策略](SUPPORT.md)为准，旧工具链
  不应通过关闭 tuned path 绕过能力门禁。
