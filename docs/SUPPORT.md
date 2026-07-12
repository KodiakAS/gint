# 支持矩阵

本文定义 gint 的正式支持边界。它描述的是维护者承诺验证和处理回归的范围，
不是“某个编译器恰好能够编译”的兼容性猜测。

## 语言与编译器

- 公共 API 的最低语言级别是 C++11。CMake target `gint::gint` 会传递
  `cxx_std_11`，库代码不得要求 C++14 或更高标准。
- CMake 集成的最低版本是 3.13；直接复制头文件的集成不依赖 CMake。
- 正式支持 GCC、LLVM Clang 的 GNU-compatible driver，以及 AppleClang。
- 实现依赖 `__int128` 和 GCC/Clang builtin；这些能力属于支持契约的一部分。
- MSVC 和 `clang-cl` 明确不受支持。`clang-cl` 即使使用 Clang frontend，仍采用
  MSVC-compatible driver/ABI，因此不属于受支持的 Clang 配置。
- 其他编译器 frontend 当前不受支持。CMake 配置会在检测到不受支持的编译器时
  直接失败，而不是生成一个未经验证的构建。

项目不承诺覆盖每个历史编译器版本。发布候选必须通过当前 CI 和
`docs/VALIDATION_ENVIRONMENTS.md` 中的验证环境；发现旧版 GCC/Clang 回归时，
以能够稳定运行完整 C++11 测试矩阵的最旧环境作为是否继续支持的依据。

### 0.9.0 validated baseline

| Compiler | 已实测最低版本 | 说明 |
| --- | ---: | --- |
| GCC | 8.5 | C++11 完整测试矩阵通过 |
| LLVM Clang | 12 | C++11 完整测试矩阵通过 |
| AppleClang | 21 | macOS arm64 C++11 完整测试矩阵通过 |

这些版本是 0.9.0 发布准备中实际通过验证的 baseline，不是对更老版本的兼容
承诺，也不表示每个 compiler/OS/architecture 组合都由该最低版本覆盖。CI 的
`ubuntu-latest`、`ubuntu-24.04-arm` 与 `macos-latest` 使用 rolling toolchain；
每次发布报告必须记录 runner 当时解析到的真实编译器版本，不能只写 runner
别名。

## 平台矩阵

| 平台 | 编译器 | 状态 |
| --- | --- | --- |
| Linux x86_64 | GCC、Clang | 正式支持，发布门禁 |
| Linux AArch64 | GCC、Clang | 正式支持，发布门禁 |
| macOS arm64 | AppleClang | 正式支持，发布门禁 |
| Windows | MSVC、`clang-cl` | 不支持 |
| 32-bit target | 任意 | 不支持 |
| big-endian target | 任意 | 未验证，不支持 |
| 其他 OS/架构组合 | GCC/Clang | best effort，不属于发布门禁 |

支持矩阵只覆盖 64-bit little-endian target。跨平台验证必须分别报告，不将
macOS arm64、Linux AArch64 和 Linux x86_64 的结果相互替代。

## 集成方式

下列三种方式都属于正式支持的 header-only 集成：

1. 只复制并包含 `include/gint/gint.h`；
2. 复制 `include/gint/core.h` 与 `include/gint/gint.h`，按需使用精简入口；
3. 使用 CMake `add_subdirectory` / `FetchContent`，或安装后通过
   `find_package(gint CONFIG REQUIRED)` 消费 `gint::gint` / `gint::checked`。

安装包只包含头文件、CMake metadata 和许可证，不生成或安装静态库、动态库。

## 0.x 维护策略

- 当前版本线是 0.9.x。
- 同一 minor 内的 patch 版本保持源码兼容；CMake package 使用
  `SameMinorVersion` 规则进行版本匹配。
- 0.x 的 minor 升级可能包含有意的源码或语义调整，消费者必须阅读
  `CHANGELOG.md` 与 `docs/UPGRADING.md` 并显式更新版本要求。
- 正式发布后只维护当前 minor；旧 minor 不承诺并行回补修复。
