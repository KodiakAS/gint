# gint 技术规格

## 1. 设计目标与特性

- 固定宽度：类型的实际位宽严格等于模板参数（无隐式扩展/收缩）
- 易嵌入：单头文件实现，无外部代码生成步骤
- 高性能：针对常见位宽与路径做了专门优化（如 128/256 位乘除）
- 良好互操作性：与原生整数、`__int128`、浮点数进行双向转换与混合运算
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
  - 右移为逻辑右移（零扩展），无论 `Signed`。
  - 左移丢弃高位，按模 `2^Bits` 包裹。

测试参考：`tests/shift_test.cpp`。

## 9. 比较运算

- 支持：`== != < > <= >=`；与原生整数/`__int128`/浮点混合比较。
- 有符号比较：先依据最高位判符号，再逐 limb 比较。
- 与浮点比较：通过 `long double` 中间值比较。


## 10. 字符串、流与格式化

- `gint::to_string()`：十进制字符串表示；负数以 `-` 前缀。
- `operator<<`：输出十进制表示。
- `fmt` 支持：定义 `GINT_ENABLE_FMT` 宏后，提供 `fmt::formatter<gint::integer<...>>`。

测试参考：`tests/fmt_support_test.cpp`。

## 11. 与原生类型的互操作

- 整数：所有内建整数与 `__int128`/`unsigned __int128` 可双向转换与混合运算。
- 浮点：`float/double/long double` 可混合算术；结果经 `long double` 计算后向零截断至 `integer<...>` 的位宽（超范围时等价于模 `2^Bits` 截断）。
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

测试参考：`tests/exceptions_overflow_test.cpp`、`tests/compile_test.cpp`。

## 14. 边界与特殊情况（规范性说明）

- 从负值构造无符号类型：结果按模 `2^Bits`（与 C++ 内建一致）。
- 无符号下溢/溢出：按模 `2^Bits` 包裹。
- 右移（包括有符号类型）：逻辑右移（零扩展）。
- 位移量为负：结果保持不变（不触发 UB）。
- 向窄整数转换：保留低位，符合补码规则（与内建一致）。
- 浮点参与算术：经 `long double` 计算再向零截断；巨大值可能因精度受限而产生舍入误差或被模截断。

测试参考：`tests/shift_test.cpp`、`tests/conversion_test.cpp`。

## 15. 性能与算法简述

- `add/sub`：逐 limb 相加/相减并传播进位/借位；对 256 位有专门展开版本。
- `mul`：
  - 128 位：基于 `__int128` 的学校乘法。
  - 256 位：组合式（Comba 风格）实现，减少中间存取。
  - 其它位宽：一般 O(n^2) 学校乘法。
- `div/mod`：按 §6 语义在若干快速路径与通用算法间分派：
  - 单 limb 除法/取模快速路径：`div_mod_small`（含有符号/无符号变体）。
  - 128 位路径：`div_128` 使用内建 `__int128`。
  - 最高至 256 位路径：Knuth 除法（`div_large`）。
  - 通用大数：移位-减法（`div_shift_subtract`）。


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
