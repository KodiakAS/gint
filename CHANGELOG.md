# 变更记录

本文只记录使用者可感知的变化。版本策略见[支持策略](docs/SUPPORT.md)，发布步骤
见[发布流程](docs/RELEASING.md)。

## [0.9.0] - Unreleased

### 新增

- 增加项目版本、单头文件版本宏和可安装的 `gintConfigVersion.cmake`。
- 增加 `gint::gint`、`gint::checked` 两个 header-only CMake targets，以及
  package version 与安装清单验证。
- 增加 `<gint/core.h>` 精简算术入口和同时返回商、余数的 `gint::divmod`。
- 增加正式支持策略、集成说明、升级指南和发布清单。
- 将单头文件实现拆分为可被 clangd、IDE 和静态分析器直接解析的内部 `.hpp`
  依赖图，并增加确定性 amalgamation 生成器、模块分层、definition-pass 生命周期、
  内部头编译契约与同步门禁；用户交付物仍是已提交的 `gint.h`，不新增 consumer
  构建依赖。

### 修复

- 修复 2/8/16 进制快速解析中的字符分类下溢；十六进制输入不再把 `@` 或 `` ` ``
  错误接受为数字 9。
- 修复宽整数转 `float` / `double` / `long double` 的二次舍入，以及混合类型
  `/=`、`%=` 与对应二元运算不一致的问题。
- 修复负宽整数溢出转换为浮点数时，`FE_UPWARD` / `FE_DOWNWARD` 错误沿用正数
  中间值舍入方向的问题。

### 变更

- 正式支持范围限定为 GCC、LLVM Clang 和 AppleClang，最低语言标准保持 C++11；
  Linux x86_64 的 GCC 最低版本为 4.8.5，Linux AArch64 维护独立 baseline。
- CMake 与 public header 对不满足 compiler/frontend 契约的配置 fail-closed；
  GCC 4.8.5 使用兼容的 scalar `__int128` carry/borrow 路径。
- 0.x CMake package 采用 `SameMinorVersion`：patch 保持兼容，minor 升级需要
  消费者显式确认。
- 2/8/16 进制解析使用常量时间字符分类和分块路径，在保持严格输入校验的同时
  降低宽整数解析开销。
