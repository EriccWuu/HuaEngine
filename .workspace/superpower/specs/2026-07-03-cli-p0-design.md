# HuaEngine CLI P0 设计文档

## 背景

HuaEngine 当前已经具备 `HuaEngineCLI`、`HuaEngineCLICore`、`HE::CLI::CommandRunner`、JSON 输出、exit code 映射，以及 `CLIHostSmoke` 和 `CLIWorkflowSmoke`。CLI 也已经覆盖 `project`、`scene`、`asset`、`script`、`validation` 等基础工作流。

因此本轮 P0 不再按“从零建设 CLI”处理，而是一次性完成三个基础收口目标：

1. 固化 CLI 对 Agent 和自动化脚本的契约。
2. 引入轻量 command catalog，让 help、usage 和命令契约有单一来源。
3. 拆分当前偏大的 `CLICommandRunner.cpp`，为后续 Reflection Tool、资产工具、YAML 迁移和 package 命令预留可维护入口。

本轮不要求暴露所有 `ApplicationOperations`。已有正式操作面但 CLI 尚未暴露的能力，可以在本轮结构稳定后按真实需求逐步增加。

## 目标

### 契约稳定

CLI stdout 继续保持机器可读 JSON，exit code 继续与 `ResultEnvelope` 语义一致：

- `success` -> `0`
- `failure` -> `1`
- `manual_intervention_required` -> `2`
- CLI 宿主未处理异常 -> `70`

P0 需要新增或强化契约测试，覆盖空参数、未知命令、未知选项、缺少选项值、缺 required option、manual intervention、cwd project resolve，以及 JSON 顶层结构。

### Command Catalog 收口

新增轻量 `CLICommandCatalog`。它只描述 CLI 命令契约，不承载业务逻辑。

每个命令描述至少包含：

- command path，例如 `scene entity create`
- summary
- usage
- formal operation，例如 `scene.entity.create`
- value options
- flag options
- required options

`help`、usage error 和后续命令清单都应从 catalog 生成。`ops list` 继续展示正式 `OperationRegistry`，不改变为 CLI 命令列表。

### Runner 拆分

当前 `CLICommandRunner.cpp` 同时承担 command path 识别、option parse、usage error、路径解析、业务编排和响应组装。P0 拆分目标是降低文件职责密度，而不是引入复杂命令框架。

拆分后，`CommandRunner` 保留为总入口，只负责：

- 接收 argv 和 working directory
- 匹配 command path
- 调用 option parser
- 分发到 domain handler
- 返回 `CLICommandResponse`

业务写操作仍必须只通过 `ApplicationOperations` 完成。

## 架构设计

### 模块

#### `CLICommandCatalog`

负责集中声明 CLI 命令契约。

职责：

- 注册当前支持的 CLI commands。
- 支持按 argv 前缀匹配最长 command path。
- 为 help、usage error 和 tests 提供命令元数据。
- 记录 CLI command 到 formal operation 的显式映射。

非职责：

- 不调用 `ApplicationOperations`。
- 不保存运行时业务状态。
- 不决定命令执行流程。

#### `CLIOptionParser`

负责基于 catalog 中的 option 定义解析参数。

职责：

- 识别 value options 和 flag options。
- 统一处理 unknown option、unexpected positional argument、option missing value。
- 校验 required options。
- 失败时返回标准 `cli.usage` 结果。

#### `CLICommandContext`

封装 handler 所需的公共上下文和 helper。

必须包含：

- `ApplicationOperations&`
- working directory
- path normalize helper
- project context resolve helper
- scene path resolve helper
- payload/details merge helper

这样 project、scene、asset、script、validation handler 不再各自复制路径解析细节。

#### Domain handlers

按 domain 拆分命令实现：

- `CLIMetaCommands`：`help`、`ops list`
- `CLIProjectCommands`：`project init/status`
- `CLISceneCommands`：`scene create/validate/entity/component`
- `CLIAssetCommands`：`asset register-default-mesh/validate`
- `CLIScriptCommands`：`script status/initialize/update/shutdown`
- `CLIValidationCommands`：`validation run`

每个 handler 只做当前 domain 的命令编排，并调用正式 `ApplicationOperations`。

### 数据流

```text
argv
  -> CommandRunner
  -> CLICommandCatalog match
  -> CLIOptionParser parse/validate
  -> domain command handler
  -> ApplicationOperations
  -> CLICommandResponse
  -> RenderJson + ExitCodeFor
```

### Help 与命令清单

`help` 继续返回 JSON result。P0 要求 help 内容来自 catalog，不再在 `CommandRunner` 中散落硬编码命令摘要。

`ops list` 继续返回 `data.operations`，数据源仍是 `ApplicationOperations::GetOperationRegistry()`。它展示正式操作注册表，不展示 CLI alias 或 CLI command catalog。

如果需要展示 CLI command list，可以在 catalog 稳定后增加 `commands list` 或 `help --json`。这不是 P0 必须项，除非实现时成本极低且不扩大结构复杂度。

## 测试设计

### 新增 `CLIContractSmoke`

该测试聚焦契约，不承担完整业务工作流。

必须覆盖：

- 空参数：exit code 为 `1`，`result.operation` 为 `cli.usage`。
- unknown command：exit code 为 `1`。
- unknown option：exit code 为 `1`。
- option 缺少 value：exit code 为 `1`。
- required option 缺失：exit code 为 `1`。
- `validation run --include-scripts` 缺 `--scene`：exit code 为 `1`。
- manual intervention case：exit code 为 `2`。
- 每个失败输出都包含 `host`、`result`、`status`、`payload`、`details`。

manual intervention 可优先使用损坏 project metadata 或损坏 asset registry 构造，避免依赖 GUI 或渲染。

### 强化 `CLIHostSmoke`

`CLIHostSmoke` 继续以 in-process 方式验证 runner 和 core 模块。

新增断言：

- catalog 包含当前文档列出的正式 CLI commands。
- 每个 catalog command 都有 summary 和 usage。
- 映射到 formal operation 的命令，其 operation 存在于 `OperationRegistry`；`cli.*` meta command 例外。
- `help` 输出来自 catalog。

### 强化 `CLIWorkflowSmoke`

`CLIWorkflowSmoke` 继续保留外部进程 happy path，防止 runner 拆分后破坏真实可执行流程。

现有工作流继续覆盖：

- `ops list`
- `project init`
- `asset register-default-mesh`
- `scene create`
- `scene entity create`
- `script status`
- `validation run`

必须补充 cwd project resolve：

- 在项目子目录运行 `project status` 或 `validation run`。
- 不传显式 `--path`，验证 CLI 能向上解析项目上下文。

## 验收命令

```powershell
cmake --build build --config Debug --target HuaEngineCLI
cmake --build build --config Debug --target CLIHostSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
cmake --build build --config Debug --target CLIContractSmoke

& .\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIContractSmoke.exe
```

如果拆分过程中影响 CMake target，应确保 `HuaEngineCLICore` 正确包含新增 `.cpp/.h` 文件。

## 非目标

- 不要求一次性暴露所有 `ApplicationOperations`。
- 不引入第三方 CLI parser。
- 不改变 stdout JSON 顶层协议。
- 不改变 exit code 语义。
- 不改变 `ops list` 的正式 operation registry 语义。
- 不把 CLI 变成绕过 `ApplicationOperations` 直接改 scene、asset、project 文件的工具。
- 不把 Reflection Tool、YAML 迁移、package、RenderGraph、RHI 纳入本轮。

## 风险与缓解

Runner 拆分可能在没有行为保护时引入回归。缓解方式是先补 `CLIContractSmoke`，再做 catalog 和拆分，最后跑 `CLIHostSmoke` 与 `CLIWorkflowSmoke`。

Command catalog 可能膨胀成复杂框架。缓解方式是只保存命令契约和 option 元数据，不承载业务逻辑，也不引入 handler 注册框架。

`ops list` 和 CLI command list 容易混淆。缓解方式是在代码和文档中明确：`ops list` 只展示 `OperationRegistry`；CLI command list 只能来自 `CLICommandCatalog`，两者语义不同。

manual intervention 测试如果依赖文件损坏细节，可能较脆弱。缓解方式是选择现有服务已经稳定返回 `ManualInterventionRequired` 的路径，并只断言 exit code、status 和关键 operation，不断言完整英文 summary。

## 后续事项

P0 完成后，再按真实需求决定是否新增 CLI 命令暴露更多正式操作，例如：

- `scene entity rename`
- `scene entity transform`
- `scene component upsert`
- `asset load-mesh/load-material/register-texture/resolve`

新增命令时应先进入 catalog，再补 contract 或 workflow smoke，最后实现 handler。
