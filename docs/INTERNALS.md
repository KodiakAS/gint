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
| [`limb_ops.hpp`](../src/gint/limb_ops.hpp) | 整数前置声明、traits、定长 limb 运算 | configuration |
| [`integer_type.hpp`](../src/gint/integer_type.hpp) | 整数布局、转换、运算符和通用辅助函数 | limb_ops |
| [`integer_division.hpp`](../src/gint/integer_division.hpp) | `detail::division_ops` 的除模内核、公开 `divmod` | integer_type |
| [`integer.hpp`](../src/gint/integer.hpp) | 整数入口、静态成员定义和测试适配 | integer_division |
| [`standard.hpp`](../src/gint/standard.hpp) | `std::numeric_limits`、`std::hash` 特化 | integer |
| [`string_stream.hpp`](../src/gint/string_stream.hpp) | 字符串解析、进制转换和 stream 输出 | integer |
| [`fmt.hpp`](../src/gint/fmt.hpp) | 可选 fmt 适配，复用文本转换函数 | string_stream |
| [`cleanup.hpp`](../src/gint/cleanup.hpp) | 清理私有实现宏 | 无 |

生成器将 prelude、configuration、limb_ops、integer_type、integer_division、integer、
standard 依次归入 core 角色，
string_stream、fmt 依次归入 IO 角色；同角色只能依赖较早模块，IO 可依赖 core。
完整入口属于 distribution，可直接依赖 core、IO 和 cleanup；cleanup 不依赖其他模块。
标准库适配和字符串/stream 是整数实现之上的并列模块，fmt 的外部头由 `fmt.hpp`
在 `GINT_ENABLE_FMT` 条件内包含。

所有内部头都使用普通的 `#pragma once`，每个文件只展开一次。`gint.hpp` 显式列出
主要功能模块，各模块仍直接包含自身需要的依赖。`configuration.hpp` 建立私有实现宏，
完整入口在全部定义之后包含 `cleanup.hpp`，统一清理这些宏。入口的 include 按职责
分组，保留可读顺序并避免格式化工具把清理头提前。

内部模块不是公共入口；单独解析中间模块时会保留后续定义需要的实现宏，只有完整
入口承诺清理。内部图测试覆盖直接包含完整入口，以及先使用整数层、字符串/stream
或 fmt 模块再包含完整入口的顺序，并检查标准库适配和宏清理仍然完整。

### 整数实现的阅读路径

从 `integer_type.hpp` 的整数类查看布局、文本接口声明和运算符分派。底层加减、
乘法和位运算可沿调用进入 `limb_ops.hpp`。运算符通过依赖模板
`detail::division_ops<Bits, Signed>` 调用 `integer_division.hpp` 中的静态内核，
友元声明允许内核访问 limb 布局。需要完整整数实现时包含 `integer.hpp`。
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

- 128-bit 同型加减的运行时 helper 使用原生 `unsigned __int128`。C++14 起的常量求值，
  以及 C++14 起缺少 `__builtin_is_constant_evaluated` 的构建，使用逐 limb 的标量
  helper。右操作数为 `limb_type` 的 128-bit 加减直接使用原生 128-bit 表达式，
  支持常量求值。其他位宽逐 limb 传播进位或借位，常用 256-bit 路径显式展开。
- x86-64 GCC 与 AArch64 Clang 的 256-bit 整数加减 `limb_type` 标量使用固定长度传播。
  加法通过溢出比较传播进位。AArch64 LLVM Clang 的减法使用两个 128-bit 半部传递借位，
  x86-64 GCC 与 AppleClang 使用四 limb 传播。
  这些差异用于避免短链和调用方额外 outlining 的退化；其他组合及位宽保留原有提前结束路径。
- 128-bit 乘法使用 `__int128`；256-bit 使用定长 Comba 风格累加。
- 512/1024-bit 使用 O(n²) 学校乘法。
- 不超过 64-bit 的内建整数（不含 `bool`）在两种乘法操作数顺序和 `*=` 中都走
  标量路径。`*=` 原位更新各 limb。64/128-bit 宽整数使用原生无符号乘法，
  有符号标量按结果位宽转换。更宽的整数逐 limb 乘以标量幅值，负标量对结果取补码。
  幅值在无符号域计算，覆盖 `INT64_MIN`。AArch64 Clang 上，256-bit 整数乘以
  64-bit 无符号标量或有符号标量的幅值时，用一条进位链累加对齐的部分积。
  512/1024-bit 只有低 limb 非零或所有 limb 为零时，乘以有符号标量可直接计算原生
  128-bit 乘积，负标量再向高位符号扩展。`bool` 和原生 128-bit 标量先构造宽整数。

### 移位

128-bit 左移和无符号右移在检查位移量后使用原生 128-bit 运算。有符号右移使用
受支持编译器的原生算术右移，位移量达到位宽时由 gint 直接生成符号填充值。

### 除法与取模

两个 128-bit 宽整数的除模在无符号域计算幅值，最后还原符号，因此最小值除以
`-1` 可按固定位宽回绕。求商时，如两数均为负数，可直接比较补码位模式提前判商为零。
除数幅值只有一个 limb 且不为 2 的幂时，直接使用原生除模。2 的幂使用移位和掩码。
除数幅值至少为 `2^127` 时，比较即可得到商是 0 还是 1。AArch64 上，介于
`2^126` 和 `2^127` 之间的除数可用最多三次减法求商。

除数幅值在 `[2^64, 2^126)` 时，规范化后用高 limb 估商，再用余数作一次修正。
设 `k` 是舍去的低位数，截断后的除数为 `d0`，原除数为 `d`。有
`d >= d0 >= 2^(63+k)`，且 `d - d0 < 2^k`。当 `k >= 2` 时，将
`d0` 代入除法造成的商偏差小于 `2^(2-k)`。当 `k == 1` 时，截断最多减
1，偏差同样小于 1。因此取整后的估商最多偏大 1。先从估商减 1 再乘回，
可保证乘积不溢出，若所得余数仍大于等于除数则修正一次。

64-bit 同型取模直接使用原生无符号余数。有符号输入先计算幅值，再按被除数恢复
余数符号，覆盖 `INT64_MIN % -1`。128-bit 同型运算使用上面的独立入口。
更宽的同型取模先检测正单 limb 除数。无符号 256-bit 整数对不超过 32-bit 的
无符号标量取模时，软件倒数路径保留除数的窄类型。求商也使用正单 limb 除数的捷径，
但 x86-64 Clang 将此检测留在通用入口，避免影响较窄稠密除数的除模及同一翻译
单元的文本解析。AArch64 GCC 的无符号 256-bit 求商在调用点按除数的有效 limb 数
分派，多 limb 的 2 次幂用已知范围内的右移求商。
直接传入单 limb 标量并同时求商和余数时，由原生商乘回得到余数。

其他除法按操作数形态、架构和编译器选择：

- 2 的幂转为移位和掩码；
- 64-bit 和 128-bit 被除数分别使用原生 64/64 和 128/64 运算；
  非 x86-64 上，256-bit 及以上使用 32/64-bit 倒数估商路径。求商和单独求余
  共用每步估商和一次修正，单独求余只读取被除数；
- x86-64 上，256-bit 及以上整数对运行时单 limb 除数逐 limb 使用 `divq`。
  除零和 2 的幂由前置路径处理，因此进入循环时除数至少为 3。初始余数为零，
  每一步的余数都小于除数，所以下一个 128-bit 被除数严格小于除数乘以 `2^64`，
  商可放入一个 limb。标量取模的单 limb 路径直接读取被除数，负数先转换为幅值。
  3-limb 除数的估商步骤在分子最高 limb 小于除数最高 limb 时也使用 `divq`。
  256-bit 及以上整数对十进制分块等编译期固定的 64-bit 除数使用编译期倒数，
  包括 1024-bit。
- 常见 2/3-limb 和 256-bit 满宽场景使用定长热点内核。2-limb 除数的规范化高 limb
  等于 `2^63` 时，直接用移位估商，省略倒数计算。满宽 256-bit 求商和求余
  共用规范化、估商和乘减链。规范化最多左移一位时，估商最多为三，可用比较与减法
  得到。生成余数时，修正后的规范化余数严格小于 `2^256`，低四个 limb 反移位即得
  最终余数；
- 多 limb 幅值有效长度相同时，宽整数通用单结果路径使用前几个 limb 估算单 limb 商，
  再用乘回比较或乘减及加回完成修正。其余通用路径使用规范化的 Knuth Algorithm D，
  同一内核按模板参数生成商、余数或同时生成两者。x86-64 Clang 的通用求商入口继续使用 Algorithm D，
  因为增加等长单商分派会放慢较窄稠密除数的除模组合，并改变宽八进制解析的内联。

`divmod` 在 256-bit 及以上由普通 `inline` 的公开入口分派无符号内核。256-bit 在最高
limb 已能判定被除数更小时直接返回，并复用定宽的 2 的幂判定。其余情况用除数幅值的
有效 limb 数判零。更宽的多 limb 除数仅在最高有效 limb 为 2 的幂时检查低位是否全零。
2 的幂直接使用移位和掩码。
超过 256-bit 且除数至少有四个有效 limb 时，等长幅值只对最高几个 limb 估商，
通过一次乘减及至多一次加回修正商和余数。较短的等长幅值保留原本为零的高 limb。
通用双输出入口从规范化后的被除数提取余数，并将余数未使用的高 limb 清零。
通用单结果入口独立实例化，单独求商不写余数，单独求余不保存商。
有符号输入在无符号域计算幅值，再分别恢复商和余数的符号。
64/128-bit 仍通过一次商计算加 `dividend - quotient * divisor` 重建余数。

单独 `%` 对 512-bit 及以上整数在除数幅值的最高两个 limb 至少一个非零时直接求余。
256-bit 通常使用四 limb 满宽求余核，2/3-limb 除数共用定长求商核后乘回。
AArch64 GCC 的 256-bit 取模在除数幅值的最高两个 limb 至少一个非零时直接使用通用多 limb
求余入口。该组合的 256-bit 有符号入口保留统一幅值转换，512-bit 的正正组合
单独提前返回，以保留带负数时的代码生成。其他非负输入直接使用幅值，省去符号转换。
对于负操作数，先转换幅值，再按被除数恢复余数符号。x86-64 GCC 的 1024-bit
取模在转换幅值后保留乘回求余，因为直接求余会放慢正被除数、负除数的组合。同平台
的通用直接求余内核保留独立调用边界，以维持 GCC 13.3 对同一翻译单元中 512-bit
稠密宽整数乘以正有符号标量的内联。

### 文本与浮点

- 十进制输出按 `10^19` 分块。各平台均通过模板参数把分块除数传入内核，按
  [除法与取模](#除法与取模)中的固定除数路径处理。输出收集余数，从缓冲区末尾
  写入各块，最后一次构造字符串。
- 2/8/16 进制解析直接写入能放进单个 limb 的短输入。更长的输入按单 limb 分块，
  在块内每次校验和打包四位。至多 256-bit 时，移位后拼入下一块。
  更宽的整数按块的位偏移写入目标 limb。
- 十进制解析先读入 1–19 位，再按 19 位块用常量 `10^19` 乘加。累积过程记录已使用的
  limb 上界，跳过尚为零的高 limb。达到目标位宽后丢弃最高进位，并继续校验后续字符。
  其余进制使用运行时选定的分块大小。分块乘加使用独立入口，保留 2/8/16 进制的分派。
- 浮点比较先比较指数，再直接从对应的 limb 提取对齐后的有效位。有效位相等时，
  检查宽整数中被舍弃的低位是否存在非零位。
- 宽整数转浮点时，有效位数不超过目标精度的单 limb 幅值直接转换。需要舍入时使用
  guard/sticky bits，避免逐 limb 累加的二次舍入。

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

固定宽度 helper、强制内联和循环改写都可能改变寄存器分配、内联边界或产生新的调用。
性能优化与重构遵守以下实现准则，采样方法、对比脚本和门禁流程见[基准测试](BENCHMARKS.md)：

- **算法优先与跨微架构 Minimax 选择**：优先做全架构同向受益的算法与拷贝消除优化。
  涉及微架构指令延迟分歧时，在每个同机、同工具链组合内以较快候选为分母计算相对损失，
  优先选择已测组合中最大损失较小的默认实现，再权衡常见负载收益、代码尺寸与复杂度。
- **平台与编译器特化准入门槛**：仅在存在 ISA 硬件原语差异，或主流编译器稳定出现且
  通用代码无法抹平的显著代码生成缺陷时，才引入 `#if` 特化。分支条件必须能实际区分
  受益组合（例如通用 x86-64 宏无法区分 Alder Lake 与 Broadwell），同时验证备用路径
  正确性并补充对应的 codegen contract 探针；证据不足时保留简单通用实现并记录待测组合。
- **区分指令退化与布局漂移**：确认单项退化后必须核对最终链接二进制反汇编。只修复
  真实的指令序列、调用边界、寄存器分配或内存拷贝退化；不针对归一化指令一致、仅因
  地址对齐偏移引起的代码布局抖动修改源码或增加特化。
- **优先级与明牌取舍**：正确性、固定位宽补码回绕、对象大小契约与 C++11 兼容为红线；
  跨平台取舍优先保障主流 Linux x86-64 与 ARM64 现代工具链（核心 128/256-bit 优先于
  512/1024-bit），老编译器仅保正确性与编译兼容。无法避免的显著局部取舍须由维护者
  明确接受，并按[基准测试](BENCHMARKS.md#取舍字段)完整记录受损范围、收益与证据。
