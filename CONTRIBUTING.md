# 贡献指南

## 开发步骤

1. 从[文档导航](docs/README.md)确认受影响契约，保留无关工作区修改。
2. 实现变更编辑 `src/gint/*.hpp`，不要直接编辑生成的 `include/gint/gint.h`。
   新增或移动内部头需遵守[模块与生成器约束](docs/INTERNALS.md)。
3. 修改内部头后运行 `make amalgamate`，再运行
   `make internal-headers-check amalgamate-check`，保证内部头可独立解析且生成头同步。
4. 按下表验证，用户可见变化同步所属文档、[变更记录](CHANGELOG.md)和必要的
   [升级说明](docs/UPGRADING.md)。

测试构建需要 CMake、GoogleTest、`fmt` 和 Python 3.5+。依赖准备、独立构建目录
和最低编译器环境见[验证环境](docs/VALIDATION_ENVIRONMENTS.md)。所有构建、依赖、
日志和临时结果放入 `runs/`，不同 OS、架构或编译器不得复用 CMake cache。

## 按风险选择验证

所有变更运行 `git diff --check`；多个类别同时适用时，合并执行所需门禁。

| 变更 | 必需验证 |
| --- | --- |
| 仅文档 | 相对链接、路径和命令；不要求完整测试 |
| C++ | 覆盖变更的精确测试，再运行 `make test`；correctness 修复先保存最小复现，增加旧实现会失败的回归测试 |
| 内部头 | `make internal-headers-check amalgamate-check` |
| 公共头、CMake 或安装 | C++11 `-Wall -Wextra -Werror`、单头独立及重复包含、consumer/package、精确安装清单；另用 CMake 3.13 lane 验证最低版本 |
| 解析、除法、移位、浮点或 signed 边界 | 主测试外运行 sanitizer 和独立 differential；适用时运行 `scripts/run-fuzz.sh` |
| hot path | 修改前建立 baseline，通过受影响架构的 codegen contract，再串行采集同环境前后 benchmark，见[基准测试](docs/BENCHMARKS.md) |
| 性能工具 | Python 工具测试和 codegen contract，命令见[基准测试](docs/BENCHMARKS.md#codegen-contract) |
| Shell | `bash -n`，可用时运行 `shellcheck` |
| Workflow | 可用时运行 `actionlint`，并执行对应 smoke |

header、consumer、package 与安装检查由 `CMakeLists.txt` 和 `tests/cmake/` 维护，
随 `make test` 执行。确定性 differential 的标准入口：

```sh
CXX=c++ GINT_DIFFERENTIAL_BUILD_DIR=runs/local/differential \
  scripts/run-differential.sh
```

C++ 使用仓库 `.clang-format` 格式化受影响文件。跨平台或发布候选按
[支持策略](docs/SUPPORT.md)和[发布流程](docs/RELEASING.md)扩大矩阵，逐环境报告；
不能用单一工具链结果替代其他支持组合。

## Pull request

- 保持单一主题，使用 [.github/PULL_REQUEST_TEMPLATE/](.github/PULL_REQUEST_TEMPLATE/)
  中匹配的模板；API 或 CLI 创建时也需套用并核对最终标题与正文。
- 标题使用 `feat:`、`fix:`、`perf:`、`refactor:`、`docs:` 或 `break:` 前缀，
  不添加 `[codex]`、`AI` 等额外标记。
- 正文说明问题、最终行为和验证结果；风险与迁移动作按需填写，不贴本地 artifact 路径或执行流水账。
- 准备期间保持 draft；完成本地验证并准备好评审后标记 Ready，触发仓库自动 review。
  CI 与 review 通过后才能合并，并确认本地提交、远端 head 与验证对象一致。
