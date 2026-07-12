# 实现说明

本文解释当前内部实现，帮助维护者评估 correctness、代码生成和性能影响。
这些细节不是独立的公共兼容承诺；公共行为以[技术规格](TECH_SPEC.md)为准。

## 数据布局

`integer<Bits, Signed>` 使用 `Bits / 64` 个 `std::uint64_t` limb，低位 limb 位于
较小索引。对象保持 `Bits / 8` 字节大小，有符号值直接使用同一补码位模式。

人工维护的实现是以 `src/gint/gint.hpp` 为入口的普通 C++ `.hpp` 依赖图。
`scripts/generate-amalgamation.py` 递归展开仓库内 include，确定性生成并核对已提交的
`include/gint/gint.h`；普通 consumer 不运行生成器，也不依赖 Python。每个内部头
都应保持可由 clangd、IDE 和静态分析器直接解析。

`include/gint/gint.h` 本身仍是可独立复制的完整产物。`include/gint/core.h` 通过
受控的两阶段 include 模式暴露精简入口，不维护第二份算术实现。

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
