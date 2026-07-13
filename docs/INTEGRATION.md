# 集成指南

本文只说明如何把 gint 接入项目。运算与边界语义见[技术规格](TECH_SPEC.md)，
编译器和平台承诺见[支持策略](SUPPORT.md)。

## 公共头文件

### 完整接口

只复制 [`include/gint/gint.h`](../include/gint/gint.h) 即可获得完整接口：宽整数、
字符串转换、stream 输出，以及按需启用的 `fmt` 适配。该方式不依赖 CMake、
生成头或链接库。

```cpp
#include <gint/gint.h>

using U256 = gint::integer<256, unsigned>;
U256 value = (U256(1) << 128) + 42;
```

### 精简算术接口

纯算术翻译单元可以包含 `<gint/core.h>`，跳过字符串、stream 和 `fmt` 实现的
解析。使用这个入口时应同时分发 `core.h` 与 `gint.h`；`core.h` 会从同目录包含
后者。之后在同一翻译单元中再包含 `<gint/gint.h>`，可以补齐完整接口。

## CMake

### 源码树消费

```cmake
add_subdirectory(path/to/gint)
target_link_libraries(my_target PRIVATE gint::gint)
```

`FetchContent` 获取源码后使用同一 target。作为子项目时，gint 默认不构建测试、
benchmark 或安装规则。

### 安装后消费

最低支持 CMake 3.13。安装仓库可使用兼容该版本的 build target：

```sh
cmake -S . -B runs/local/install \
  -DGINT_BUILD_TESTS=OFF \
  -DGINT_BUILD_BENCHMARKS=OFF \
  -DGINT_INSTALL=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/runs/local/prefix"
cmake --build runs/local/install --target install
```

消费者随后使用：

```cmake
find_package(gint 0.9 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE gint::gint)
```

0.x package 的版本匹配规则见[升级指南](UPGRADING.md)，精确安装清单见
[发布流程](RELEASING.md)。

## CMake targets 与配置

| Target / 宏 | 用途 |
| --- | --- |
| `gint::gint` | 默认 header-only 接口 |
| `gint::checked` | 传递 `GINT_ENABLE_DIVZERO_CHECKS`，除零或模零进入错误路径 |
| `GINT_ENABLE_FMT` | 启用 `fmt::formatter`；消费者还需提供并链接 `fmt` |

直接使用宏时，应在首次包含相关 gint 头文件前定义。影响语义或代码生成的宏必须
在一个 target 内保持一致；跨翻译单元混用配置不属于支持用法。

启用 `fmt` 的典型配置：

```cmake
find_package(fmt CONFIG REQUIRED)
target_compile_definitions(my_target PRIVATE GINT_ENABLE_FMT)
target_link_libraries(my_target PRIVATE gint::gint fmt::fmt)
```

## 常用 API

`gint::integer<Bits, Signed>` 支持 `64/128/256/512/1024` 位；`Signed` 使用
`signed` 或 `unsigned`。常用别名包括 `Int128`、`UInt128`、`Int256` 和
`UInt256`。

```cpp
gint::UInt256 value = gint::from_string<gint::UInt256>("0xffff", 0);
std::string decimal = gint::to_string(value);
gint::UInt256 divisor = 10;
gint::divmod_result<gint::UInt256> qr = gint::divmod(value, divisor);
```

完整运算符、转换、解析和错误语义由[技术规格](TECH_SPEC.md)定义。

## 无异常构建

普通算术可以使用 `-fno-exceptions`。当语言异常不可用时，本应抛出异常的路径
（例如无效字符串、非有限浮点除模或启用检查后的除零）调用 `std::abort`。
