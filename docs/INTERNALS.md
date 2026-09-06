# 实现说明

本文解释当前内部实现，帮助维护者评估 correctness、代码生成和性能影响。
这些细节不是独立的公共兼容承诺；公共行为以[技术规格](TECH_SPEC.md)为准。

## 源码组织

实现入口是 [`src/gint/gint.hpp`](../src/gint/gint.hpp)，由普通 `.hpp` 依赖图组成。
[`generate-amalgamation.py`](../scripts/generate-amalgamation.py) 将其确定性展开为
已提交的 `include/gint/gint.h`。编辑与同步命令见[贡献指南](../CONTRIBUTING.md)。

测试配置为内部入口提供 C++11、fmt、checked 和 `-fno-exceptions` 翻译单元，
写入 `compile_commands.json` 供语言服务使用。每个内部头必须可独立解析。

### 模块角色与依赖

生成器 manifest 将每个内部头归入角色并强制依赖方向；新增、删除或漏分类的
`.hpp` 都会生成失败。当前模块关系如下：

| 文件 | 职责 | 直接内部依赖 |
| --- | --- | --- |
| [`gint.hpp`](../src/gint/gint.hpp) | 显式组合完整接口，最后清理私有宏 | integer、standard、string_stream、fmt、cleanup |
| [`prelude.hpp`](../src/gint/prelude.hpp) | 编译器要求、版本和公共标准库头 | 无 |
| [`configuration.hpp`](../src/gint/configuration.hpp) | 功能策略、编译器属性和配置 namespace | prelude |
| [`integer.hpp`](../src/gint/integer.hpp) | 整数声明、traits、limb 运算、整数类及全部算术内核 | configuration |
| [`standard.hpp`](../src/gint/standard.hpp) | `std::numeric_limits`、`std::hash` 特化 | integer |
| [`string_stream.hpp`](../src/gint/string_stream.hpp) | 字符串解析、进制转换和 stream 输出 | integer |
| [`fmt.hpp`](../src/gint/fmt.hpp) | 可选 fmt 适配，复用文本转换函数 | string_stream |
| [`cleanup.hpp`](../src/gint/cleanup.hpp) | 清理私有实现宏 | 无 |

生成器将 prelude、configuration、integer、standard 依次归入 core 角色，
string_stream、fmt 依次归入 IO 角色；同角色只能依赖较早模块，IO 可依赖 core。
完整入口属于 distribution，可直接依赖 core、IO 和 cleanup；cleanup 不依赖其他模块。
标准库适配和字符串/stream 是整数实现之上的并列模块，fmt 的外部头由 `fmt.hpp`
在 `GINT_ENABLE_FMT` 条件内包含。

所有内部头都使用普通的 `#pragma once`，每个文件只展开一次。`gint.hpp` 显式列出
主要功能模块，各模块仍直接包含自身需要的依赖。`configuration.hpp` 建立私有实现宏，
完整入口在全部定义之后包含 `cleanup.hpp`，统一清理这些宏。入口的 include 按职责
分组，保留可读顺序并避免格式化工具把清理头提前。

内部模块不是公共入口；单独解析中间模块时会保留后续定义需要的实现宏，只有完整
入口承诺清理。内部图测试覆盖直接包含完整入口，以及先使用字符串/stream、fmt
模块再包含完整入口的顺序，并检查标准库适配和宏清理仍然完整。

### 整数实现的阅读路径

`integer.hpp` 依次包含类型声明、traits、limb 工具、文本接口声明、整数类、
公共运算符和私有算术内核。按运算符可找到除法、移位、浮点转换与平台特化。
文本接口的定义位于 `string_stream.hpp`，`fmt.hpp` 复用文本转换。

### 生成器输入与处理契约

生成器只接受受限的 C++ 预处理方言，不支持的输入直接报错。编写内部头时遵守：

- **文件与依赖**：以唯一、规范的 `#pragma once` 开始；本地 quoted include
  只能位于顶层无条件上下文。条件 include、宏 include 和内部 angle include 均禁止；
  外部 angle include 保留原拼写。
- **路径**：使用非空相对路径，不含空组件、`.`、反斜杠、空白或控制字符。
  `..` 只能沿源树内真实存在且非符号链接的目录回退。路径按物理文件身份去重和判环，
  拒绝逃出源树、缺失组件、symlink、hardlink alias 和非精确大小写。
  等价路径、内部命名空间大小写变体和物理别名均不能绕过内部 include 检查。
- **词法**：使用 LF 换行与规范续行；禁止块注释、raw string、trigraph 和 digraph。
  禁止形式按字节保守检查，注释和字符串中的同样拼写也受限制。
- **预处理**：禁止 `#import`、`#include_next`、文件搜索 operator、module/import
  控制行和 pragma operator，包括宏别名形式。token paste 仅用于项目配置 namespace
  宏。除 `#pragma once` 外，只允许 GCC/Clang 的 `diagnostic push/pop` 和
  `diagnostic ignored "-W..."`。
- **文件上下文**：禁止依赖文件名、行号、包含层级或文件时间的宏及 builtin，
  如 `__FILE__`、`__LINE__` 和 `__builtin_source_location`，避免展开前后值不同。

精确的禁止拼写、路径识别和拒绝用例由
[生成器](../scripts/generate-amalgamation.py)与
[生成器测试](../tests/perf/test_generate_amalgamation.py)维护。

处理顺序是：检查原始字节并拼接逻辑行，统一解析指令，校验条件栈、include 和
pragma，再按路径身份与模块角色展开全图。输出使用相同规则检查，不得残留内部
include；分发头仅保留一个 `#pragma once`。条件表达式和 C++ 宏展开由编译器处理。

生成器测试覆盖非法输入和源码图变体；`generator.compiler_equivalence_0/1`
使用当前 CMake 工具链分别编译内部图与独立生成头，对照声明、条件分支、宏清理
和运行结果。consumer 测试检查单头独立及重复包含、宏清理和完整 IO 接口。

## 数据布局

`integer<Bits, Signed>` 使用 `Bits / 64` 个 `std::uint64_t` limb，低位 limb 位于
较小索引。对象保持 `Bits / 8` 字节大小，有符号值直接使用同一补码位模式。

## 算法结构

### 加减与乘法

- 加减逐 limb 传播进位或借位，常用 256-bit 路径显式展开。
- x86-64 GCC 与 AArch64 Clang 的 256-bit 整数加减 `limb_type` 标量使用固定长度传播。
  加法通过溢出比较传播进位，AppleClang 保留布尔进位形式；减法在 AArch64 LLVM Clang
  使用两个 128-bit 半部传递借位，x86-64 GCC 与 AppleClang 使用四 limb 传播。
  这些差异用于避免短链和调用方额外 outlining 的退化；其他组合及位宽保留原有提前结束路径。
- 128-bit 乘法使用 `__int128`；256-bit 使用定长 Comba 风格累加。
- 512/1024-bit 使用 O(n²) 学校乘法。
- 宽整数与不超过 64-bit 的无符号内建整数相乘时，两种操作数顺序均走单 limb
  乘法（不含 `bool`）；`bool`、有符号标量和 128-bit 标量保留先构造宽整数的路径。

### 除法与取模

除法按操作数形态、架构和编译器选择：

- 2 的幂转为移位和掩码；
- 单 limb 除数使用原生 128/64 运算或 32/64-bit 倒数估商路径；
- 常见 2/3-limb 和 256-bit 满宽场景使用定长热点内核；
- 其他多 limb 情况使用规范化的 Knuth Algorithm D。

`divmod` 复用一次商计算，再以 `dividend - quotient * divisor` 重建余数。部分
架构和编译器对 `%` 有独立直接求余路径；这些分派不能改变公共除模语义。

### 文本与浮点

- 十进制输出按 `10^19` 分块，避免逐字符头插造成的重复移动。
- 2/8/16 进制解析按 digit chunk 打包；其他进制使用分块乘加。
- 浮点比较按指数和有效位对齐，不把宽整数整体降精度为 `long double`。
- 宽整数转浮点统一使用 guard/sticky bits，避免逐 limb 累加的二次舍入。

## 编译器与配置隔离

实现依赖 `__int128` 和 GCC/Clang builtin。编译器相关路径由
`GINT_GCC_TUNED_PATHS`、`GINT_CLANG_TUNED_PATHS` 与
`GINT_ENABLE_AARCH64_LIMB_ASM` 选择；默认值由目标平台和 frontend 推导，普通
消费者不应手动覆盖。

GCC 4.8.5 缺少较新的 carry/borrow intrinsic，x86_64 兼容路径使用
`unsigned __int128` 标量实现。正式支持边界由[支持策略](SUPPORT.md)定义，不能
从某个内部 fallback 推导额外平台承诺。

影响行为或代码生成的策略进入 inline namespace，防止不同配置的 header-only
定义通过 COMDAT/weak inline 链接顺序相互替代。配置一致性要求见
[集成指南](INTEGRATION.md)。

## 性能维护

固定宽度 helper、强制内联和循环改写都可能改变寄存器分配或产生新的调用。
涉及这些路径时按[基准测试](BENCHMARKS.md)检查代码生成并进行同环境前后采样。
