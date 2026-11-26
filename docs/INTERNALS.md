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

- **文件与依赖**：以唯一、规范的 `#pragma once` 开头；本地头必须在顶层无条件地
  用双引号包含；外部 `<...>` 可位于条件块内并按原样保留；禁止宏 include 以及
  用 `<...>` 包含内部头。
- **路径**：所有 include 名称须非空、使用 UTF-8，且不含反斜杠、ASCII 空白或控制字符；
  本地路径还须为不含空组件与 `.` 的相对路径，`..` 只能沿源树内真实存在且非符号
  链接的目录回退。路径按物理文件身份去重和判环，拒绝逃出源树、缺失组件、symlink、
  hardlink alias 和非精确大小写；等价路径、内部命名空间大小写变体和物理别名均
  不能绕过内部 include 检查。
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
- 256-bit 同型减法由 `limb_ops.hpp` 的策略谓词启用 x86-64 GCC 快路径。
  `operator-` 直接检查右操作数的三个高 limb；全为零时调用标量借位内核。
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

下表按除数幅值的有效 limb 数定位内核。适用的零值和 2 的幂捷径均先于通用内核执行。

| 运算 | 位宽与除数形态 | 主要入口与路径 |
| --- | --- | --- |
| 同型 `/`、`%`、`divmod` | 128-bit，任意除数 | `div_or_mod_128` 与 `divmod_128` 共用分派，按幅值选择原生运算、移位/掩码、比较或估商 |
| 同型 `%` | 64-bit，任意除数 | `modulo`，检查除零后直接使用原生无符号余数 |
| 同型 `/` | 64-bit 或 256-bit 及以上，单 limb | `div_mod_small`，正除数可经 `div_by_positive_limb` 进入 |
| 同型 `%` | 256-bit 及以上，单 limb | `modulo` 对正除数经 `rem_by_positive_limb` 进入 `mod_small`，负除数转幅值后进入 `mod_small` |
| 标量 `/`、`%` | 各位宽，适用单 limb 快路径的右操作数 | 标量重载调用 `div_mod_small`、`div_by_positive_limb` 或 `rem_by_positive_limb` |
| 同型 `/` | 256-bit 及以上，多 limb | 按除数长度选 `div_large_2`、`div_large_3`、256-bit 满宽 `div_large_4` 或 `div_large` |
| 同型 `%` | 256-bit，2/3/4-limb | `modulo` 通常对 2/3-limb 求商后乘回，对 4-limb 调 `rem_large_4`。AArch64 GCC 对 3/4-limb 调 `rem_large` |
| 同型 `%` | 512/1024-bit，多 limb | `modulo` 在除数幅值最高两个 limb 至少一个非零时调 `rem_large`，其余求商后乘回（x86-64 GCC 1024-bit 符号转换路径有例外） |
| `divmod` | 256-bit 及以上，有符号和无符号 | 比较无符号幅值，按形态使用移位/掩码、单 limb、定长 2/3/4-limb 或通用双输出内核，再按需恢复符号 |
| `divmod` | 64-bit，有符号和无符号 | 除数非零时计算一次 `/`，用 `dividend - quotient * divisor` 重建余数 |

#### 64-bit 取模与 128-bit 除模

- **64-bit 同型取模**：直接使用原生无符号余数；有符号输入先转幅值，再按被除数恢复余数符号，覆盖 `INT64_MIN % -1`。
- **128-bit 同型除模**：统一在无符号域计算幅值并还原符号（`min / -1` 按固定位宽回绕）；求商时若两数均为负数，直接比较补码位模式提前判商为零。非零除数按幅值分派：
  - 2 的幂走移位和掩码，其余单 limb 除数直接原生除模；
  - 其余 `>= 2^127`：一次比较即得商（0 或 1）与余数；
  - 其余 `[2^126, 2^127)`：取模走原生运算；AArch64 GCC/Clang 的 tuned 求商路径用最多三次减法，其余组合用原生除法；
  - 其余 `[2^64, 2^126)`：规范化后用高 limb 估商，再用余数做一次修正。
    x86-64 复用 `divrem_u128_u64`，规范化除数高 limb 至少为 `2^63`，严格大于
    `dividend >> 1` 的高 limb，故估商可装入一个 limb。

> **128-bit 估商最多偏大 1 的理由**：设被除数为 `n`，舍去的低位数为 `k`，截断除数为 `d0`，原除数为 `d`。`n < d` 时直接返回；进入估商路径时满足 `d <= n < 2^128`、`d >= d0 >= 2^(63+k)`、`d - d0 < 2^k`。当 `k >= 2` 时，估商偏差 `n/d0 - n/d = n(d - d0)/(d0*d) < 2^(2-k) <= 1`；当 `k == 1` 时，`d - d0 <= 1` 且 `d0*d >= 2^128`，偏差同样 `< 1`。因此向下取整后的估商最多偏大 1。先减 1 再乘回可保证乘积不溢出；若所得余数仍 `>= d`，再将商加 1、余数减 `d`。

#### 单 limb 除数

- **入口分派**：256-bit 及以上的同型取模与常规求商均先检测正单 limb 除数。
  - *特例（x86-64 Clang，仅同型 `/`）*：在通用入口处理幅值后再识别单 limb（同型 `%` 仍前置检测正单 limb），以维持较窄稠密除数的除模性能及同一翻译单元内文本解析的内联。
  - *特例（`UInt256` 对 `<= 32-bit` 无符号标量取模）*：软件倒数路径保留除数的窄类型。
- **2 的幂与原生位宽**：128-bit 及以上单 limb 内核将 2 的幂转为移位和掩码；64-bit 与 128-bit 被除数分别走原生 64/64 与 128/64 运算（128/64 同时求商余时用原生商乘回得余数）。
- **非 x86-64（256-bit 及以上）**：使用 32/64-bit 倒数估商；求商与单独求余共用每步估商与一次修正，其中 `mod_small` 内核只读被除数（特例：非 x86-64 的 `Int256 % int64_t` 标量重载仍经 `div_mod_small` 同时求商余后返回余数）。
- **x86-64（256-bit 及以上）**：对运行时单 limb 除数逐 limb 使用 `divq`；十进制分块等编译期固定且未命中 2 的幂捷径的 64-bit 除数（含 1024-bit），在 `div_mod_small_constant` 路径使用编译期倒数。标量取模直接读取被除数，负被除数先转幅值。
  - *`divq` 商宽不溢出理由*：除零与 2 的幂已由前置路径处理，进入循环时除数 `>= 3`；初始余数为 0 且每步余数 `< divisor`，故下一步拼接出的 128-bit 被除数严格 `< divisor * 2^64`，商必定可放入单个 limb。

#### 多 limb 求商

- **入口与 2 的幂**：常规路径先处理符号和幅值，再按除数长度分派，遇到有符号最小值时经 `div_unsigned_path` 在无符号域表示幅值；1024-bit 正多 limb 除数有单独的 2 的幂捷径。
  - *特例（AArch64 GCC `UInt256 /`）*：在调用点经 `div_unsigned_256` 按有效 limb 数分派，多 limb 的 2 的幂用已知范围内的右移求商。
- **定长热点内核（2/3-limb 与 256-bit 满宽）**：
  - 2-limb 除数规范化高 limb 等于 `2^63` 时直接移位估商，省去倒数计算；
  - x86-64 上，3-limb 除数估商在分子最高 limb 小于除数最高 limb 时同样使用 `divq`；
  - 256-bit 满宽求商和求余共用规范化、估商与乘减链：规范化最多左移 1 位时估商不超过 3，直接比较与减法即可；求余时修正后的规范化余数严格 `< 2^256`，低四个 limb 反移位即得最终余数。
- **通用多 limb 内核**：单结果路径遇到等长幅值时，用最高几个 limb 估算单 limb 商，再用乘回比较或乘减加回修正；其余通用路径使用规范化 Knuth Algorithm D，通过模板参数分别实例化只生成商、只生成余数、同时生成商和余数的内核。
  - *特例（x86-64 Clang）*：通用求商入口继续走 Algorithm D，以维持较窄稠密除数的除模性能及宽八进制解析的内联。

#### 多 limb 取模

- **主路径分派**：对 512-bit 及以上和 AArch64 GCC 的 256-bit，经 `rem_unsigned_magnitude_with_large_direct` 检查除数幅值的最高两个 limb；至少一个非零时先检测 2 的幂，其余命中项走 `rem_large`。回到 `rem_unsigned_magnitude` 时先检测 2 的幂，再对 256-bit 满宽除数走四 limb 满宽求余核，其他多 limb 除数求商后乘回。
- **符号与幅值**：非负输入通常直接使用原值，负操作数先转幅值再按被除数恢复余数符号。
- **编译器特例**：
  - *AArch64 GCC*：256-bit 有符号入口在正单 limb 捷径后保留统一幅值转换；512-bit 非负组合单独提前返回，以维持带负数时的代码生成。
  - *x86-64 GCC*：1024-bit 取模在转换幅值后保留乘回求余（直接求余会放慢“正被除数、负除数”组合）；通用直接求余内核保留独立调用边界，以维持 GCC 13.3 对同一翻译单元内 512-bit 稠密宽整数乘正有符号标量的内联。

#### 同时求商与余数

`divmod` 的公开入口使用普通 `inline`。256-bit 及以上统一在无符号内核中
计算商和余数。有符号输入先在无符号域计算幅值，随后由两操作数的符号决定商的
符号，余数按被除数的符号恢复。

- 256-bit 的最高 limb 已能判定被除数较小时直接返回。其余情况先用除数的
  有效 limb 数判零，再直接处理被除数小于或等于除数的情况。
- 256-bit 复用定宽的 2 的幂判定。更宽的整数由单 limb 内核处理单 limb 除数，
  多 limb 除数仅在最高有效 limb 为 2 的幂时检查其余 limb 是否全零。2 的幂
  通过移位求商、掩码求余。
- 除数为 2/3-limb 或 256-bit 满宽时，复用定长内核，按模板参数同时输出余数。
  超过 256-bit 且除数至少有四个有效 limb 时，等长幅值只用最高几个 limb 估商，
  通过一次乘减及至多一次加回修正商和余数。短于对象位宽的等长幅值保留原本为零
  的高 limb。
- 其余通用双输出路径从规范化后的被除数提取余数，并将余数未使用的高 limb
  清零。通用单结果入口独立实例化，单独求商只写商，单独求余只写余数。

128-bit 按编译期输出模式与单结果入口共用分派和估商校正内核。常规双 limb
估商直接复用同一次试乘得到的余数。单 limb 非幂除数及 `[2^126, 2^127)` 内的
非幂除数用商乘回求余。64-bit 在除数非零时用 `dividend - quotient * divisor`
重建余数。

### 文本与浮点

- 十进制输出按 `10^19` 分块。各平台均通过模板参数把分块除数传入内核，按
  [除法与取模](#除法与取模)中的固定除数路径处理。输出收集余数，从缓冲区末尾
  写入各块，最后一次构造字符串。
- 2/8/16 进制解析将能放进单个 limb 的短输入直接写入低 limb。更长的输入按单 limb
  分块，在块内每次校验和打包四位，块尾不足四位时逐位处理。至多 256-bit 时，
  移位后拼入下一块。
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
