# gint 技术规格

## 1. 设计目标与特性

- 固定宽度：类型的实际位宽严格等于模板参数（无隐式扩展/收缩）
- 易嵌入：单头文件实现，无外部代码生成步骤
- 低开销入口：纯算术翻译单元可包含 `<gint/core.h>`，跳过字符串、stream 与 fmt
  实现的解析；完整接口仍由 `<gint/gint.h>` 提供。
- 高性能：针对常见位宽与路径做了专门优化（如 128/256 位乘除）
- 良好互操作性：与原生整数、`__int128`、浮点数进行双向转换与混合运算（浮点混合算术经截断，见“浮点互操作”）
- 可选的运行时检查：按需开启除零检查
- 标准库集成：提供 `std::numeric_limits` 与 `std::hash` 专用化


## 2. 核心类型与模板参数

- 主类型模板：`gint::integer<Bits, Signed>`
  - `Bits`：位宽，只支持 `64/128/256/512/1024`（否则触发静态断言）。
  - `Signed`：符号标签，取值须为 `signed` 或 `unsigned` 类型名。
- 预定义别名：
  - `gint::Int128`、`gint::UInt128`、`gint::Int256`、`gint::UInt256`。


## 3. 内部表示与数值模型

- 存储：小端 limb 数组，`uint64_t data_[Bits/64]`（低位 limb 在较小索引）。
- 对象大小：严格等于 `Bits/8` 字节，不因内部对齐策略引入尾部填充。
- 有符号表示：二进制补码（two’s complement），最高位为符号位。
- 数值域：按位宽进行模环计算（`std::numeric_limits<integer>::is_modulo == true`）。


## 4. 构造、赋值与常量表达式

- 从整数类型构造：隐式，`constexpr`（C++11）。支持所有内建整数与 `__int128`/`unsigned __int128`。
- 从浮点类型构造：隐式；见 §8 的舍入与截断规则。
- 拷贝/移动构造：`constexpr`/`noexcept`。
- 赋值运算符：C++14 及以上为 `constexpr`（见 `GINT_CONSTEXPR14`）。


## 5. 类型转换（显式）

- 向整数类型转换：显式 `operator T()`，返回低 `sizeof(T)*8` 位；
  - 有符号目标类型：按补码解释低位（与原生窄化一致）。
  - 无符号目标类型：按模 `2^N` 截断。
- 向布尔转换：显式，非零即真。
- 向浮点转换：显式 `operator long double/double/float()`。


## 6. 算术运算

- 支持：`+ - * / %` 及其复合赋值，与同型或原生整数/`__int128`/浮点混合。
- 除法与取模语义：与 C++ 内建整数一致
  - 商向零截断（truncation toward zero）。
  - 余数与被除数同号。
- 同时需要商与余数时可调用 `gint::divmod(dividend, divisor)`；返回值的
  `quotient`、`remainder` 字段分别对应 `/`、`%`。该接口只执行一次宽除法，
  再由 `dividend - quotient * divisor` 精确重建余数。
- 溢出：按模 `2^Bits` 包裹（wraparound）。

测试参考：`tests/arithmetic_basic_test.cpp`、`tests/arithmetic_divmod_test.cpp`。

## 7. 位运算

- 支持：`~ & | ^` 及其复合赋值；可与原生整数/`__int128` 混用。
- 语义：直接作用于补码位模式；对有符号类型上的位运算与内建行为一致。

测试参考：`tests/bitwise_test.cpp`。

## 8. 移位运算

- 左/右移：`<<`, `>>` 及其复合赋值；位移量支持内建整数类型以及 `__int128`/`unsigned __int128`。
- 语义：
  - 位移量为 0：不改变（no-op）。
  - 有符号位移量 < 0：不改变（no-op）。
  - 位移量 >= 位宽：
    - 左移：结果为零。
    - 右移：无符号为零；有符号按算术右移填充（负数为 -1，非负为 0）。
- 右移：
  - `Signed = signed` 时为算术右移（符号扩展）。
  - `Signed = unsigned` 时为逻辑右移（零扩展）。
- 左移丢弃高位，按模 `2^Bits` 包裹。

测试参考：`tests/shift_test.cpp`。

## 9. 比较运算

- 支持：`== != < > <= >=`；与原生整数/`__int128`/浮点混合比较。
- 有符号比较：先依据最高位判符号，再逐 limb 比较。
- 与浮点比较：采用基于 `frexp` 的比较算法（不将大整数整体转换为 `long double`）。
  - 将浮点拆解为 `m * 2^e`（0.5 ≤ m < 1），比较最高位位置与尾数有效位（`digits`）对齐后的有效位；
  - 处理低位截断与小数尾部以决定严格大小关系；
  - `NaN` 与任意值比较均返回 `false`；与 `±inf` 的比较遵循有界整数的自然序关系。


## 10. 字符串、流与格式化

- `gint::to_string()`：十进制字符串表示；负数以 `-` 前缀。
- `gint::from_string()` 与显式字符串构造：按给定位宽解析字符串；默认自动识别十进制、`0x`/`0X` 十六进制、`0b`/`0B` 二进制和前导 `0` 八进制，也可显式传入 2..36 的进制。结果按固定宽度累积，超出位宽时按模 `2^Bits` 包裹；无效输入抛出 `std::invalid_argument`。
- `operator<<`：默认输出十进制表示，并遵循 `std::hex`、`std::oct`、`std::showbase`、`std::showpos`、`std::uppercase`、宽度、填充和对齐等 stream 格式标志。
- `fmt` 支持：定义 `GINT_ENABLE_FMT` 宏后，提供 `fmt::formatter<gint::integer<...>>`，支持十进制、十六进制、八进制、二进制、字符展示、符号、替代格式、宽度、动态宽度和本地化分组等常用整数格式项。

测试参考：`tests/stream_test.cpp`、`tests/fmt_support_test.cpp`。

## 11. 与原生类型的互操作

- 整数：所有内建整数与 `__int128`/`unsigned __int128` 可双向转换与混合运算。
  - 当有符号 `integer` 与当前位宽无法表示的无符号内建值混合比较、除法或取模时，内部提升到可表示双方的更宽有符号临时类型，再按固定宽度结果截回。
  - 其它超出当前 `integer<Bits, Signed>` 可表示范围的整型混合场景，仍先按固定宽度构造/截断为 `integer`，再执行对应运算。
- 浮点：`float/double/long double` 可混合算术；混合算术首先将浮点数按“向零截断”转换为整数，再以固定宽度整数语义进行计算（超范围时按模 `2^Bits` 截断）。
  - 比较运算采用 §9 的 `frexp`‑对齐算法，不通过将宽整数整体转换为 `long double` 做出决定。
- 条件表达式：可与字面量/原生值混用（见测试）。

测试参考：`tests/conversion_test.cpp`。

## 12. 数值极值、`numeric_limits` 与 `std::hash`

- `digits = Bits - (is_signed ? 1 : 0)`；`radix = 2`；`is_exact = true`；`is_modulo = true`；`round_style = round_toward_zero`。
- `min()`：无符号为 0；有符号为最小负值（仅最高位为 1）。
- `max()`：无符号为全 1；有符号为按补码的最大正值。
- `std::hash<gint::integer<Bits, Signed>>`：按所有 limb 组合哈希值，满足默认构造与 `noexcept` 调用要求。

测试参考：`tests/numeric_limits_test.cpp`、`tests/hash_test.cpp`。

## 13. 异常与错误语义

- 除零检查（可选）：
  - 宏 `GINT_ENABLE_DIVZERO_CHECKS` 未定义（默认）：不抛异常；除零返回 0，模零返回被除数本身。该行为用于避免 unchecked 路径触发底层除法 UB。
  - 宏已定义（需在包含头文件前定义）：当前翻译单元中的除零或模零调用点抛出 `std::domain_error`。
  - 除零、异常、compiler-tuned path 与 AArch64 asm 策略共同编码进 inline
    namespace。采用不同策略的翻译单元因此生成不同的 ABI 类型和符号，不会再由
    COMDAT/weak inline 的链接顺序静默选择另一套实现。
  - 对外暴露 `gint::integer` 的同一 C++ 接口仍应使用一致的 target 级配置；若配置
    不一致，类型/符号会明确隔离并在跨边界使用时产生链接或类型不匹配，而不是改变
    运行语义。
- 其它操作（溢出、下溢、移位越界等）：不抛异常，按模或规则处理（见 §6、§8）。

测试参考：`tests/exceptions_overflow_test.cpp`。

## 14. 边界与特殊情况（规范性说明）

- 从负值构造无符号类型：结果按模 `2^Bits`（与 C++ 内建一致）。
- 无符号下溢/溢出：按模 `2^Bits` 包裹。
- 右移：有符号为算术右移（符号扩展），无符号为逻辑右移（零扩展）。
- 位移量为负：结果保持不变（不触发 UB）。
- 向窄整数转换：保留低位，符合补码规则（与内建一致）。
- 浮点参与算术：混合算术先向零截断浮点数，再进行整数算术；比较采用 `frexp` 对齐算法；`NaN/Inf` 行为如下：
  - 与 `NaN` 的任意比较均为 `false`；
  - 与 `±inf` 的相等比较为 `false`；有界整数满足 `x < +inf`、`x <= +inf`、`x > -inf`、`x >= -inf`；
  - 整数 ÷ `NaN` 或 整数 % `NaN` 抛出 `std::domain_error`；
  - 整数 ÷ `±inf` 结果为 0；
  - `±inf` ÷ 整数、`±inf` % 整数抛出 `std::domain_error`；
  - 整数 % `±inf` 结果为该整数本身；
  - 当 `|f| < 1` 时，以浮点作除数或模时将被截断为 0；若开启除零检查（见 §13），视为除零并抛出异常；否则遵循 unchecked 除零/模零规则。

测试参考：`tests/shift_test.cpp`、`tests/conversion_test.cpp`。

## 15. 性能与算法简述

- 加/减（add/sub）：逐 limb 相加/相减并传播进位/借位；对 256 位提供显式展开以减小循环开销。
- 乘法（mul）：
  - 128 位：基于 `__int128` 的学校乘法。
  - 256 位：Comba 风格（对角线累加 + 显式进位），对 4‑limb 做明确展开以减少循环/索引开销，便于寄存器分配与流水并行。
  - 512/1024 位：一般 O(n^2) 学校乘法。
- 除法/取模（div/mod）：
  - 小除数（单 limb）`div_mod_small`：
    - 32 位除数：基于 2^32 的倒数乘法路径（Barrett 风格）：对每个 64 位 word 以 hi/lo 两步进行估商，使用 `rinv = floor((2^64−1)/d32)`，通过 `high64(T * rinv)` 估算，再以至多 +1 进行校正；循环内无硬件除法。
    - 64 位除数：采用 128/64 的倒数乘法估商：`inv = floor((2^128−1)/div)`，`q_est = high128(num * inv)`；以一次“乘回求余”进行校正（若 `rem >= div` 则 `++q`，`rem -= div`）。在 256 位上进行顺序展开以减少循环/索引与分支开销。
    - 2 的幂：直接转化为右移得到商、按掩码取余。
  - 2‑limb（128 位）快速路径：`div_128` 基于 `__int128` 进行 128/128→128 的除法，返回 128 位商。
  - 多 limb 除数（一般情况）Knuth 算法 D：
    - 通用实现 `div_large`：每个估商步骤只做一次 128/64 除法并以“乘回求余”代替取模，减少除法使用；规范化左移通过内部工具函数 `lshift_limbs_to()` 复用，消除重复代码。
    - 两肢专用 `div_large_2`：定长内核，基于最高 limb 估商，校正最多两次；乘回复用 `qhat*v1 = numerator - rhat` 消去一处乘法。
    - 三肢专用 `div_large_3`：定长内核，复用 `qhat*v[2] = numerator - rhat` 优化；保持 Algorithm D 的估商修正与借位补偿流程。
    - 四肢专用 `div_large_4`：针对 256 位满宽除数（`divisor_limbs == 4`）且商最多为单 limb 的场景，定长展开规范化、估商修正和乘回减法；`operator/` 使用该内核。x86_64/GCC 的无符号满宽 `operator%` 走直接求余的 `rem_large_4`，避免先生成商再乘回。
  - 公共 `divmod`：复用一次 `/` 的商，并以乘回相减生成余数；调用方同时需要两者时
    避免独立执行 `/` 与 `%` 的重复宽除法。
- 字符串转换（to_string）：
  - 采用十进制 10^19 分块法：每次除以 10^19 收集一段 19 位十进制块（低位到高位），最后一次性拼接输出（首块不补零，其余块左零填充）。
  - 相较逐位除以 10 并在字符串头部插入，避免 O(n^2) 内存移动，整体近似 O(n)。
- 字符串解析（from_string）：
  - `2/8/16` 进制直接按 64-bit digit chunk 做位打包，超宽输入自然按 `2^Bits`
    截断，不再经过通用乘加循环。
  - `const char *` 与 `std::string` 共用 pointer-range parser；C 字符串入口不再先构造
    临时 `std::string`。


## 16. 浮点互操作

为避免在 `Bits > 64` 时将大整数转换为 `long double` 带来的精度丢失与非直观行为（例如比较/运算结果随平台 `long double` 精度不同而变化），浮点互操作按如下规则实现（参考 Boost.Multiprecision 做法）：

- 算术（`+ - * / %`）
  - 当参与方为“宽整数 × 浮点”时，先对浮点操作数执行“向零截断”为整数，再进行整数算术。
  - 示例：`u256(1000) / 3.5` 等价于 `u256(1000) / u256(3)`，结果为 `333`。
  - 对于除法/取模：若浮点数截断后为 `0`（即 `|f| < 1`），则视为除以/模零；在启用 `GINT_ENABLE_DIVZERO_CHECKS` 时抛出 `std::domain_error`。

- 比较（`== != < <= > >=`）
  - 不再将宽整数转换为 `long double`。改用基于 `frexp` 的指数/尾数比较：
    1) 处理 `NaN/Inf/0` 和符号；
    2) 比较整数最高位位置与浮点的二进制指数；
    3) 若最高位相同，则提取整数最高的 `p = numeric_limits<F>::digits` 个二进制位，与浮点尾数的 `p` 位进行比较；必要时利用整数余位判定大小或相等。
  - 该算法避免因浮点精度不足导致的“超大整数被截断到近似值”而产生的非直观比较结果。

- 转换（`float/double/long double` ↔ 宽整数）
  - 浮点→宽整数：有限值向零截断（小数部分丢弃）；超出位宽的高位被截断（按 2^W 模意义）。
  - 非有限浮点→宽整数：`NaN` 映射为 0；`+inf` 饱和为最大可表示值；`-inf` 对有符号类型饱和为最小可表示值，对无符号类型映射为 0。
  - 宽整数→浮点：提取目标格式的最高有效位，并用 guard/sticky bits 按当前
    `fegetround()` 模式执行一次舍入；常见的 53-bit、64-bit 与 113-bit significand
    均由同一内核覆盖，避免逐 limb 浮点累加造成的双重舍入。


## 17. 平台与兼容性

- 依赖：GCC/Clang 扩展（如 `__int128`、`__builtin_clzll` 等）。
- 正式支持 GCC、LLVM Clang（GNU-compatible driver）与 AppleClang；源码 CMake
  集成和安装后的 `find_package` 都会拒绝其他 compiler frontend。
- MSVC 与 `clang-cl` 明确不支持；直接包含头文件时也会给出针对性编译错误。
- 语言级别：C++11 起；部分 `constexpr` 能力在 C++14 提升。
- 单头文件公开 `GINT_VERSION_MAJOR`、`GINT_VERSION_MINOR`、
  `GINT_VERSION_PATCH` 与 `GINT_VERSION`，不依赖生成头。
- 正式 OS、架构与已实测编译器 baseline 见 `docs/SUPPORT.md`。


## 18. 示例

```cpp
#include <gint/gint.h>
#include <limits>

using U256 = gint::integer<256, unsigned>;
using S256 = gint::integer<256, signed>;

void demo() {
    U256 a = 1;                 // 隐式从内建整数构造
    U256 b = (a << 128) | 42;   // 固定位宽的移位与按位或

    auto low64 = static_cast<std::uint64_t>(b);  // 显式向内建窄化

    S256 s = -123;
    auto q = s / 7;             // 商向零截断
    auto r = s % 7;             // 余数同号（-4）

    // 可选：启用除零检查
    // #define GINT_ENABLE_DIVZERO_CHECKS
    // #include <gint/gint.h>
}
```

## 19. 配置开关

- `GINT_ENABLE_DIVZERO_CHECKS`：启用除零/模零抛出 `std::domain_error` 的编译期策略；应在包含 `gint/gint.h` 前定义，并在同一 target 内统一配置。
- `GINT_ENABLE_FMT`：启用 `fmt` 的格式化适配（需要链接/可用 `fmt`）。
- `GINT_GCC_TUNED_PATHS` / `GINT_CLANG_TUNED_PATHS`：编译器相关优化路径选择；默认按当前编译器自动选择，两个宏互斥。普通使用不需要手动配置。
- `GINT_ENABLE_AARCH64_LIMB_ASM`：控制 AArch64 limb 级内联汇编路径；默认在 `__aarch64__` 下开启、其它平台关闭，可用 `-DGINT_ENABLE_AARCH64_LIMB_ASM=0/1` 覆盖。
