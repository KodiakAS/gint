# AGENTS.md

本项目是一个高性能宽整数库，目标是提供“固定位宽、语义严格、易于集成”的实现，能够无缝嵌入到其他 C++ 工程中。

---

## 关键特性

1. C++11 优先（C++11-first）
   面向 C++11 标准编写，最大化兼容性。

2. 仅头文件（Header-only）
   无需单独的构建步骤，直接包含头文件即可使用。

3. 严格位宽（Strict bit-width）
   实际位宽即为模板参数。例如 `gint::integer<256, signed>`（即 `gint::Int256`）严格为 256 位，无隐式填充。

4. 与原生整数互操作（Interop）
   支持与标准 C++ 整数类型的算术运算及相互转换。

---

## 项目结构

- `include/` — 库头文件
- `tests/` — 单元测试（GoogleTest）
- `bench/` — 基准/对比测试（Google Benchmark）
- `docs/` — 文档
- `third_party/` — 依赖（只读，仅用于构建与测试）

---

## 代码风格

编辑后请运行 `clang-format`。根目录下的 `.clang-format` 只作用于 C++ 源码。

---

## 拉取请求（PR）规范

1. 分支名需使用英文。
2. 必须使用 PR 模板：
   - 使用 `.github/PULL_REQUEST_TEMPLATE/` 下的模板创建 PR。
   - 请选择最合适的专用模板：`feature.md`、`bug_fix.md`、`perf_change.md`、`refactor.md`、`docs_chore.md`、`breaking_change.md`。
   - 若不属于以上任一类别，再使用默认模板。

---

## 沟通规范

- 默认语言：中文（简体）。所有与维护者、贡献者以及自动化代理（AI）的对话、评审意见与报告，均应使用中文回应。
- 外部工具/命令/标志/文件名保持英文原文，不做翻译（示例：`--benchmark_min_time`、`CMAKE_BUILD_TYPE`）。
- 如用户或上游需求明确要求使用其他语言，可临时切换；未特别说明时，一律使用中文。

---

## 性能优化准则

- 概念对齐
  - 基准测试（Benchmark）：仅运行 gint 自身用例，不包含第三方对比。
    - 命令：`make bench`（标准子集）/ `make bench-full`（完整矩阵，等价传入 `--gint_full`）。
    - 可通过 `BENCH_ARGS` 传递 Google Benchmark 参数（示例：`--benchmark_min_time=0.2s`）。
  - 对比测试（Comparison）：在相同用例上对比 ClickHouse 与 Boost。
    - 命令：`make bench-compare`（标准矩阵）/ `make bench-compare-full`（完整矩阵，等价传入 `--gint_full`）。

- 术语定义
  - 目标用例：指一个或一组“具体 Benchmark 用例”，其命名遵循统一模式：
    - 名称结构：`<运算>/<场景>[/<库名>]`
      - `<运算>`：首个斜杠前的标识，如 `Add`、`Sub`、`Mul`、`Div`、`ToString` 等（未来新增运算同样遵循此前缀）。
      - `<场景>`：具体子场景（可能新增/扩展，不需要在本文档枚举）。
      - `[/<库名>]`：库名后缀在两类二进制中都存在，用于区分实现。
        - 基准测试可执行只会注册 `.../gint`。
        - 对比测试可执行会注册 `.../gint`、`.../ClickHouse`、`.../Boost`。
    - 自动发现：使用 `--benchmark_list_tests` 列出全部可用用例，避免硬编码名称；所有构建与产物目录均在 `runs/` 下。
      - 本地（仅 gint）：`make BENCH_BUILD_DIR=runs/local/build-bench bench BENCH_ARGS="--benchmark_list_tests"`（或 `bench-full`）
      - 本地（对比）：`make BENCH_BUILD_DIR=runs/local/build-bench bench-compare BENCH_ARGS="--benchmark_list_tests"`（或 `bench-compare-full`）
      - 容器（任选其一）：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench[-compare][-full] BENCH_ARGS="--benchmark_list_tests"`
  - 目标选择（生成过滤器而非枚举）：
    - 选定运算级别目标（如“优化 Add”）：使用前缀过滤 `--benchmark_filter="^Add/"`，自动包含未来新增的 `Add/*` 子场景。
    - 精确到场景（如“Add 的 NoCarry 与 FullCarry”）：`--benchmark_filter="^(Add/NoCarry|Add/FullCarry)(?:/|$)"`。
    - 多运算组合（如“Add 与 Sub”）：`--benchmark_filter="^(Add|Sub)/"`。
    - 指定库（用于对比测试中的单库评估）：在过滤器结尾限定库名，例如只看 `gint`：`--benchmark_filter="^Add/.*?/gint$"`；或三库：`/(gint|ClickHouse|Boost)$`。
  - 当提出“优化某一项计算（如 Add/Sub）”的需求时，应以“过滤器表达式”明确目标用例集合，并在整个优化周期保持一致；示例：`BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s"`。

- 统一准则
  - 优化前，先在“目标用例”上建立基线：运行对应的基准测试并记录结果。
  - 优化后，在相同环境与参数下重跑同一组基准测试作为结果。
  - 评估与报告以“结果 vs 基线”为准，确保同一机器、同一编译器与选项、相同 `BENCH_ARGS`。
  - 若需求明确表示“关注某项指标上与 Boost/ClickHouse 的差异”，则：
    - 基线与结果均应采用对比测试（`bench-compare` 或 `bench-compare-full`）。
    - 报告以三方对比数据为准，突出 gint 相对 ClickHouse/Boost 的提升或达标情况。
  - 若需求要求同时关注“本地环境”和“Docker 环境”的表现：
    - 必须分别在两个环境中建立基线并产出结果（两套基线、两套结果）。
    - Docker 环境固定使用本项目的开发镜像 `gint:centos8`（若镜像已存在则复用；不存在再构建）。
    - 报告需分别呈现本地与 Docker 的数据，避免跨环境直接混合对比，并注明镜像与工具链版本。

- 环境选择原则（Codex/AI 执行）
  - 默认仅使用“本地环境（local）”进行评估与产出，不自动启用容器。
  - 仅当用户在指令中明确提出“Docker”或“两者（local+docker）”时，才在容器内额外（或仅）执行对应流程。
  - 当用户指定环境集合时，严格按指定执行；未指定时，一律视为“仅本地”。
  - 构建与产物目录位置（强制）：
    - 本地构建目录：`BENCH_BUILD_DIR=runs/local/build-bench`（禁止写到 `runs/` 之外）。
    - 容器构建目录：`BENCH_BUILD_DIR=runs/docker/build-bench`（禁止写到 `runs/` 之外）。
  - 在本地与容器间切换或并行评估时，无需清理共享目录（因目录已隔离于 `runs/`）；若误写至仓库根的构建目录，请先清理再改用 `runs/` 下目录。
  - 所有中间文件与结果统一写入 `runs/`（本地：`runs/local/`；容器：`runs/docker/`）。

- 最佳实践
  - 固定并记录环境：CPU/内存/OS、编译器版本、`CMAKE_BUILD_TYPE`、`BENCH_ARGS` 与（若适用）Docker 镜像标签。
  - 建议设定最短运行时间减少抖动：`BENCH_ARGS="--benchmark_min_time=0.2s"`。
  - 需要完整矩阵时使用 `bench-full` 或 `bench-compare-full`（等价于传 `--gint_full`）。
  - 报告中包含提交哈希（`git rev-parse HEAD`）与关键修改点，便于复现与回溯。
- Docker 使用规范（适于自动化脚本/AI 执行）：
    - 检查镜像是否存在：`docker image inspect gint:centos8 >/dev/null 2>&1`；若不存在，构建：`make image`（等价于 `docker build -t gint:centos8 .`）。
    - 目录约定（强制）：所有构建与产物目录必须在 `runs/` 下，禁止写到仓库根目录。
      - 本地构建目录：`BENCH_BUILD_DIR=runs/local/build-bench`；容器构建目录：`BENCH_BUILD_DIR=runs/docker/build-bench`。
    - 在容器内运行基准或对比测试（挂载当前工作区）：
      - 基准完整矩阵：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-full BENCH_ARGS="--benchmark_min_time=0.2s"`
      - 对比完整矩阵：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-compare-full BENCH_ARGS="--benchmark_min_time=0.2s"`
    - 确保本地与 Docker 使用完全相同的 `BENCH_ARGS` 与源码版本，以保证“结果 vs 基线”的可比性。

---

## 产物目录规范（中间结果与自动化输出）

- 强制要求：所有中间结果与临时目录（包含构建目录、缓存、基准输出等）必须写入仓库根目录下的 `runs/`；禁止在 `runs/` 之外新建临时文件/目录。
- 统一将本地与容器内的中间结果（如 `--benchmark_out` 生成的 JSON/CSV、对比脚本输出等）写入 `runs/`，该目录已加入 `.gitignore`，默认不提交。
- 推荐结构：
  - 本地：`runs/local/`
  - 容器：`runs/docker/`
- 示例：
  - 本地对比（Add 运算）：`make BENCH_BUILD_DIR=runs/local/build-bench bench-compare-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/local/add_compare.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
  - 容器对比（Add 运算）：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-compare-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/docker/add_compare.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`

---

## 报告规范（性能）

- 默认仅展示结果：除非 PR 模板明确要求“测试方法/步骤”，性能报告只需要给出结果（表格/要点 + 产出文件路径），无需重复描述方法。
- 方法固定且可复用：测试方法已在本文件“性能优化准则/优化流程”中固化，命令与参数不必在每个 PR 中再次粘贴。
- 必要最小信息：
  - 目标用例过滤器（如 `^Div/`）、`BENCH_ARGS`、`CMAKE_BUILD_TYPE`、编译器版本；
  - 环境选择（本地/容器/两者；容器镜像标签）、提交哈希；
  - 结果文件路径（存于 `runs/` 下的 JSON/CSV）。
- 若 PR 模板要求“测试方法/步骤”，则引用本文件对应章节，并补充与本次评估相关的差异化信息（如特定过滤器或额外参数）。

---

## 优化流程（可直接复用的步骤）

- 0) 选择环境（遵循“环境选择原则”）
  - 未指定时，仅在本地环境执行；
  - 指定 docker 或 both 时，按要求在容器（或两者）执行；
  - 构建目录强制位于 `runs/`：本地 `BENCH_BUILD_DIR=runs/local/build-bench`；容器 `BENCH_BUILD_DIR=runs/docker/build-bench`。

- 1) 明确目标用例（用过滤器表达目标，而非枚举名称）
  - 列出全部用例（便于确认命名与可选场景；构建目录在 runs/ 下）：
    - 本地：`make BENCH_BUILD_DIR=runs/local/build-bench bench BENCH_ARGS="--benchmark_list_tests"` 或 `bench-full`
    - 对比：`make BENCH_BUILD_DIR=runs/local/build-bench bench-compare BENCH_ARGS="--benchmark_list_tests"` 或 `bench-compare-full`
    - 容器：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench[-compare][-full] BENCH_ARGS="--benchmark_list_tests"`
  - 选择过滤器（示例）：
    - 运算级：`BENCH_ARGS="--benchmark_filter=^Add/"`
    - 精确到场景：`BENCH_ARGS="--benchmark_filter=^(Add/NoCarry|Add/FullCarry)(?:/|$)"`
    - 对比中特定库：`BENCH_ARGS="--benchmark_filter=^Add/.*?/gint$"`

- 2) 建立基线（local 与 docker 视需求分别建立）
  - 推荐统一输出 JSON，便于自动化对比：添加 `--benchmark_out=<file>.json --benchmark_out_format=json --benchmark_report_aggregates_only=true`
  - 本地-基准（示例，仅 gint）：
    - 标准集：`make BENCH_BUILD_DIR=runs/local/build-bench bench BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/local/add_baseline.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
    - 完整矩阵：`make BENCH_BUILD_DIR=runs/local/build-bench bench-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/local/add_baseline_full.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
  - Docker-基准（如需）：
    - `docker image inspect gint:centos8 >/dev/null 2>&1 || make image`
    - `docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/docker/add_baseline_full.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
  - 若目标是“超过 Boost/ClickHouse”，则改用对比可执行：`make bench-compare[-full] ...`

- 3) 实施优化并重建
  - 修改代码后运行：`make bench` 或 `make bench-full`（或对应的 `bench-compare`/`bench-compare-full`）。
  - 保持与基线完全一致的 `BENCH_ARGS`、构建类型与环境。

- 4) 采集结果
  - 本地-结果：`make BENCH_BUILD_DIR=runs/local/build-bench bench-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/local/add_result_full.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
  - Docker-结果（如需）：`docker run --rm -t -v "$PWD":/work -w /work gint:centos8 make BENCH_BUILD_DIR=runs/docker/build-bench bench-full BENCH_ARGS="--benchmark_filter=^Add/ --benchmark_min_time=0.2s --benchmark_out=runs/docker/add_result_full.json --benchmark_out_format=json --benchmark_report_aggregates_only=true"`
  - 对比目标：使用 `bench-compare[-full]` 生成对应的 `runs/*/add_baseline*_compare.json` 与 `runs/*/add_result*_compare.json`。

- 5) 评估与报告（机器可读）
  - 从 JSON 中读取 `real_time` 或 `cpu_time` 的聚合指标（mean/median），计算加速比：`speedup = baseline / result`。
  - 报告需包含：
    - 过滤器与 `BENCH_ARGS`、`CMAKE_BUILD_TYPE`、编译器版本；
    - 机器信息/容器镜像标签；
    - 基线与结果文件名、提交哈希（`git rev-parse HEAD`）；
    - 若有对比目标，列出 gint 与 ClickHouse/Boost 的并列表现与相对差距。
  - 报告精简：除非 PR 模板要求，否则此处在 PR 中仅呈现“结果摘要/表格 + 产物路径”，方法从略。
