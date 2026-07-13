# 实现说明

本文解释当前内部实现，帮助维护者评估 correctness、代码生成和性能影响。
这些细节不是独立的公共兼容承诺；公共行为以[技术规格](TECH_SPEC.md)为准。

## 数据布局

`integer<Bits, Signed>` 使用 `Bits / 64` 个 `std::uint64_t` limb，低位 limb 位于
较小索引。对象保持 `Bits / 8` 字节大小，有符号值直接使用同一补码位模式。

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

`include/gint/gint.h` 本身仍是可独立复制的完整产物。`include/gint/core.h` 通过
受控的两阶段 include 模式暴露精简入口，不维护第二份算术实现。

### 模块角色与 definition pass

生成器内的 manifest 将每个内部头归入角色并强制依赖方向；新增、删除或漏分类的
`.hpp` 都会生成失败。角色约束允许同层继续拆分，但不复制一份容易漂移的精确
include edge 清单：

| 角色 | 成员（同角色按顺序递增） | 可依赖角色 |
| --- | --- | --- |
| core | `prelude.hpp`、`configuration_pass.hpp`、`primitives.hpp`、`integer.hpp`、`standard.hpp` | 较早的 core |
| core entry | `core.hpp` | core、cleanup |
| IO | `io_prelude.hpp`、`string_stream.hpp`、`fmt.hpp` | core、较早的 IO |
| IO entry | `io.hpp` | core entry、IO、cleanup |
| cleanup | `cleanup_pass.hpp` | 无 |
| distribution | `gint.hpp` | IO entry |

core 与 IO 是两个相同生命周期的 definition pass。入口先建立
`*_PASS_IN_PROGRESS`，`configuration_pass.hpp` 从唯一规则源建立 pass-local 宏，
完成本层定义后由 `cleanup_pass.hpp` 收口。这样无论直接使用内部源码图，还是先包含
公开 `core.h` 再升级到 `gint.h`，每一遍的 begin/end 与宏清理边界都一致。

这两个 lifecycle fragment 故意可重复包含，但仍使用 `.hpp`，以保留语言服务和独立
语法检查能力；它们不是可继续承载普通声明/定义的模块。作为角色规则的窄化例外，
生成器还要求每个可能成为 definition pass 中首个未被 `#pragma once` 跳过、且需要
pass-local 环境的 frontier 模块直接声明 lifecycle 依赖：
`configuration_pass.hpp` 只由 `primitives.hpp`、`string_stream.hpp`、`fmt.hpp` 各直接
包含一次。这样后续模块不会依赖较早模块的间接 include 时序副作用。
`cleanup_pass.hpp` 只由 `core.hpp`、`io.hpp` 各直接包含一次，且 cleanup 必须是两个
入口的最后一条有效语句。

生成器将内部源视为受限、fail-closed 的 C++ 头文件方言，而不是尝试实现完整
preprocessor：普通模块必须以唯一、规范的 `#pragma once` 开始；只有 manifest 中
两个 definition-pass fragment 可改为以唯一、规范的
`// GINT_REENTRANT_DEFINITION_PASS` 开始，并在每个 include site 重放。本地 quoted
include 只能出现在顶层无条件上下文；条件/宏/内部 angle include、`#import`、
`#include_next`、`__has_include`、`__has_include_next`、`__has_embed`、
module/import 控制行、块注释、raw string、pragma operator、trigraph、digraph 和
非规范续行都会使生成失败。quoted include 必须是非空相对路径且不得包含空或
`.` 组件；`..` 只能在 `src/gint` 内沿真实存在且非符号链接的目录回退，不得逃出
源树或穿越缺失/符号链接组件。路径按物理文件身份去重和判环，并拒绝 symlink、
hardlink alias 与非精确大小写。普通模块按物理文件身份去重，fragment 保持判环但
按 include site 展开。维护生成头需要 Python 3.5 或更高版本。

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
