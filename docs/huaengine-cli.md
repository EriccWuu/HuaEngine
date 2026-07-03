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

## 6. 命令总览

| 命令 | 作用 | 备注 |
|---|---|---|
| `help` | 输出帮助摘要 | 不接受选项 |
| `ops list` | 列出正式 operation registry | `data.operations` 返回列表 |
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

## 7. 命令详解

### 7.1 `help`

```powershell
<CLI> help
```

用途：

- 查看 CLI 摘要
- 快速确认当前宿主可执行

### 7.2 `ops list`

```powershell
<CLI> ops list
```

用途：

- 列出当前正式 operation registry
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

### 7.3 `project init`

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

### 7.4 `project status`

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

### 7.5 `scene create`

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

### 7.6 `scene validate`

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

### 7.7 `asset register-default-mesh`

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

### 7.8 `asset validate`

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

### 7.9 `script status`

```powershell
<CLI> script status --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 读取场景
- 汇总脚本绑定数量、启用数量、活动实例数量、缺绑定数量

注意：

- 这一步只检查当前场景里的脚本状态
- 不会自动初始化脚本实例

### 7.10 `script initialize`

```powershell
<CLI> script initialize --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再为场景创建脚本实例

### 7.11 `script update`

```powershell
<CLI> script update --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再推进一轮脚本更新

适合：

- 单步 smoke
- 无 GUI 逻辑验证

### 7.12 `script shutdown`

```powershell
<CLI> script shutdown --project D:/Workspace/MyGame --scene gameplay/main.scene
```

作用：

- 先附着 script runtime
- 再销毁活动脚本实例

### 7.13 `validation run`

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

## 8. 推荐工作流

### 8.1 新建项目到最小可运行资产

```powershell
<CLI> project init --root D:/Workspace/MyGame --name MyGame
<CLI> asset register-default-mesh --project D:/Workspace/MyGame --asset-id builtin.quad
<CLI> scene create --project D:/Workspace/MyGame --name Main
<CLI> validation run --path D:/Workspace/MyGame --include-assets
```

### 8.2 场景和脚本 smoke

```powershell
<CLI> scene validate --project D:/Workspace/MyGame --scene main.scene
<CLI> script status --project D:/Workspace/MyGame --scene main.scene
<CLI> script initialize --project D:/Workspace/MyGame --scene main.scene
<CLI> script update --project D:/Workspace/MyGame --scene main.scene
<CLI> script shutdown --project D:/Workspace/MyGame --scene main.scene
```

### 8.3 自动化脚本判断建议

推荐脚本按这个顺序判断：

1. 先看进程退出码
2. 再看 `result.status`
3. 再看 `result.can_continue_automatically`
4. 最后按 `payload` 和 `details` 做分支处理

## 9. 常见注意点

- `ops list` 展示的是正式 operation 名，不是 CLI 别名表
- `scene create` 的 CLI 命令会映射到正式 `scene.create`
- `asset register-default-mesh` 的正式 operation 名是 `asset.create_builtin_mesh`
- `validation run --include-scripts` 必须同时给 `--scene`
- CLI 当前输出契约是 JSON，不要混用日志文本去做机器解析
- 如果状态是 `manual_intervention_required`，脚本层应把它当成“需要停下来处理”的显式信号

## 10. 与 GUI 的关系

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
