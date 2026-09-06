# 集成指南

本文只说明如何把 gint 接入项目。运算与边界语义见[技术规格](TECH_SPEC.md)，
编译器和平台承诺见[支持策略](SUPPORT.md)。

## 公共头文件

只复制 [`include/gint/gint.h`](../include/gint/gint.h) 即可获得完整接口：宽整数、
字符串转换、stream 输出，以及按需启用的 `fmt` 适配。该方式不依赖 CMake、
生成步骤或链接库。

```cpp
#include <gint/gint.h>

using U256 = gint::integer<256, unsigned>;
U256 value = (U256(1) << 128) + 42;
```

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

package 的版本匹配规则见[支持策略](SUPPORT.md#0x-维护策略)，精确安装清单见
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

解析、输出与同时求商余数的示例：

```cpp
gint::UInt256 value = gint::from_string<gint::UInt256>("0xffff", 0);
std::string decimal = gint::to_string(value);
gint::UInt256 divisor = 10;
gint::divmod_result<gint::UInt256> qr = gint::divmod(value, divisor);
```

完整运算符、转换、解析和错误语义由[技术规格](TECH_SPEC.md)定义。

## 无异常构建

可使用 `-fno-exceptions`；错误路径的行为见
[技术规格](TECH_SPEC.md#9-错误路径)。
