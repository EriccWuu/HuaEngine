---
name: huaengine-core-runtime
description: >
  HuaEngine 核心运行时模块导航，覆盖 Application、EntryPoint、ApplicationServices/ApplicationOperations、
  Layer/LayerStack、Window/Input、Event、Log 与 ImGui glue。Use when the user asks about application lifecycle,
  event dispatch, host startup, window callbacks, input polling, layer ordering, logging, ImGui integration,
  or where to modify HuaEngine core runtime behavior.
---

# HuaEngine Core Runtime

## Overview

这个 Skill 用于定位 HuaEngine 的核心运行时骨架，适合回答“主循环怎么驱动”“`Start()` 和 `Run()` 的边界是什么”“headless host 如何共享同一套 runtime”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/Application.*`、`HuaEngine/src/HuaEngine/Application/` 与 `EntryPoint.h`：应用生命周期、运行时服务与共享入口
- `HuaEngine/src/HuaEngine/Core/`：Layer、LayerStack、Window、Input、Log、断言和基础类型
- `HuaEngine/src/HuaEngine/Events/`：事件基类和应用/键鼠事件
- `HuaEngine/src/HuaEngine/GUI/`：`ImguiLayer` 与 ImGui 集成入口
- `HuaEngine/src/Platform/Windows/`：Window/Input 的 Windows/GLFW 实现

## 核心子系统概览

- `Application`：单例应用对象、窗口创建、事件入口、主循环、LayerStack 管理
- `ApplicationServices + ApplicationOperations`：runtime 内部组合根与宿主唯一公开操作面
- `Layer / LayerStack`：层与 Overlay 的生命周期和顺序控制
- `Event / EventDispatcher`：事件类型、分类和分发工具
- `Window / WindowsWindow`：窗口抽象、GLFW 回调接线、swap/poll/update
- `Input / WindowsInput`：静态查询入口，运行时从当前窗口拉取状态
- `Log / LogSink`：核心日志、客户端日志和 editor console 共用缓冲
- `ImguiLayer`：ImGui attach/begin/end，挂在主循环里的 GUI Overlay

## Core Rules

- 统一启动链路现在是 `main() -> Log::Init() -> CreateApplication() -> Application::Start()/Run()`
- `Application` 构造本身不再隐式做 runtime side effects；窗口、序列化、GUI layer、服务注册和操作层创建都在 `Start()` 内完成
- `ApplicationSpecification::EnableWindow / EnableGuiLayer` 只决定宿主外壳，不改变内部服务层与结果语义
- `LayerStack` 中普通 Layer 插在前面，Overlay 插在末尾，`Application::OnEvent()` 会按逆序把事件分发给层
- `Input` 是静态门面，但底层实现依赖 `Application::GetInstance().GetWindow()`；在无窗口宿主里不能把它当成通用输入源
- `WindowsWindow` 负责把 GLFW 回调包装成 HuaEngine 事件对象并立即回调给 `Application`
- `ImguiLayer::End()` 在多 viewport 模式下会保存和恢复 GLFW 当前上下文；涉及平台窗口或 GL context 变更时要一并考虑

## 关键入口文件

- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `HuaEngine/src/HuaEngine/Core/Layer.h`
- `HuaEngine/src/HuaEngine/Core/LayerStack.h`
- `HuaEngine/src/HuaEngine/Core/LayerStack.cpp`
- `HuaEngine/src/HuaEngine/Core/Window.h`
- `HuaEngine/src/HuaEngine/Core/Input.h`
- `HuaEngine/src/HuaEngine/Core/Log.h`
- `HuaEngine/src/HuaEngine/Core/Log.cpp`
- `HuaEngine/src/HuaEngine/Events/Event.h`
- `HuaEngine/src/HuaEngine/GUI/ImguiLayer.cpp`
- `HuaEngine/src/Platform/Windows/WindowsWindow.cpp`
- `HuaEngine/src/Platform/Windows/WindowsInput.cpp`

## Navigation

- 想理解启动链路、Layer 顺序、事件分发和 runtime 初始化边界：读 `references/lifecycle-and-events.md`
- 想定位窗口回调、输入查询、GLFW 依赖与 ImGui 带来的边界：读 `references/window-input-and-imgui.md`
- 想看 Editor 如何消费这些 runtime 能力：转到 `huaengine-editor-workbench`
- 想看 Scene/System 如何挂到主循环：转到 `huaengine-ecs-scene`
- 想看渲染提交流程如何被每帧驱动：转到 `huaengine-rendering`

## Cross-Skill Navigation

- 如果问题已经进入 Scene、系统注册、实体更新或编辑器层级树可见性：转到 `huaengine-ecs-scene`
- 如果问题发生在 Scene 面板、Inspector、Console、Selection 或 EditorLayer 的 Docking/FrameBuffer 管理：转到 `huaengine-editor-workbench`
- 如果 runtime 层改动影响相机、FrameBuffer、RenderSystem 或渲染主路径：转到 `huaengine-rendering`
- 如果是 `Application` 启动期的序列化初始化、`ToJson/FromJson`、backend 注册问题：转到 `huaengine-serialization-reflection`

## Common Pitfalls

- `ImguiLayer` 已经由 `Application` 自动创建并作为 Overlay 压栈；Editor 不需要也不应该再造一套全局 GUI layer
- headless host 不走 window loop，但仍共享同一套 `Start()`、services 和 operations 初始化边界
- 事件是逆序发给 Layer 的，后压入的 Overlay 会先看到事件；排查“事件被吃掉”时先看后面的层
- `WindowsWindow` 里维护了 `ms_GLFWWindowCount`，涉及多窗口扩展时要额外核对这段生命周期逻辑
- `ConsolePanel` 依赖 `LogSink` 缓冲；如果 `Log::Init()` 没跑，GUI console 也不会有可靠数据源
