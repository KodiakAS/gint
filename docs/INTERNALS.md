# 实现说明

本文解释当前内部实现，帮助维护者评估 correctness、代码生成和性能影响。
这些细节不是独立的公共兼容承诺；公共行为以[技术规格](TECH_SPEC.md)为准。

## 源码组织

人工维护的实现是以 `src/gint/gint.hpp` 为入口的普通 C++ `.hpp` 依赖图。
`scripts/generate-amalgamation.py` 递归展开仓库内 include，确定性生成并核对已提交的
`include/gint/gint.h`；普通 consumer 不运行生成器，也不依赖 Python。每个内部头
都应保持可由 clangd、IDE 和静态分析器直接解析。这个 source graph + committed
distribution header 模型与 [nlohmann/json 的 amalgamation
工具](https://github.com/nlohmann/json/tree/develop/tools/amalgamate)等成熟 header-only
项目一致。

测试配置会构建 `gint_internal_header_graph`，因此 `compile_commands.json` 包含一个
以内部图为入口的真实 C++11 translation unit；fmt、checked 和
`-fno-exceptions` 另有独立配置 translation unit。它们为语言服务提供 canonical
context；clangd 仍会以 heuristic 选择 header command，因此这里不承诺每个编辑器
会话必然选中同一个 target。

`include/gint/gint.h` 是唯一公开头文件，始终包含字符串和 stream 接口；fmt 仍由
`GINT_ENABLE_FMT` 控制。内部按职责拆分，为维护者提供清晰的源码边界。

### 模块角色与依赖

生成器 manifest 将每个内部头归入角色并强制依赖方向；新增、删除或漏分类的
`.hpp` 都会生成失败。角色约束不复制源码中的精确 include edge 清单：

| 文件 | 职责 | 直接内部依赖 |
| --- | --- | --- |
| [`gint.hpp`](../src/gint/gint.hpp) | 显式组合完整接口，最后清理私有宏 | integer、standard、string_stream、fmt、cleanup |
| [`prelude.hpp`](../src/gint/prelude.hpp) | 编译器要求、版本和公共标准库头 | 无 |
| [`configuration.hpp`](../src/gint/configuration.hpp) | 功能策略、编译器属性和配置 namespace | prelude |
| [`limb_ops.hpp`](../src/gint/limb_ops.hpp) | limb 算术、比较、移位、浮点 magnitude 内核及架构特化 | configuration |
| [`integer.hpp`](../src/gint/integer.hpp) | 整数声明、存储、公共重载、类型/符号策略和内核调用 | limb_ops |
| [`standard.hpp`](../src/gint/standard.hpp) | `std::numeric_limits`、`std::hash` 特化 | integer |
| [`string_stream.hpp`](../src/gint/string_stream.hpp) | 字符串解析、进制转换和 stream 输出 | integer |
| [`fmt.hpp`](../src/gint/fmt.hpp) | 可选 fmt 适配，复用文本转换函数 | string_stream |
| [`cleanup.hpp`](../src/gint/cleanup.hpp) | 清理私有实现宏 | 无 |

生成器将 prelude、configuration、limb_ops、integer、standard 依次归入 core 角色。
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

### 代码归属与阅读路径

模块按所处理的数据和职责划分，不按函数长短或是否使用平台 intrinsic 划分：

- `limb_ops.hpp` 的函数接收原生整数或 limb 数组，不访问 `integer::data_`，也不
  决定对象的 signed 语义。加减乘、位运算和基础检查使用自由函数；移位、浮点
  magnitude 转换和除法分别集中在无状态的 `limb_shift<Limbs>`、
  `limb_float<Limbs>`、`limb_division<Limbs>` 中。这些类型只组织共享编译期 limb
  数的函数，不持有存储、不继承整数类，也不通过 friend 或访问器依赖上层。
  scalar、constexpr/runtime 分派和架构特化与各自算法放在一起。
- `integer.hpp` 负责对象布局、构造、公共重载、signed/unsigned 规则和转换。
  前置声明、类型别名、位宽约束、提升边界、C++11 序列工具和 traits 在文件开头
  集中分区维护；它们服务于后面的整数实现，没有独立包含的需求，不另拆小文件。
  文本接口的声明也放在类之前，定义位于 `string_stream.hpp`。
  运算符保留类型提升、符号处理、越界移位和除零策略；具体计算调用下层数组
  接口。跨位宽复制、符号扩展和特殊值构造仍属于对象表示和转换策略。

阅读任意算术操作时，从 `integer.hpp` 的运算符进入 `limb_ops.hpp` 的对应内核。
例如除法先在上层处理符号、零和特殊值，再调用下层的单 limb、定长 divisor 或
通用 Knuth 内核；移位由上层检查位数并选择扩展 fill，下层接收 limb/bit 偏移
并执行数组移动。
浮点转换由上层决定符号以及 NaN/无穷大的策略，下层提取数位、比较有效位并按
guard/sticky bits 舍入。有效位比较的高位扩展模式由上层在编译期选择，避免把
额外的运行期 fill 参数带过库函数调用。文本解析的 `mul_add_limb` 成员也只是
数组内核的适配。

数组接口使用低索引为低位的固定长度存储。除法的输出必须与输入分离，并由内核
完整初始化；原地移位和位运算使用明确的 assign 接口，独立输出的移位不允许
与输入重叠。浮点有限值数位提取要求上层先清零目标数组。调用约束标在对应内核
分区，直接数组测试验证输出初始化、输入保持和扩展 fill。整数类的原有对象布局
不变，不引入额外存储包装或动态分派。

原来需要保留的 noinline 内核边界放在下层，整数到数组的除法适配强制内联，避免
增加额外调用层。提取保持算法及平台分派，但数组接口可能改变复制、别名分析和
寄存器分配，因此必须通过 codegen contract 与同环境 benchmark 验证，不能仅凭
“只是拆分”推导性能不变。

两 limb 原地右移显式保留 low/high 局部值，避免拆分后的小数组索引产生栈往返。
浮点转换内核强制内联到原有对象转换入口；有效位比较的临时 magnitude 使用
函数内的纯数据 aggregate，保持整体复制形式，且不依赖整数类。
AArch64 GCC 的小除法内核和符号适配使用 `GINT_SMALL_DIV_INLINE` 保持原有内联
路径，避免共享内核后编译器新增函数调用；其他目标保留原有调用策略。
小除法在扫描高位零 limb 时同步写零对应商位，其余商位由计算路径写入；保持
完整初始化，同时避免只取余数时保留整块商缓冲区清零。

阅读文本功能时，从 `integer.hpp` 开头看声明，从 `string_stream.hpp` 看实现；
文本适配通过限定的 friend 访问私有存储，十进制输出复用小除法，解析复用
`mul_add_limb`。`fmt.hpp` 在此之上复用文本转换，不另建一套整数算术。

共享标准库头仍集中在 `prelude.hpp`，以保持生成头的包含顺序。保留的 include
都有直接用途：`array` 用于除法和文本分块；`cfenv`、`cmath` 用于浮点转换；
`cstddef`、`cstdint` 用于长度和 limb 类型；`cstdlib` 用于无异常配置下的
`std::abort`；`cstring` 用于 `std::strlen`；`functional`、`limits` 用于标准库
适配和数值边界；`ios`、`ostream`、`string` 用于文本接口；`stdexcept` 用于
错误报告；`type_traits` 用于模板约束。x86 intrinsic 头只在 x86 条件内包含，
`locale` 和外部 fmt 头只在启用 fmt 时包含。不依靠其他标准库头的传递包含
来删除这些直接依赖。

### 生成器输入与处理契约

生成器处理受限、fail-closed 的 C++ 头文件方言。所有模块必须以唯一、规范的
`#pragma once` 开始。本地 quoted
include 只能出现在顶层无条件上下文；条件/宏/内部 angle include、`#import`、
`#include_next`、`__has_include`、`__has_include_next`、`__has_embed`、
module/import 控制行、块注释、raw string、pragma operator、trigraph、全部六种
digraph token（`<:`、`:>`、`<%`、`%>`、`%:`、`%:%:`）和
非规范续行都会使生成失败。禁止形式按字节保守检查，注释和字符串中的相同拼写也
受限制。危险预处理运算符即使通过宏别名出现也会被拒绝；token
paste 仅允许用于项目定义的配置 namespace 宏。会让源码图和扁平头产生不同值的
`__BASE_FILE__`、`__FILE__`、`__FILE_NAME__`、`__INCLUDE_LEVEL__`、`__LINE__`、
`__TIMESTAMP__` 和 `__builtin_LINE`、`__builtin_COLUMN`、`__builtin_FILE`、
`__builtin_FILE_NAME`、`__builtin_source_location` 同样不属于内部头方言。
除规范的 `#pragma once` 外，只允许 GCC/Clang 的 `diagnostic push/pop` 和
`diagnostic ignored "-W..."`；`system_header` 等依赖文件边界的 pragma 不受支持。
quoted include 必须是非空相对路径且不得
包含空或 `.` 组件；`..` 只能在 `src/gint` 内沿真实存在且非符号链接的目录回退，不得逃出
源树或穿越缺失/符号链接组件。内部 angle include 的识别覆盖 `./gint/...`、重复
分隔符和 `..` 折叠后的等价路径；`gint` include 命名空间的大小写变体均视为内部
引用。检查同时覆盖 `src`、`src/gint`、包含者目录下的候选路径及指向已发现内部头的
物理文件别名。外部 angle include 保留原拼写。include 路径不得含反斜杠、空白或
控制字符，防止分发头残留对内部源树的依赖。
路径按物理文件身份去重和判环，并拒绝 symlink、
hardlink alias 与非精确大小写。所有模块按物理文件身份去重并判环。维护生成头需要 Python 3.5 或更高版本。

处理阶段依次为：

1. 检查原始 LF 字节和 trigraph，按反斜杠换行拼接逻辑行，保留原始字节与起始行号。
2. 对完整逻辑行统一检查禁止形式；指令名只解析一次，产生共享的指令记录。
3. 依据指令记录维护条件栈、解析 include 和校验 pragma。`#if(0)`、
   `#elif!defined(X)`、`#else// ...`、`#endif// ...` 均按指令处理；重复 else、
   else 之后的 elif、非法参数和未闭合条件直接报错。任何条件臂内的本地 include
   都拒绝展开。
4. 按源码清单、路径身份与角色约束展开依赖，核对全图可达性；输出检查使用同一份
   解析规则，禁止遗留内部 include 和内部 pragma once。最后为分发头添加唯一的
   `#pragma once`，使重复包含完整头与原始源码图保持一致。

这套语法层不计算条件表达式，也不实现完整 C++ 宏展开。C++ 语义由编译器门禁验证。
`generator.compiler_equivalence_0/1` 将同一个依赖图和实际生成的独立头编译为普通
CMake targets，使用当前配置的编译器、target、sysroot 和 flags，对照声明、条件分支、
宏清理与运行结果。flat target 没有内部源码 include 路径。Python 测试覆盖拒绝矩阵
和生产 manifest 下的源码图变体，consumer 测试覆盖单头文件独立及重复包含、私有宏清理和完整 IO 接口。

## 数据布局

`integer<Bits, Signed>` 使用 `Bits / 64` 个 `std::uint64_t` limb，低位 limb 位于
较小索引。对象保持 `Bits / 8` 字节大小，有符号值直接使用同一补码位模式。

## 算法结构

### 加减与乘法

- 加减逐 limb 传播进位或借位，常用 256-bit 路径显式展开。
- 128-bit 乘法使用 `__int128`；256-bit 使用定长 Comba 风格累加。
- 512/1024-bit 使用 O(n²) 学校乘法。

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

内部重构不能只用单元测试证明安全：固定宽度 helper、强制内联和循环改写都可能
改变寄存器分配或生成新的调用。hot path 变更应先运行 codegen contract，再按
[基准测试方法](BENCHMARKS.md)进行同环境前后采样。

机器可读的结构预算位于
[`tests/perf/codegen_contract.json`](../tests/perf/codegen_contract.json)；文档不
复制其中阈值，以避免门禁与说明漂移。
