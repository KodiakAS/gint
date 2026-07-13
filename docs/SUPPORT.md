# 支持策略

本文是语言、编译器、平台和版本生命周期的唯一权威来源。它描述维护者承诺验证
和处理回归的范围，不把“某个组合恰好能够编译”视为正式支持。

## 基础要求

| 项目 | 契约 |
| --- | --- |
| C++ | 公共 API 最低 C++11；更高标准不能替代 C++11 门禁 |
| CMake | 最低 3.13；直接复制头文件不依赖 CMake |
| Compiler frontend | GCC、LLVM Clang 的 GNU-compatible driver、AppleClang |
| 必要能力 | `__int128`、GCC/Clang builtin、64-bit little-endian target |
| 不支持 | MSVC、`clang-cl`、32-bit、big-endian |

CMake contract 会拒绝不受支持的 frontend 和版本低于 4.8.5 的 GNU GCC；
public header 还会独立拒绝低于 C++11 或缺少 `__int128` 的编译。这个能力门禁
不等同于把 GCC 4.8.5 承诺扩展到所有架构。

## 平台矩阵

| 平台 | 编译器 | 支持状态 |
| --- | --- | --- |
| Linux x86_64 | GCC 4.8.5+、LLVM Clang | 正式支持；GCC 4.8.5 是最低兼容版本 |
| Linux AArch64 | GCC、LLVM Clang | 正式支持；GCC baseline 独立维护 |
| macOS arm64 | AppleClang | 正式支持 |
| 其他 64-bit little-endian OS/架构 | GCC/Clang | best effort，不属于发布门禁 |
| Windows、32-bit、big-endian | 任意 | 不支持 |

Linux x86_64 的最低 GCC 版本不用于降低 AArch64 baseline。三个正式平台的结果
必须分别报告，不能以一个 OS、架构或编译器替代另一个组合。

## 支持承诺与验证快照

长期策略只固定上表的边界。CI runner、rolling compiler 和一次发布实际使用的
版本会随时间变化，应记录在对应 CI artifact 或发布证据中，不写入本文件形成
易过期的第二套矩阵。

可复现依赖和 legacy 环境见[验证环境](VALIDATION_ENVIRONMENTS.md)，发布门禁见
[发布流程](RELEASING.md)。

## 产品边界

gint 保持 header-only：

- 单独复制 `include/gint/gint.h` 是一级集成方式；
- `include/gint/core.h` 是可选的精简算术入口；
- CMake 只导出 `gint::gint` 与 `gint::checked` 两个 `INTERFACE_LIBRARY`；
- 安装产物不包含静态库或动态库。

具体集成方法见[集成指南](INTEGRATION.md)，精确安装清单由
[发布流程](RELEASING.md)维护。

## 0.x 维护策略

- 当前开发版本线为 `0.9.x`；正式发布状态以 Git tag 为准，版本内容见
  [变更记录](../CHANGELOG.md)。
- 同一 minor 内的 patch 版本保持源码兼容；CMake package 使用
  `SameMinorVersion` 匹配。
- 0.x minor 升级可以包含明确记录的源码或语义调整，消费者必须阅读
  [变更记录](../CHANGELOG.md)与[升级指南](UPGRADING.md)。
- 正式发布后只维护当前 minor；旧 minor 不承诺并行回补。
