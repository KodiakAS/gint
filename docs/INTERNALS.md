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
| [`primitives.hpp`](../src/gint/primitives.hpp) | 类型前置声明、traits 和 limb 运算 | configuration |
| [`integer.hpp`](../src/gint/integer.hpp) | 整数类、运算符、转换和私有算术内核 | primitives |
| [`standard.hpp`](../src/gint/standard.hpp) | `std::numeric_limits`、`std::hash` 特化 | integer |
| [`string_stream.hpp`](../src/gint/string_stream.hpp) | 字符串解析、进制转换和 stream 输出 | integer |
| [`fmt.hpp`](../src/gint/fmt.hpp) | 可选 fmt 适配，复用文本转换函数 | string_stream |
| [`cleanup.hpp`](../src/gint/cleanup.hpp) | 清理私有实现宏 | 无 |

生成器将 prelude、configuration、primitives、integer、standard 依次归入 core 角色，
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
