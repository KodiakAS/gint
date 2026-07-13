# 贡献指南

感谢参与 gint。提交应同时维护正确性、header-only 集成、C++11 兼容性和可测量
性能；仅在更新工具链上编译成功不足以证明变更安全。

## 开始之前

- 阅读[文档导航](docs/README.md)，确认要修改的契约由哪份文档负责。
- 所有构建、依赖、日志和临时结果放在 `runs/` 下。
- 保留工作区中与当前任务无关的修改和产物。

本机主测试：

```sh
make test
```

实现的 source of truth 是 `src/gint/*.hpp`；不要直接编辑生成并提交的
`include/gint/gint.h`。修改内部头后先运行 `make amalgamate`，提交前运行
`make internal-headers-check amalgamate-check`，确保每个内部头可独立解析且交付头同步。

测试构建需要 CMake、GoogleTest 和 `fmt`。可复现的 Linux 依赖与编译器环境见
[验证环境](docs/VALIDATION_ENVIRONMENTS.md)。

## 按风险选择验证

| 变更 | 最小验证 |
| --- | --- |
| 仅文档 | 检查相对链接，运行 `git diff --check` |
| 普通 C++ | 精确回归测试，再运行 `make test` |
| 公共头文件或 CMake | C++11 单头文件、`core.h`、consumer/package contract |
| 解析、除法、移位或浮点 | 主测试外增加 sanitizer 与 differential；适用时增加 fuzz |
| hot path | 先跑 codegen contract，再做同机同工具链的前后 benchmark |
| workflow 或性能工具 | `actionlint`、Python 工具测试及对应 smoke |

常用附加门禁：

```sh
CXX=c++ GINT_DIFFERENTIAL_BUILD_DIR=runs/local/differential \
  scripts/run-differential.sh
python3 -m unittest discover -s tests/perf -p 'test_*.py'
bash scripts/check-codegen-contract.sh c++ runs/local/codegen
```

libFuzzer、采样参数和结果解释见[基准测试](docs/BENCHMARKS.md)与相关脚本。
跨平台或发布候选变更应按[支持策略](docs/SUPPORT.md)和
[发布流程](docs/RELEASING.md)扩大验证范围。

## 代码与文档

- C++ 文件使用仓库 `.clang-format`；提交前运行 `git diff --check`。
- shell 脚本保持 Bash 兼容并运行 `bash -n`；可用时再运行 `shellcheck`。
- correctness 修复应先保存最小复现，再增加能在旧实现上失败的回归测试。
- 性能结论必须来自同环境前后数据，不跨 OS、架构或编译器合并比较。
- 用户可见行为、支持范围或集成方式变化，应同步权威文档、
  [CHANGELOG.md](CHANGELOG.md) 和必要的[升级说明](docs/UPGRADING.md)。

## Pull request

- 保持单一主题，使用 `.github/PULL_REQUEST_TEMPLATE/` 中匹配变更类型的模板。
- 标题使用 `feat:`、`fix:`、`perf:`、`refactor:`、`docs:` 或 `break:` 前缀。
- 正文说明行为、风险和验证结果，不粘贴本地 artifact 路径或重复执行流水账。
- CI 和 review 完成前保持 draft；准备合并时确认本地提交、远端 head 与验证对象一致。
