# Changelog

本文记录对使用者可见的变化。版本策略与支持周期见 `docs/SUPPORT.md`，发布步骤
见 `docs/RELEASING.md`。

## [0.9.0] - Unreleased

### Added

- 增加项目版本、单头文件版本宏和可安装的 `gintConfigVersion.cmake`。
- 增加 package version 的 exact、compatible 和 incompatible consumer 回归。
- 安装 Apache-2.0 `LICENSE`，并验证完整 header-only 安装清单。
- 增加正式支持矩阵、发布流程与升级指南。
- 增加 Linux x86_64/AArch64 的确定性差分 oracle、Clang libFuzzer smoke 与定时
  深度 fuzz，持续覆盖宽除法、字符串解析和整数到浮点转换。
- 增加 GCC/Clang/AppleClang 的 C++11 codegen contract；定时性能工作流使用固定
  Google Benchmark v1.9.5 保存完整矩阵、1024-bit 满位宽/短输入 parser 样本与
  噪声摘要。
- 增加 `<gint/core.h>` 精简算术入口、`gint::divmod` 和 CMake `gint::gint` /
  `gint::checked` INTERFACE targets。

### Fixed

- 修复 2/8/16 进制快速解析中的字符分类下溢；十六进制输入不再把 `@` 或 `` ` ``
  错误接受为数字 9。
- 修复宽整数转 `float` / `double` / `long double` 的二次舍入，以及混合类型
  `/=`、`%=` 与对应二元运算不一致的问题。
- 修复负宽整数溢出转换为浮点数时，`FE_UPWARD` / `FE_DOWNWARD` 错误沿用正数
  中间值舍入方向的问题。

### Changed

- 正式支持范围限定为 GCC、LLVM Clang 和 AppleClang，最低语言标准保持 C++11。
- CMake 在 MSVC、`clang-cl` 或其他不受支持的 compiler frontend 下直接停止配置。
- 0.x CMake package 采用 `SameMinorVersion` 兼容策略：patch 保持兼容，minor
  升级需要消费者显式确认。
- Linux/GCC 开发镜像改用仍受维护的 AlmaLinux 8 基础，同时保留历史
  `gint:centos8` 本地 tag 兼容现有自动化。
- CI 补齐 Linux AArch64 Clang，与既有 Linux x86_64 GCC/Clang、AArch64 GCC 和
  macOS arm64 AppleClang 一起构成支持矩阵。
- 开发镜像门禁在实际 x86_64/GCC 8.5 环境中运行完整 C++11 测试矩阵，不再只
  检查编译器版本。
- 2/8/16 进制解析使用常量时间字符分类；在保持严格输入校验的同时降低宽整数
  Base8/Base16 解析开销。
