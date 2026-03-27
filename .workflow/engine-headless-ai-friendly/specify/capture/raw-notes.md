# Raw Notes

## Feature
- Feature slug: engine-headless-ai-friendly
- Proposed title: HuaEngine 无 GUI / AI 友好操作面
- Mode: standard
- Review method: independent agent (user mandated)

## Project Context
- HuaEngine 当前是 C++17 / CMake / OpenGL / GLFW / GLAD / EnTT / ImGui 的引擎原型。
- 当前存在 Editor 与 Sandbox 两个主要可视化宿主，很多验证路径仍偏向 GUI / demo scene。
- Scene、Serialization、Rendering、Editor 都已有骨架，但操作面主要偏向进程内对象和 GUI 面板消费。

## User Intent
- 用户希望引擎足够 AI 友好。
- 用户希望引擎在完全脱离 GUI 的情况下也能完成核心操作，体验形态类似 `git` / `p4` 这样的命令行系统。
- 用户希望从新的 workflow-specify 流程开始，作为独立需求能力重新定义。
- 用户要求所有审查环节统一使用独立 Agent。

## Stakeholders / User Roles
- 引擎维护者：希望所有核心能力不依赖 GUI 才能使用。
- AI 代理 / 自动化脚本作者：希望通过稳定 CLI / headless 接口执行引擎操作并拿到机器可读反馈。
- 游戏/内容开发者：希望在无 GUI 环境中也能完成项目、场景、资源和验证操作。
- Editor 用户：希望 GUI 成为可选客户端，而不是唯一入口。

## Functional Requirements (raw)
- 提供无需 GUI 的核心操作入口。
- 关键引擎操作可通过命令行 / 非交互方式执行。
- AI 代理可获得稳定、机器可读、可脚本化的执行结果。
- Scene / Asset / Script / Validation 等当前重点能力不能绑定在 GUI 才能使用。
- GUI 仍可存在，但不再是唯一操作面。

## Non-functional Requirements (raw)
- 命令稳定、输出可解析、适合自动化。
- 无 GUI 模式下操作应与 GUI 模式下保持结果一致。
- 非交互调用要可批处理、可组合、可定位失败原因。
- 能支持本地开发、CI、Agent 自动执行等环境。

## Constraints
- 现有项目上下文仍是 C++17 / CMake / OpenGL / JSON serialization / Editor + Sandbox。
- 当前项目已经有 GUI 工作台，但新需求要求 GUI 变成可选层。
- 这次规范只定义 WHAT，不定义 CLI 框架、协议、命令实现方式。

## Inferred Clarifications
- “AI 友好”在本需求里被解释为：非交互、可脚本化、机器可读、稳定反馈、可组合调用。
- “完全脱离 GUI 也能够操作”在本需求里被解释为：核心引擎操作和当前重点能力必须可在 headless / CLI 路径中完成，而不是必须保留 GUI 同步操作才能成立。
- 本需求不是要求删除 GUI，而是要求 GUI 不再是唯一控制面。

## Evidence from local context
- `huaengine-architecture`: Editor/Sandbox 目前是主要宿主，核心能力分布在 Core / Scene / Rendering / Serialization 中。
- `huaengine-core-runtime`: 当前主循环与生命周期围绕 Application/Layer/Window/Imgui 展开，说明运行入口仍偏 GUI app 宿主。
- `huaengine-editor-workbench`: EditorLayer 同时承担工作台和示例场景初始化，说明部分操作目前与 GUI 验证面强耦合。
- `huaengine-ecs-scene`: Scene / ScriptableEntity / NativeScriptComponent 已有结构，但脚本实际运行时能力尚未完全闭环。
- `huaengine-serialization-reflection`: Scene / Material / Mesh 的持久化链路已存在，说明无 GUI 操作面可以依托现有受控数据对象，但当前仍偏引擎内部语义。

## Ambiguity Assessment
- ambiguity_score: 0
- rationale:
  - 用户目标非常明确：无 GUI 可操作、AI 友好、CLI 类体验。
  - 关键模糊词已在上面转成可观察需求口径。
  - 不需要再停下来询问实现风格或具体命令形态。
