# gint 技术规格

## 1. 设计目标与特性

- 固定宽度：类型的实际位宽严格等于模板参数（无隐式扩展/收缩）
- 易嵌入：单头文件实现，无外部代码生成步骤
- 高性能：针对常见位宽与路径做了专门优化（如 128/256 位乘除）
- 良好互操作性：与原生整数、`__int128`、浮点数进行双向转换与混合运算（浮点混合算术经截断，见“浮点互操作”）
- 可选的运行时检查：按需开启除零检查


## 2. 核心类型与模板参数

- 主类型模板：`gint::integer<Bits, Signed>`
  - `Bits`：位宽，必须是 64 的整数倍（否则触发静态断言）。
  - `Signed`：符号标签，取值须为 `signed` 或 `unsigned` 类型名。
- 预定义别名：
  - `gint::Int128`、`gint::UInt128`、`gint::Int256`、`gint::UInt256`。


## 3. 内部表示与数值模型

- 存储：小端 limb 数组，`uint64_t data_[Bits/64]`（低位 limb 在较小索引）。
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
- 溢出：按模 `2^Bits` 包裹（wraparound）。

测试参考：`tests/arithmetic_basic_test.cpp`、`tests/arithmetic_divmod_test.cpp`。

## 7. 位运算

- 支持：`~ & | ^` 及其复合赋值；可与原生整数/`__int128` 混用。
- 语义：直接作用于补码位模式；对有符号类型上的位运算与内建行为一致。

测试参考：`tests/bitwise_test.cpp`。

## 8. 移位运算

- 左/右移：`<<`, `>>` 及其复合赋值，位移量类型为 `int`。
- 语义：
  - 位移量 <= 0：不改变（no-op）。
  - 位移量 >= 位宽：结果为零。
- 右移：
  - `Signed = signed` 时为算术右移（符号扩展）。
  - `Signed = unsigned` 时为逻辑右移（零扩展）。
- 位移量为负：保持不变（no-op）。
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
- `operator<<`：输出十进制表示。
- `fmt` 支持：定义 `GINT_ENABLE_FMT` 宏后，提供 `fmt::formatter<gint::integer<...>>`。

测试参考：`tests/fmt_support_test.cpp`。

## 11. 与原生类型的互操作

- 整数：所有内建整数与 `__int128`/`unsigned __int128` 可双向转换与混合运算。
- 浮点：`float/double/long double` 可混合算术；混合算术首先将浮点数按“向零截断”转换为整数，再以固定宽度整数语义进行计算（超范围时按模 `2^Bits` 截断）。
  - 比较运算采用 §9 的 `frexp`‑对齐算法，不通过将宽整数整体转换为 `long double` 做出决定。
- 条件表达式：可与字面量/原生值混用（见测试）。

测试参考：`tests/conversion_test.cpp`。

## 12. 数值极值与 `numeric_limits` 专用化

- `digits = Bits - (is_signed ? 1 : 0)`；`radix = 2`；`is_exact = true`；`is_modulo = true`；`round_style = round_toward_zero`。
- `min()`：无符号为 0；有符号为最小负值（仅最高位为 1）。
- `max()`：无符号为全 1；有符号为按补码的最大正值。

测试参考：`tests/numeric_limits_test.cpp`。

## 13. 异常与错误语义

- 除零检查（可选）：
  - 宏 `GINT_ENABLE_DIVZERO_CHECKS` 未定义（默认）：不做检查；若执行到除零/模零，行为未定义（与内建整数一致）。
  - 宏已定义（需在包含头文件前定义）：除零或模零抛出 `std::domain_error`。
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
   - `±inf` ÷ 整数、`±inf` % 整数 抛出 `std::domain_error`；
   - 整数 % `±inf` 结果为该整数本身；
   - 当 `|f| < 1` 时，以浮点作除数或模时将被截断为 0；若开启除零检查（见 §13），视为除零并抛出异常。

测试参考：`tests/shift_test.cpp`、`tests/conversion_test.cpp`。

## 15. 性能与算法简述

- 加/减（add/sub）：逐 limb 相加/相减并传播进位/借位；对 256 位提供显式展开以减小循环开销。
- 乘法（mul）：
  - 128 位：基于 `__int128` 的学校乘法。
  - 256 位：Comba 风格（对角线累加 + 显式进位），对 4‑limb 做明确展开以减少循环/索引开销，便于寄存器分配与流水并行。
  - 其它位宽：一般 O(n^2) 学校乘法。
- 除法/取模（div/mod）：
  - 单 limb 除数：`div_mod_small`（含有符号/无符号变体）。
    - 32 位除数：基于 2^32 基数的长除法快速路径（每 limb 两次 32 位除法），避免循环内 128 位除法；
    - 64 位除数：采用“倒数乘法”估算商（Granlund‑Montgomery 风格），`q_est = high128(num * inv)`（`inv = floor((2^128−1)/div)`），最多一次修正（若 `num - q_est*div >= div` 则 `++q`）；在 256 位上进行完全展开以降低循环/索引开销；
  - 幂为 2 的除数：转化为右移。
  - 2‑limb（128 位）路径：`div_128` 基于内建 `__int128`。
  - 多 limb 除数（一般情况）：统一使用 Knuth 算法 D（`div_large`）。
    - 该策略减少路径分叉，并显著优于移位‑减法法，尤其是在“被除数与除数位数接近”的场景。
  - 仍保留移位‑减法（`div_shift_subtract`）作为后备，但常规路径不再使用。
- 字符串转换（to_string）：
  - 采用十进制 10^9 分块法：每次除以 10^9 收集一段 9 位十进制块（低位到高位），最后一次性拼接输出（首块不补零，其余块左零填充）。
  - 相较逐位除以 10 并在字符串头部插入，避免 O(n^2) 内存移动，整体近似 O(n)。
 

## 19. 浮点互操作

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
  - 浮点→宽整数：向零截断（小数部分丢弃）；超出位宽的高位被截断（按 2^W 模意义）。
  - 宽整数→浮点：通过逐 limb 累加计算目标浮点值（可能受目标浮点精度限制）。


## 16. 平台与兼容性

- 依赖：GCC/Clang 扩展（如 `__int128`、`__builtin_clzll` 等）。
- 已在 Clang/AppleClang/GCC 上验证。MSVC 对 `__int128` 支持缺失，暂不保证可用性。
- 语言级别：C++11 起；部分 `constexpr` 能力在 C++14 提升。


## 17. 示例

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

## 18. 配置开关

- `GINT_ENABLE_DIVZERO_CHECKS`：启用除零/模零抛出 `std::domain_error` 的运行时检查。
- `GINT_ENABLE_FMT`：启用 `fmt` 的格式化适配（需要链接/可用 `fmt`）。
