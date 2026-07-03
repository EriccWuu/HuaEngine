# HuaEngine CLI 使用文档

## 1. 定位

`HuaEngineCLI.exe` 是当前引擎正式的无 GUI 控制面。

它的定位不是“测试小工具”，而是：

- 引擎的脚本化入口
- AI Agent 的稳定操作面
- GUI 之外可自动化消费的正式宿主

当前约束是：

- stdout 输出机器可读 JSON
- exit code 反映统一结果语义
- 命令本身只消费 `ApplicationOperations`
- CLI 不直接绕过正式操作层访问底层 domain services

## 2. 构建

当前仓库默认通过 CMake 构建。

示例：

```powershell
cmake -S . -B build
cmake --build build --config Debug --target HuaEngineCLI
```

如果你不确定可执行文件落在哪里，可以在构建目录里搜索：

```powershell
Get-ChildItem -Recurse -Filter HuaEngineCLI.exe build
```

下文用 `<CLI>` 代表 `HuaEngineCLI.exe` 的实际路径。

## 3. 输出契约

每次执行都会向 stdout 输出一段 JSON，顶层结构如下：

```json
{
  "host": "huaengine-cli",
  "result": {
    "operation": "scene.create",
    "target": "D:/path/to/scene.scene",
    "status": "success",
    "summary": "Scene created and saved",
    "can_continue_automatically": true,
    "requires_manual_intervention": false,
    "payload": {},
    "details": []
  },
  "data": {}
}
```

其中：

- `host`
  - 固定为 `huaengine-cli`
- `result.operation`
  - 正式操作名，不一定和 CLI 子命令字面完全相同
- `result.status`
  - `success`
  - `failure`
  - `manual_intervention_required`
- `result.payload`
  - 稳定键值载荷，适合脚本消费
- `result.details`
  - 诊断明细，包含 `severity / code / message / context`
- `data`
  - 某些命令附加的数据区，例如 `ops list`

## 4. 退出码

- `0`
  - `success`
- `1`
  - `failure`
- `2`
  - `manual_intervention_required`
- `70`
  - CLI 宿主内部未处理异常

脚本自动化时，建议同时看：

- 退出码
- `result.status`
- `result.can_continue_automatically`

## 5. 路径语义

### 项目根

项目通过：

```text
<root>/.huaengine/project.json
```

识别。

### 相对路径解析

- `project status --path`
  - 会向上解析最近的项目上下文
- `scene create --project`
  - 相对输出路径会解析到项目的 scene root
- `scene validate --project`
  - 相对 `--scene` 会解析到项目的 scene root
- `validation run --path`
  - `--path` 解析项目根，`--scene` 在有项目上下文时相对 scene root 解析

如果没有显式传项目路径，CLI 会从当前工作目录尝试解析。

## 6. Command Catalog 与 Operation Registry

CLI 里有两张表，职责不同，不能混用：

- Command Catalog
  - 描述 CLI 命令本身，例如 `project status`、`scene create`、`asset register-default-mesh`
  - 负责用户可见的命令路径、用法、参数摘要和 help 输出
  - 允许存在 `cli.*` 这样的 CLI 宿主命令，例如 `help` 和 `ops list`
- Operation Registry
  - 描述引擎正式能力，例如 `project.initialize`、`scene.create`、`asset.create_builtin_mesh`
  - 是自动化和宿主层判断正式能力是否存在的来源
  - `ops list` 只展示这张正式 OperationRegistry，不展示 CLI 别名或 help 命令

因此：

- `help` 展示的是 CLI 用法，来源是 Command Catalog。
- `ops list` 展示的是正式 OperationRegistry，适合脚本确认引擎能力。
- 新增 CLI 命令时，应先把命令路径、summary、usage 和参数定义加入 catalog，再接入 handler，最后补 smoke 覆盖。

## 7. 命令总览

| 命令 | 作用 | 备注 |
|---|---|---|
| `help` | 输出帮助摘要 | 不接受选项 |
| `ops list` | 列出正式 OperationRegistry | `data.operations` 返回列表 |
| `project init` | 初始化项目根 | 会创建 `.huaengine/project.json` 和托管目录 |
| `project status` | 检查项目状态 | 验证 metadata、assets、scenes 目录 |
| `scene create` | 创建并保存场景 | 先建内存 Scene，再落盘 |
| `scene validate` | 校验场景运行时约束 | 会先加载场景 |
| `asset register-default-mesh` | 注册并持久化内建 mesh 资产 | 当前支持 `quad/cube/sphere` |
| `asset validate` | 校验资产注册表健康度 | 基于项目上下文 |
| `script status` | 查看脚本绑定健康度 | 会先加载场景 |
| `script initialize` | 为场景创建脚本实例 | 会先附着 script runtime |
| `script update` | 推进一帧脚本更新 | 会先附着 script runtime |
| `script shutdown` | 销毁活动脚本实例 | 会先附着 script runtime |
| `validation run` | 聚合校验项目/场景/资产/脚本 | 可按 flag 增减域 |

## 8. 命令详解

### 8.1 `help`

```powershell
<CLI> help
```

用途：

- 查看 CLI 摘要
- 快速确认当前宿主可执行

### 8.2 `ops list`

```powershell
<CLI> ops list
```

用途：

- 列出当前正式 OperationRegistry
- 确认自动化层应该调用哪些正式能力名

返回中会包含：

```json
{
  "data": {
    "operations": [
      {
        "name": "project.initialize",
        "domain": "Project",
        "summary": "Initialize a HuaEngine project root"
      }
    ]
  }
}
```

### 8.3 `project init`

```powershell
<CLI> project init --root D:/Workspace/MyGame --name MyGame
```

可选项：

- `--root <path>`
  - 项目根目录
  - 缺省时使用当前工作目录
- `--name <name>`
  - 项目名

作用：

- 初始化项目 metadata
- 创建托管目录
- 建立后续 asset/scene CLI 的上下文根

### 8.4 `project status`

```powershell
<CLI> project status --path D:/Workspace/MyGame
```

可选项：

- `--path <path>`
  - 可传项目根，也可以传项目内任意子路径，CLI 会向上解析项目上下文

作用：

- 检查 `.huaengine/project.json`
- 检查 metadata 目录
- 检查 asset/scene 目录

### 8.5 `scene create`

```powershell
<CLI> scene create --project D:/Workspace/MyGame --name "Main Scene"
```

```powershell
<CLI> scene create --project D:/Workspace/MyGame --name "Main Scene" --output gameplay/main.scene
```

参数：

- `--project <path>`
  - 项目路径，可选
- `--name <name>`
  - 必填
- `--output <path>`
  - 可选
  - 相对路径会落到项目 scene root 下
  - 缺省时按场景名生成 `<sanitized_name>.scene`

返回 payload 常见键：

- `scene_name`
- `scene_path`

### 8.6 `scene validate`

```powershell
<CLI> scene validate --project D:/Workspace/MyGame --scene gameplay/main.scene
```

参数：

- `--project <path>`
  - 可选
- `--scene <path>`
  - 必填

当前校验重点包括：

- 是否有场景名
- 是否有实体缺失 `TransformComponent`
- 是否有渲染实体缺失 `MeshComponent`
- 是否有渲染实体缺失 `MaterialComponent`
- 是否仍使用 legacy `RendererComponent`

### 8.7 `asset register-default-mesh`

```powershell
<CLI> asset register-default-mesh --project D:/Workspace/MyGame --asset-id builtin.quad
```

```powershell
<CLI> asset register-default-mesh --project D:/Workspace/MyGame --asset-id builtin.cube --primitive cube --name "Default Cube"
```

参数：

- `--project <path>`
  - 可选
- `--asset-id <id>`
  - 必填
- `--primitive <quad|cube|sphere>`
  - 可选，默认 `quad`
- `--name <display-name>`
  - 可选，默认取 primitive 名

作用：

- 创建内建 mesh
- 持久化到项目资产空间
- 同步登记到正式资产注册表

### 8.8 `asset validate`

```powershell
<CLI> asset validate --path D:/Workspace/MyGame
```

参数：

- `--path <path>`
  - 可选
  - 会向上解析项目上下文

作用：

- 校验项目资产注册表健康度
- 发现缺文件、类型不匹配或需人工介入的问题

### 8.9 `script status`

```powershell
<CLI> script status --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 读取场景
- 汇总脚本绑定数量、启用数量、活动实例数量、缺绑定数量

注意：

- 这一步只检查当前场景里的脚本状态
- 不会自动初始化脚本实例

### 8.10 `script initialize`

```powershell
<CLI> script initialize --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再为场景创建脚本实例

### 8.11 `script update`

```powershell
<CLI> script update --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再推进一轮脚本更新

适合：

- 单步 smoke
- 无 GUI 逻辑验证

### 8.12 `script shutdown`

```powershell
<CLI> script shutdown --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再销毁活动脚本实例

### 8.13 `validation run`

只校验项目：

```powershell
<CLI> validation run --path D:/Workspace/MyGame
```

校验项目和资产：

```powershell
<CLI> validation run --path D:/Workspace/MyGame --include-assets
```

校验项目、场景、资产、脚本：

```powershell
<CLI> validation run --path D:/Workspace/MyGame --scene gameplay/main.scene --include-assets --include-scripts
```

参数：

- `--path <path>`
  - 可选，项目路径
- `--scene <path>`
  - 可选，场景路径
- `--include-assets`
  - flag
- `--include-scripts`
  - flag
  - 使用时必须同时提供 `--scene`

作用：

- 聚合 project/scene/asset/script 多域验证
- 统一得到一份正式 `ValidationReport` 结果语义

## 9. 推荐工作流

### 9.1 新建项目到最小可运行资产

```powershell
<CLI> project init --root D:/Workspace/MyGame --name MyGame
<CLI> asset register-default-mesh --project D:/Workspace/MyGame --asset-id builtin.quad
<CLI> scene create --project D:/Workspace/MyGame --name Main
<CLI> validation run --path D:/Workspace/MyGame --include-assets
```

### 9.2 场景和脚本 smoke

```powershell
<CLI> scene validate --project D:/Workspace/MyGame --scene main.scene
<CLI> script status --project D:/Workspace/MyGame --scene main.scene
<CLI> script initialize --project D:/Workspace/MyGame --scene main.scene
<CLI> script update --project D:/Workspace/MyGame --scene main.scene
<CLI> script shutdown --project D:/Workspace/MyGame --scene main.scene
```

### 9.3 自动化脚本判断建议

推荐脚本按这个顺序判断：

1. 先看进程退出码
2. 再看 `result.status`
3. 再看 `result.can_continue_automatically`
4. 最后按 `payload` 和 `details` 做分支处理

## 10. 常见注意点

- `ops list` 展示的是正式 OperationRegistry，不是 CLI 别名表
- `scene create` 的 CLI 命令会映射到正式 `scene.create`
- `asset register-default-mesh` 的正式 operation 名是 `asset.create_builtin_mesh`
- `validation run --include-scripts` 必须同时给 `--scene`
- CLI 当前输出契约是 JSON，不要混用日志文本去做机器解析
- 如果状态是 `manual_intervention_required`，脚本层应把它当成“需要停下来处理”的显式信号

## 11. 与 GUI 的关系

当前架构里：

- `Editor`
  - 是 GUI 宿主
- `HuaEngineCLI`
  - 是无 GUI 宿主

它们都消费同一套正式控制面：

- `ApplicationOperations`
- `ResultEnvelope`
- `ValidationReport`

所以：

- 先用 CLI 做自动化和 smoke
- 再用 GUI 做可视化验证

这是当前仓库推荐的使用方式。

## 12. Reflection 命令

Reflection 命令用于把源码里的反射标记转换成机器可消费的 manifest，并在需要时生成 C++ 反射元数据文件。CLI 只负责参数解析、结果包装和调用正式 operation；具体扫描、生成和校验逻辑由 `ReflectionToolService` 封装 `Tools/Reflection/reflection_tool.py` 完成。

当前命令会通过 `ApplicationOperations` 调用以下正式 operation：

- `reflection scan` -> `reflection.scan`
- `reflection generate` -> `reflection.generate`
- `reflection validate` -> `reflection.validate`

### 12.1 `reflection scan`

```powershell
<CLI> reflection scan --root D:/Workspace/HuaEngine
<CLI> reflection scan --root D:/Workspace/HuaEngine --out D:/Workspace/HuaEngine/.workspace/reflection/reflection_manifest.json
```

参数：

- `--root <path>`
  - 必填，仓库根目录。
  - `ReflectionToolService` 会在该目录下查找 `Tools/Reflection/reflection_tool.py`。
- `--out <manifest>`
  - 可选，manifest 输出路径。
  - 未指定时默认写入 `<root>/.workspace/reflection/reflection_manifest.json`。

作用：

- 扫描源码中的 `HE_REFLECT_COMPONENT` 和 `HE_REFLECT_FIELD` 标记。
- 写出 reflection manifest。
- 返回 payload 中常用字段：`root`、`manifest`、`output_directory`、`reflected_type_count`、`tool_output`。

### 12.2 `reflection generate`

```powershell
<CLI> reflection generate --root D:/Workspace/HuaEngine
<CLI> reflection generate --root D:/Workspace/HuaEngine --out-dir D:/Workspace/HuaEngine/HuaEngine/src/HuaEngine/Generated
<CLI> reflection generate --root D:/Workspace/HuaEngine --out D:/Workspace/HuaEngine/.workspace/reflection/reflection_manifest.json --out-dir D:/Workspace/HuaEngine/HuaEngine/src/HuaEngine/Generated
```

参数：

- `--root <path>`
  - 必填，仓库根目录。
- `--out <manifest>`
  - 可选，扫描阶段使用的 manifest 路径。
  - 未指定时默认写入 `<root>/.workspace/reflection/reflection_manifest.json`。
- `--out-dir <path>`
  - 可选，生成的 C++ 文件输出目录。
  - 未指定时默认写入 `<root>/HuaEngine/src/HuaEngine/Generated`。

作用：

- 先执行 scan，得到最新 manifest。
- 再调用 Python 工具的 generate 子命令生成 `GeneratedReflection.h` 和 `GeneratedReflection.cpp`。
- 如果 scan 阶段发现 error diagnostic，生成流程会失败并返回 failure。

### 12.3 `reflection validate`

```powershell
<CLI> reflection validate --root D:/Workspace/HuaEngine
```

参数：

- `--root <path>`
  - 必填，仓库根目录。

作用：

- 扫描源码并把校验 JSON 放入 `payload.tool_output`。
- 不写生成文件。
- 适合在 smoke、CI 或提交前检查反射标记是否仍能被工具解析。

脚本消费时建议同时检查进程退出码、`result.status` 和 `payload.reflected_type_count`。当前 smoke 期望仓库内可反射类型数量为 `5`。
