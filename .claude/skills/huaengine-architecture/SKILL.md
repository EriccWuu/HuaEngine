---
name: huaengine-architecture
description: >
  HuaEngine 仓库整体架构导航，覆盖构建图、HuaEngine/Editor/Sandbox/Headless 四类目标关系、
  启动入口、Application Service Layer、统一结果协议，以及 ECS、渲染、序列化与宿主边界之间的主链路。
  Use when the user asks about HuaEngine architecture, project structure, build targets, startup flow,
  subsystem boundaries, host boundaries, overall design, entry points, or where to modify engine/editor/headless behavior.
---

# HuaEngine Architecture

## Overview

这个 Skill 用于快速定位 HuaEngine 仓库的整体结构，适合回答“改动应该落在哪一层”“当前正式控制面是什么”“Headless/GUI/Agent 分别怎么接引擎”这类问题。

## 模块边界

- 根目录 `CMakeLists.txt` 负责整个解决方案的构建图、依赖接入，以及 `HuaEngine / Editor / Sandbox / HuaEngineHeadless` 和 smoke targets 的注册
- `HuaEngine/` 是核心静态库，承载 runtime、services、operations、ECS、Scene、Rendering、Serialization、Reflection 等主能力
- `Editor/` 是 GUI 宿主，当前通过 `EditorLayer` + `ApplicationOperations` 消费统一操作层
- `Sandbox/` 是实验与验证宿主，适合验证渲染、材质、网格、场景序列化等引擎能力
- `Headless/` 是正式无 GUI 宿主，复用同一套 runtime/service/operation contracts，输出结构化 JSON 结果

## 核心子系统概览

- `Core`：应用生命周期、窗口、输入、LayerStack、日志
- `Application Service Layer`：`ApplicationServices + ApplicationOperations + OperationRegistry + ResultEnvelope`
- `ECS + Scene`：基于 EnTT 管理实体和组件，`Scene` 维护 `EntityManager` 与 `System` 列表
- `Rendering`：Renderer、Buffer、Shader、Texture、Framebuffer、Material、Mesh、RenderPipeline
- `Module/Rendering`：当前运行时渲染系统入口，`RenderSystem` 把 Scene 中 Camera 接到渲染层
- `Serialization + Reflection`：反射元信息通过 `srefl_class` 提供字段遍历，序列化层注册 JSON backend 并驱动场景/材质读写
- `Automation / Headless`：`AgentHostAdapter` 和 `HuaEngineHeadless` 共享统一操作层，不允许宿主直连 domain services

## Core Rules

- 先看根 `CMakeLists.txt` 与宿主入口，再判断改动属于引擎库、Editor、Sandbox 还是 Headless
- 启动链路统一经过 `HuaEngine/EntryPoint.h` 的 `main()`、`CreateApplication()` 和 `Application::Start()/Run()`
- 宿主正式能力入口优先沿 `Application::GetOperations()` 往下找，不要默认去找 raw services
- 渲染问题不要只看 `Rendering/`，还要同时检查 `Module/Rendering/RenderSystem.*`、宿主侧相机/Framebuffer 配置和操作层 rendering seam
- 可序列化结构变更时，要同时检查组件定义、`srefl_class(...)` 反射声明，以及序列化入口是否需要补注册
- 当前仓库存在历史命名拼写，如 `Syetem.h`、`AddSyetem(...)`、`FranmeBuffer.cpp`；排查时按真实路径搜索
- 当前规划要求 GUI、Headless、Agent 都只能消费统一操作层；如果改动试图重新暴露 raw services 或让宿主直接操作 domain 对象，默认视为架构回退

## 关键入口文件

- `CMakeLists.txt`
- `HuaEngine/CMakeLists.txt`
- `Editor/CMakeLists.txt`
- `Sandbox/CMakeLists.txt`
- `Headless/CMakeLists.txt`
- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/Application/OperationRegistry.h`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `HuaEngine/src/HuaEngine.h`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Headless/src/main.cpp`
- `Headless/src/HeadlessCommandRunner.cpp`
- `Sandbox/src/SandboxApp.cpp`

## Navigation

- 想先理解构建图、运行入口、宿主边界和控制面：读 `references/architecture.md`
- 想直接构建、运行 Editor/Sandbox/Headless，或查看 smoke targets：读 `references/build-and-run.md`

## Cross-Skill Navigation

- 遇到 `Application`、`EntryPoint`、Layer/Overlay 顺序、Window/Input、事件分发、Log 或 ImGui runtime glue：转到 `huaengine-core-runtime`
- 遇到 Editor 启动、DockSpace、Selection、Inspector、Console 或 GUI 如何消费统一结果语义：转到 `huaengine-editor-workbench`
- 遇到 `RenderSystem`、`FrameBuffer`、材质参数、shader、mesh GPU 上传或 rendering seam：转到 `huaengine-rendering`
- 遇到 `EntityManager`、`Scene`、`AddSyetem(...)`、Scene Hierarchy、实体命名或场景保存加载：转到 `huaengine-ecs-scene`
- 遇到 `srefl_class(...)`、`Serializer<T>`、`ToJson/FromJson`、`InitializeSerialization()` 或 JSON backend：转到 `huaengine-serialization-reflection`

## Common Pitfalls

- `Editor`、`Headless`、`Sandbox` 都不应该再各自扩出独立 domain API；它们只是不同宿主
- `Scene::Update()` 只遍历已注册的系统；如果系统没通过 `AddSyetem(...)` 挂进去，逻辑不会执行
- 组件是否“可存盘”不只取决于结构体字段，还取决于它是否进入反射和序列化链路
- `HuaEngineHeadless` 的 JSON stdout 才是正式机器可读接口；日志只是补充排障信息
