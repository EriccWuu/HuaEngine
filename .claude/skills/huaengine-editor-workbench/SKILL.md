---
name: huaengine-editor-workbench
description: >
  HuaEngine Editor 模块导航，覆盖 EditorApp、EditorLayer、EditorWorkbenchState、Selection、
  ComponentEditorRegistry，以及 SceneHierarchy/Inspector/Console 面板如何消费统一操作层与统一结果语义。
  Use when the user asks about Editor startup, docking layout, scene panel behavior, selection flow,
  inspector component editing, console diagnostics, or where to modify HuaEngine editor behavior.
---

# HuaEngine Editor Workbench

## Overview

这个 Skill 用于定位 HuaEngine Editor 的工作台实现，适合回答“Editor 从哪启动”“GUI 怎么消费统一操作层”“SceneHierarchy/Inspector/Console 为什么显示了某种状态”这类问题。

## 模块边界

- `Editor/src/EditorApp.cpp`：Editor 进程入口与 `CreateApplication()` 实现
- `Editor/src/EditorLayer.*`：工作台主 Layer，负责 scene/framebuffer/panels 装配，并通过 `ApplicationOperations` 消费正式控制面
- `Editor/src/Panels/`：Scene Hierarchy、Inspector、Console 等 UI 面板
- `Editor/src/Workbench/`：Editor 侧统一结果/诊断缓存与 GUI 映射状态
- `Editor/src/Selection.*`：全局选择状态
- `Editor/src/ComponentEditor*.h`：组件编辑器绘制和自动注册

## 核心子系统概览

- `EditorApp`：把 `EditorLayer` 压入 `Application`
- `EditorLayer`：初始化 workbench project/scene、viewport shell、render seam 和各面板
- `EditorWorkbenchState`：缓存最近一次正式操作、最近一次 validation 结果与事件历史
- `Selection`：静态当前选择实体
- `SceneHierarchyPanel`：基于当前 Scene 列出实体、显示最近一次操作摘要并驱动选择
- `InspectorPanel`：读取当前选择实体，并在顶部显示最近一次 validation 状态
- `ComponentEditorRegistry`：按组件类型注册绘制函数，并借反射自动画字段编辑器
- `ConcolePanel`：同时消费 `EditorWorkbenchState` 里的正式诊断结果和 `LogSink` 日志缓冲

## Core Rules

- Editor 启动链路是 `EditorApp -> PushLayer(new EditorLayer(spec))`
- `EditorLayer::OnAttach()` 先初始化 workbench context，再初始化 viewport shell，最后按 `EditorLayerSpecification` 决定是否 bootstrap 默认示例场景
- Editor 已不再默认承担 domain orchestration 入口职责；项目、场景、校验、render seam 都优先沿 `ApplicationOperations` 走
- Selection 是全局静态状态；Scene Hierarchy、Inspector 等面板不是通过复杂状态树同步，而是通过 `Selection::GetSelection()` 共享实体
- `ComponentEditorRegistry` 依赖注册顺序遍历组件；组件是否在 Inspector 出现，不只看实体有没有该组件，还看它是否已注册绘制器
- `DrawComponentEditor(...)` 当前走反射字段遍历；字段没进反射信息，默认编辑器也不会自动画出来
- GUI 错误反馈优先沿 `ResultEnvelope / ValidationReport / EditorWorkbenchState` 排查，而不是只看零散日志

## 关键入口文件

- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.h`
- `Editor/src/EditorLayer.cpp`
- `Editor/src/Workbench/EditorWorkbenchState.h`
- `Editor/src/Selection.h`
- `Editor/src/Selection.cpp`
- `Editor/src/ComponentEditor.h`
- `Editor/src/ComponentEditorRegistry.h`
- `Editor/src/Panels/SceneHierarchyPanel.h`
- `Editor/src/Panels/SceneHierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.h`
- `Editor/src/Panels/InspectorPanel.cpp`
- `Editor/src/Panels/ConsolePanel.h`
- `Editor/src/Panels/ConsolePanel.cpp`

## Navigation

- 想理解 editor 启动、Layer 装配、DockSpace、Scene 面板与 framebuffer 驱动：读 `references/editor-flow.md`
- 想定位 Selection、SceneHierarchy、Inspector、Console 与组件编辑器：读 `references/panels-and-selection.md`
- 想看 Scene/Entity/Component 的底层事实：转到 `huaengine-ecs-scene`
- 想看 Scene 面板中的 camera/framebuffer/render system 链路：转到 `huaengine-rendering`
- 想看 `Application`、`ImguiLayer`、`Log`、`Window` 等 runtime glue：转到 `huaengine-core-runtime`

## Cross-Skill Navigation

- 如果 Editor 表面问题最终落在实体、组件、Scene 读写、系统注册或 Selection 对应实体事实上：转到 `huaengine-ecs-scene`
- 如果问题落在 Scene 面板渲染、FrameBuffer resize、EditorCamera、RenderSystem 或材质/mesh 可见性：转到 `huaengine-rendering`
- 如果问题落在主循环顺序、ImGui frame 边界、窗口事件、输入查询或 console 数据源：转到 `huaengine-core-runtime`
- 如果组件字段能在运行时存在但 Inspector 不会自动绘制，先检查 `huaengine-serialization-reflection` 中的反射规则

## Common Pitfalls

- `EditorLayer` 现在区分“工作台壳”和“可选默认场景 bootstrap”；当前默认 bootstrap 已切到 Sandbox 风格的 `MeshComponent + MaterialComponent` 场景，不再使用旧的 `RendererComponent` 示例
- `OnDockingPanel()` 会在首次进入时主动建立默认布局：左 `Scene Hierarchy`、右 `Inspector`、下 `Console`、中 `Scene`；排查布局问题时先确认是不是这段 DockBuilder 逻辑或已有布局状态在生效
- `SceneHierarchyPanel` 仍然按 `TransformComponent` 视图列实体，没有 `Transform` 的实体默认不会出现在树里
- `Selection` 是静态全局状态；场景切换或实体失效后如果没清理，Inspector 可能继续拿旧实体包装
- `InspectorPanel` 仍会直接下钻 `selection.m_EntityManager->GetRegistry()`；改 `Entity` 封装时别只看 header 表面 API
- `ConcolePanel` / `Concole` 的拼写在仓库里就是现状；搜索和重构时不要先假设已统一命名
