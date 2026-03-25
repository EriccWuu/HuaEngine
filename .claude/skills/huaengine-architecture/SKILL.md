---
name: huaengine-architecture
description: >
  HuaEngine 仓库整体架构导航，覆盖 CMake 构建图、HuaEngine/Editor/Sandbox 三目标关系、运行入口、
  引擎核心子系统分层，以及 ECS、渲染、序列化和反射之间的主链路。Use when the user asks about
  HuaEngine architecture, project structure, build targets, startup flow, subsystem boundaries,
  overall design, entry points, or where to modify engine/editor/sandbox behavior.
---

# HuaEngine Architecture

## Overview

这个 Skill 用于快速定位 HuaEngine 仓库的整体结构，适合回答“改动应该落在哪一层”“启动链路怎么走”“Editor 和 Sandbox 如何挂到引擎上”这类问题。

## 模块边界

- 根目录 `CMakeLists.txt` 负责整个解决方案的构建图、依赖接入和三个主目标注册
- `HuaEngine/` 是核心静态库，承载 Application、Core、ECS、Scene、Rendering、Serialization、Reflection 等主能力
- `Editor/` 是编辑器程序，依赖 `HuaEngine`，通过 `EditorLayer` 把场景面板、层级面板和 Inspector 挂进运行循环
- `Sandbox/` 是实验与验证程序，依赖 `HuaEngine`，适合验证渲染、材质、网格、场景序列化等引擎能力

## 核心子系统概览

- `Core`：应用生命周期、窗口、输入、LayerStack、日志
- `ECS + Scene`：基于 EnTT 管理实体和组件，`Scene` 维护 `EntityManager` 与 `System` 列表
- `Rendering`：Renderer、Buffer、Shader、Texture、FrameBuffer、Material、Mesh、RenderPipeline
- `Module/Rendering`：当前运行时渲染系统入口，`RenderSystem` 把 Scene 与 Camera 接到渲染层
- `Serialization + Reflection`：反射元信息通过 `srefl_class` 提供字段遍历，序列化层注册 JSON backend 并驱动场景/材质读写
- `GUI`：ImGui 集成和 Editor/Sandbox 的窗口绘制入口

## Core Rules

- 先看根 `CMakeLists.txt` 和三个子工程 `CMakeLists.txt`，再判断改动属于引擎库、编辑器程序还是 Sandbox 演示。
- 启动链路统一经过 `HuaEngine/EntryPoint.h` 的 `main()` 和 `CreateApplication()`；应用级行为优先在 `Application` 子类和对应 `Layer` 中找。
- 渲染问题不要只看 `Rendering/`，还要同时检查 `Module/Rendering/RenderSystem.*`、`Scene` 中的系统注册和具体 App 的相机/FrameBuffer 配置。
- 可序列化组件或数据结构变更时，要同时检查组件定义、`srefl_class(...)` 反射声明，以及序列化入口是否需要补充注册。
- 当前仓库存在历史命名拼写，如 `Syetem.h`、`AddSyetem(...)`、`FranmeBuffer.cpp`；排查问题时要按真实路径和真实 API 名称搜索，不要先假设已修正。

## 关键入口文件

- `CMakeLists.txt`
- `HuaEngine/CMakeLists.txt`
- `Editor/CMakeLists.txt`
- `Sandbox/CMakeLists.txt`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `HuaEngine/src/HuaEngine.h`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Sandbox/src/SandboxApp.cpp`
- `HuaEngine/src/HuaEngine/Scene/Scene.h`
- `HuaEngine/src/Module/Rendering/RenderSystem.h`
- `HuaEngine/src/HuaEngine/Serialization/Serialization.cpp`
- `HuaEngine/src/HuaEngine/Reflection/Reflection.h`

## Navigation

- 想先理解构建图、运行入口、子系统分层和改动影响范围：读 `references/architecture.md`
- 想直接构建、运行 Editor/Sandbox、确认输出目录和资源复制行为：读 `references/build-and-run.md`

## Cross-Skill Navigation

- 遇到 `Application`、`EntryPoint`、Layer/Overlay 顺序、Window/Input、事件分发、Log 或 ImGui runtime glue 问题时，转到 `huaengine-core-runtime`；生命周期先看 `references/lifecycle-and-events.md`，窗口输入和 ImGui 细节看 `references/window-input-and-imgui.md`。
- 遇到 Editor 启动、DockSpace、Selection、Inspector、Console 或组件编辑器问题时，转到 `huaengine-editor-workbench`；工作台装配先看 `references/editor-flow.md`，面板与选择流看 `references/panels-and-selection.md`。
- 遇到 `RenderSystem`、`CameraComponent`、FrameBuffer resize、材质参数、shader 或 mesh GPU 上传问题时，转到 `huaengine-rendering`；先看 `references/runtime-flow.md`，资源或材质问题再看 `references/assets-and-materials.md`。
- 遇到 `EntityManager`、`Scene`、`AddSyetem(...)`、Scene Hierarchy/Inspector、实体命名或场景保存加载问题时，转到 `huaengine-ecs-scene`；结构问题先看 `references/runtime-structure.md`，存档链路再看 `references/serialization-and-integration.md`。
- 遇到 `srefl_class(...)`、`Serializer<T>`、`ToJson/FromJson`、`InitializeSerialization()`、JSON backend 或 GLM/`Ref<T>` 序列化问题时，转到 `huaengine-serialization-reflection`；核心机制先看 `references/core-flow.md`，扩展新类型再看 `references/extension-and-integration.md`。
- 如果还没判断清楚问题属于哪个模块，先留在本 Skill 的 `references/architecture.md` 做一级分层，再按上面几条跳转。

## Common Pitfalls

- `Editor` 和 `Sandbox` 都不自己实现主循环，它们只是提供 `CreateApplication()` 和具体 `Layer`。
- `Scene::Update()` 只遍历已注册的系统；如果新系统没有通过 `AddSyetem(...)` 挂进去，逻辑不会执行。
- 组件是否“可存盘”不只取决于结构体字段，还取决于是否进入反射和序列化链路。
