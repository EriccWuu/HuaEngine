---
name: huaengine-editor-workbench
description: >
  HuaEngine Editor 模块导航，覆盖 EditorApp、EditorLayer、Selection、ComponentEditorRegistry、
  SceneHierarchy/Inspector/Console 面板，以及 editor 如何消费 scene、rendering 和 core runtime 能力。
  Use when the user asks about Editor startup, docking layout, scene panel behavior, selection flow,
  inspector component editing, console panel logging, or where to modify HuaEngine editor behavior.
---

# HuaEngine Editor Workbench

## Overview

这个 Skill 用于定位 HuaEngine Editor 的工作台实现，适合回答“Editor 从哪启动”“Scene 面板怎么渲染”“Selection 怎么传到 Inspector”“组件编辑器为什么没显示”这类问题。

## 模块边界

- `Editor/src/EditorApp.cpp`：editor 进程入口与 `CreateApplication()` 实现
- `Editor/src/EditorLayer.*`：工作台主 Layer，负责 scene/framebuffer/panels 的装配
- `Editor/src/Panels/`：Scene Hierarchy、Inspector、Console 等 UI 面板
- `Editor/src/Selection.*`：全局选择状态
- `Editor/src/ComponentEditor*.h`：组件编辑器绘制和自动注册

## 核心子系统概览

- `EditorApp`：把 `EditorLayer` 压入 Application
- `EditorLayer`：构造 editor scene、camera、framebuffer、render system 和各面板
- `Selection`：静态当前选择实体
- `SceneHierarchyPanel`：基于当前 Scene 列出实体并驱动选择
- `InspectorPanel`：读取当前选择实体并走 `ComponentEditorRegistry`
- `ComponentEditorRegistry`：按组件类型注册绘制函数，并借反射自动画字段编辑器
- `ConcolePanel`：从 `LogSink` 缓冲读取日志并显示

## Core Rules

- Editor 启动链路是 `EditorApp -> PushLayer(new EditorLayer())`；工作台改动通常落在 `EditorLayer` 或具体 panel，而不是 `Application`。
- `EditorLayer::OnAttach()` 当前会创建测试场景、测试几何、shader、texture、framebuffer 和 `RenderSystem`；它不只是 UI 层，也是 editor 内置 demo scene 的装配点。
- Selection 是全局静态状态；Scene Hierarchy、Inspector 等面板不是通过复杂状态树同步，而是通过 `Selection::GetSelection()` 共享实体。
- `ComponentEditorRegistry` 依赖注册顺序遍历组件；组件是否在 Inspector 出现，不只看实体有没有该组件，还看它是否已经注册绘制器。
- `DrawComponentEditor(...)` 当前走反射字段遍历；组件字段如果没有进入反射信息，默认编辑器也不会自动画出来。
- `ConsolePanel` 的数据源来自 `LogSink` 缓冲，不是单独的 editor 日志系统。

## 关键入口文件

- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.h`
- `Editor/src/EditorLayer.cpp`
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

- 想理解 editor 启动、Layer 装配、DockSpace、Scene 面板和 framebuffer 驱动：读 `references/editor-flow.md`
- 想定位 Selection、SceneHierarchy、Inspector、组件自动注册与字段编辑：读 `references/panels-and-selection.md`
- 想看 Scene/Entity/Component 的底层事实：转到 `huaengine-ecs-scene`
- 想看 Scene 面板中的 camera/framebuffer/render system 链路：转到 `huaengine-rendering`
- 想看 Application/ImguiLayer/Log/Window 等共用 runtime glue：转到 `huaengine-core-runtime`

## Cross-Skill Navigation

- 如果 Editor 表面问题最终落在实体、组件、Scene 读写、系统注册或 Selection 对应实体事实，转到 `huaengine-ecs-scene`；先看 `references/runtime-structure.md`。
- 如果问题落在 Scene 面板渲染、Framebuffer resize、EditorCamera、RenderSystem 或材质/mesh 可见性，转到 `huaengine-rendering`；先看 `references/runtime-flow.md`。
- 如果问题落在主循环顺序、ImGui frame 边界、窗口事件、输入查询或 console 数据源，转到 `huaengine-core-runtime`；先看 `references/lifecycle-and-events.md`。
- 如果组件字段能在运行时存在但 Inspector 不会自动绘制，先检查 `huaengine-serialization-reflection` 中的反射规则，再回到本 Skill 看 `ComponentEditorRegistry`。

## Common Pitfalls

- `EditorLayer::OnAttach()` 里当前既有 UI 装配，也有 demo scene 初始化；改 editor 初始化逻辑时要区分“工具框架代码”和“示例内容代码”。
- `SceneHierarchyPanel` 仍然是按 `TransformComponent` 视图列实体，所以没有 Transform 的实体默认不会出现在树里。
- `Selection` 是静态全局状态；场景切换或实体失效后如果没清理，Inspector 可能继续拿旧实体包装。
- `InspectorPanel` 直接下钻 `selection.m_EntityManager->GetRegistry()`，封装边界并不强；改 Entity 内部实现时别只看 header 表面 API。
- `ConcolePanel`、`Concole` 等拼写在仓库里就是现状；搜索和重构时不要先假设已统一命名。