---
name: huaengine-development-guidelines
description: >
  HuaEngine 开发规范导航。固化宿主分工、正式操作面、查询边界、工作台状态模型、
  序列化与资源约束、测试与文档同步要求。适用于回答开发约束、接入边界、改动落点和实现前检查项这类问题。当要修改，或开发功能时，需要遵循该skill描述的内容。
---

# HuaEngine 开发规范

## 概览

这个 Skill 用来固化当前仓库的开发约束。
当要修改，或开发功能时，需要优先遵循这里描述的规则，而不是按单点需求临时扩散实现。

## 适用范围

- `HuaEngine/` 核心引擎库
- `ProjectHub/` 项目启动器
- `Editor/` 项目工作台
- `Headless/` CLI 与自动化宿主
- `Tests/` smoke 与回归验证
- `Resources/` 引擎开发态共享资源

## 核心规则

- 宿主要薄，宿主只负责自己的交互形式，不负责发明业务能力
- 共享写操作必须统一走 `ApplicationOperations`
- GUI 与 Headless 必须共用同一套结果语义，不允许各长一套
- `Editor` 的实时只读渲染路径可以直接读运行时状态，但共享查询应逐步收敛为正式查询面
- 新增正式能力时，代码、smoke、文档和模块 Skill 必须同步

## 关键文件

- `docs/development-guidelines.md`
- `docs/refactored-engine-architecture.md`
- `docs/editor-project-workbench.md`
- `docs/huaengine-headless-cli.md`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `Editor/src/Workbench/ProjectSession.h`
- `Editor/src/Workbench/SceneDocument.h`
- `Editor/src/Workbench/EditorWorkbenchState.h`

## 导航

- 看完整规范正文、分层边界、禁止事项和检查表：读 `references/guidelines.md`
- 看整体分层与宿主拓扑：转 `huaengine-architecture`
- 看 Editor 工作台状态模型和交互约束：转 `huaengine-editor-workbench`
- 看 ECS / Scene 的共享写边界：转 `huaengine-ecs-scene`
- 看序列化 schema 与资源约束：转 `huaengine-serialization-reflection`

## 常见误区

- 不要因为当前功能只在 `Editor` 里触发，就绕开正式操作面直接写一套 GUI 私有逻辑
- 不要把每帧 GUI 读取的例外放大成“所有查询都不用统一”
- 不要在没有 smoke 和文档同步的情况下引入新的正式能力
- 不要为了兼容临时数据重新扩散旧 schema
