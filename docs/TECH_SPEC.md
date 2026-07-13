# 技术规格

本文定义 gint 公共 API 的规范性语义。集成方式见[集成指南](INTEGRATION.md)，
内部算法见[实现说明](INTERNALS.md)，正式平台边界见[支持策略](SUPPORT.md)。

## 1. 类型与数值模型

主类型为 `gint::integer<Bits, Signed>`：

- `Bits` 只支持 `64/128/256/512/1024`；模板参数就是对象的实际位宽。
- `Signed` 使用 `signed` 或 `unsigned`。
- 有符号值采用二进制补码；所有算术结果都截断到 `Bits` 位。
- 数值溢出和下溢按模 `2^Bits` 回绕。
- 常用别名为 `Int128`、`UInt128`、`Int256`、`UInt256`。

对象大小严格等于 `Bits / 8`。内部 limb 布局不是独立的序列化格式；需要稳定
持久化时应显式转换为约定的字节序或文本。

## 2. 构造与转换

### 整数

- 内建整数、`__int128` 与 `unsigned __int128` 可以隐式构造同位宽类型。
- 不同 `integer` 实例之间的构造是显式的；跨位宽或 signedness 赋值受支持，并按
  目标类型扩展或截断。
- 转换到内建整数时保留目标宽度的低位；有符号目标按补码解释这些位。
- 转换到 `bool` 为显式转换，非零值为 `true`。
- C++11 下整数构造为 `constexpr`；修改对象的复合操作从 C++14 起可获得更多
  `constexpr` 能力。

### 浮点

有限浮点数向零截断后转换为固定宽度整数，超出范围的高位按模 `2^Bits` 截断。
非有限值按以下规则转换：

- `NaN` 转为 `0`；
- `+inf` 饱和为最大值；
- `-inf` 对有符号类型饱和为最小值，对无符号类型转为 `0`。

宽整数转为 `float`、`double` 或 `long double` 时，按目标格式有效位和当前
`fegetround()` 模式完成一次舍入。

## 3. 算术

支持 `+ - * / %`、一元正负号、递增递减及对应复合赋值。操作数可以是同型宽
整数、内建整数、`__int128` 或浮点数。

- 加、减、乘均按固定宽度回绕。
- 整数除法的商向零截断；余数与被除数同号。
- `gint::divmod(a, b)` 要求两个参数是相同的 `integer` 类型，返回 `quotient` 和
  `remainder`，结果分别等同于 `a / b` 与 `a % b`。
- 有符号 gint 与无法由当前位宽表示的无符号内建整数执行 `/` 或 `%` 时，会先
  提升到可表示双方的临时类型，再把结果截回目标位宽。
- 浮点参与算术时先按第 2 节规则向零截断为整数，再执行固定宽度算术。

除零行为由配置决定：

| 配置 | `/ 0` | `% 0` |
| --- | --- | --- |
| 默认 unchecked | `0` | 被除数 |
| `GINT_ENABLE_DIVZERO_CHECKS` | `std::domain_error` | `std::domain_error` |

unchecked 行为是明确的库语义，用于避免底层整数除法 UB；它不表示除零是有效的
数学运算。

## 4. 位运算与移位

`~ & | ^` 直接作用于补码位模式，并支持内建整数与 `__int128` 混合操作。

`<<`、`>>` 及其复合赋值接受内建整数、`__int128` 和
`unsigned __int128` 位移量：

- 位移量为 `0` 或有符号负数时，值保持不变；
- 左移达到或超过位宽时结果为 `0`；
- 无符号右移达到或超过位宽时结果为 `0`；
- 有符号右移为算术右移，达到或超过位宽时负数为 `-1`、非负数为 `0`；
- 左移丢弃超出位宽的高位。

这些规则是 gint 的定义行为，不沿用内建整数对非法位移量的 UB。

## 5. 比较

支持 `== != < > <= >=`，并可与内建整数、`__int128` 和浮点数混合比较。

- 有符号 gint 与无法由当前位宽表示的无符号内建整数混合比较时，内部使用更宽
  的有符号临时值，避免把大无符号值误解释为负数。不同 signedness 的两个
  `integer` 类型不会隐式互转，应由调用方显式选择目标类型。
- 浮点比较不把整个宽整数先转换为 `long double`，而是比较符号、二进制指数和
  有效位。
- 与 `NaN` 的相等和有序比较均为 `false`；有界整数自然小于 `+inf`、大于
  `-inf`。

## 6. 字符串、stream 与 `fmt`

完整头文件提供：

- `gint::to_string(value)`：十进制字符串；
- `gint::from_string<Int>(text, base)`：解析 `std::string` 或 C 字符串；
- `operator<<`：遵循 `std::hex`、`std::oct`、`std::showbase`、符号、宽度、填充
  和对齐等整数 stream 标志；
- 定义 `GINT_ENABLE_FMT` 后提供 `fmt::formatter`。

有符号负数的十进制 stream 输出使用 `-` 前缀；十六进制和八进制输出使用当前
固定位宽的补码 bit pattern，与原生整数的非十进制展示习惯一致。

`base == 0` 时自动识别十进制、`0x`/`0X`、`0b`/`0B` 和前导 `0` 八进制；显式
进制范围是 `2..36`。解析按目标位宽累积，超宽输入按模 `2^Bits` 截断。空输入、
非法前缀、非法数字或空指针抛出 `std::invalid_argument`。

`<gint/core.h>` 不提供字符串、stream 或 `fmt` 实现；接口选择见
[集成指南](INTEGRATION.md)。

## 7. 浮点除模边界

在“浮点先向零截断”的一般规则之外，非有限值有以下明确行为：

- 除法或取模任一侧的浮点值为 `NaN` 时抛出 `std::domain_error`；
- `±inf /` 整数和 `±inf %` 整数抛出 `std::domain_error`；
- 整数 `/ ±inf` 为 `0`；整数 `% ±inf` 为原整数；
- 当浮点除数满足 `|f| < 1` 时，截断结果为 `0`，随后按当前除零配置处理。

## 8. 标准库集成

`std::numeric_limits<gint::integer<...>>` 满足：

- `digits = Bits - (is_signed ? 1 : 0)`；
- `radix = 2`、`is_exact = true`、`is_modulo = true`；
- `round_style = std::round_toward_zero`；
- `min()` / `max()` 返回对应补码边界。

`std::hash<gint::integer<...>>` 使用全部 limb，并可默认构造且 `noexcept` 调用。

## 9. 错误路径与配置一致性

启用语言异常时，解析错误抛出 `std::invalid_argument`，定义域错误抛出
`std::domain_error`。使用 `-fno-exceptions` 时，相同错误路径调用 `std::abort`。

一个 target 内必须统一影响语义或代码生成的配置；推荐通过 CMake target 管理，
见[集成指南](INTEGRATION.md)。内部隔离机制见[实现说明](INTERNALS.md)。
