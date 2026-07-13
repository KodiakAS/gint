# 支持矩阵

本文定义 gint 的正式支持边界。它描述的是维护者承诺验证和处理回归的范围，
不是“某个编译器恰好能够编译”的兼容性猜测。

## 语言与编译器

- 公共 API 的最低语言级别是 C++11。CMake target `gint::gint` 会传递
  `cxx_std_11`，库代码不得要求 C++14 或更高标准。
- CMake 集成的最低版本是 3.13；直接复制头文件的集成不依赖 CMake。
- 正式支持 GCC、LLVM Clang 的 GNU-compatible driver，以及 AppleClang。
- Linux x86_64 的最低 GCC 版本是 4.8.5。这个版本下的正式契约仍是 C++11，
  并由独立的 legacy CI 阻断门禁验证。
- GCC 4.8.5 的最低版本承诺不外推到 Linux AArch64。AArch64 的 GCC baseline
  独立维护并继续使用该架构当前验证的较新工具链。
- 实现依赖 `__int128` 和 GCC/Clang builtin；这些能力属于支持契约的一部分。
- MSVC 和 `clang-cl` 明确不受支持。`clang-cl` 即使使用 Clang frontend，仍采用
  MSVC-compatible driver/ABI，因此不属于受支持的 Clang 配置。
- 其他编译器 frontend 当前不受支持。CMake 配置会在检测到不受支持的编译器时
  直接失败，而不是生成一个未经验证的构建。

除明确承诺的 Linux x86_64/GCC 4.8.5 外，项目不承诺覆盖每个历史编译器版本。
发布候选必须通过当前 CI 和 `docs/VALIDATION_ENVIRONMENTS.md` 中的验证环境；
发现其他旧版 GCC/Clang 回归时，以对应 OS/architecture 能够稳定运行正式
C++11 验证矩阵的最旧环境作为是否继续支持的依据。

### 0.9.0 validated baseline

| 平台 | Compiler / baseline | 说明 |
| --- | --- | --- |
| Linux x86_64 | GCC 4.8.5 | 最低兼容版本；固定 legacy 环境运行 C++11 Debug、Release/O3、consumer/package 与 differential 门禁 |
| Linux x86_64 | GCC 8.5 | AlmaLinux 8 开发镜像的完整 C++11 矩阵；保留为日常开发环境，不取代 4.8.5 最低版本门禁 |
| Linux AArch64 | GCC 13 + rolling GCC | 独立 AArch64 baseline；固定 GCC 13 覆盖 codegen/performance，正确性矩阵同时记录 rolling runner 的实际版本 |
| Linux x86_64 | LLVM Clang 12 | C++11 完整测试矩阵通过 |
| Linux AArch64 | rolling LLVM Clang | 独立 AArch64 门禁；发布时记录实际版本 |
| macOS arm64 | AppleClang 21 | macOS arm64 C++11 完整测试矩阵通过 |

GCC 4.8.5 是仅限 Linux x86_64 的明确最低版本承诺，不是全平台 GCC 下限。
其他条目是 0.9.0 发布准备中的验证 baseline，不表示更老版本受支持，也不表示
一个 compiler 版本能够替代其他 OS/architecture 组合的证据。CI 的
`ubuntu-latest`、`ubuntu-24.04-arm` 与 `macos-latest` 使用 rolling toolchain；
每次发布报告必须记录 runner 当时解析到的真实编译器版本，不能只写 runner
别名。

## 平台矩阵

| 平台 | 编译器 | 状态 |
| --- | --- | --- |
| Linux x86_64 | GCC 4.8.5+、Clang | 正式支持，发布门禁；GCC 4.8.5 是最低版本 |
| Linux AArch64 | GCC、Clang | 正式支持，发布门禁；编译器 baseline 独立维护 |
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
