---
name: huaengine-core-runtime
description: >
  HuaEngine 核心运行时模块导航，覆盖 Application、EntryPoint、Layer/LayerStack、Window/Input、Event 系统、
  Log 与 ImguiLayer 的主循环接线。Use when the user asks about application lifecycle, event dispatch,
  window callbacks, input polling, layer ordering, logging, ImGui frame integration, or where to modify
  HuaEngine core runtime behavior.
---

# HuaEngine Core Runtime

## Overview

这个 Skill 用于定位 HuaEngine 的核心运行时骨架，适合回答“主循环怎么驱动”“事件怎么分发”“窗口和输入怎么接 GLFW”“ImguiLayer 怎么插进每帧流程”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/Application.*` 与 `EntryPoint.h`：应用生命周期与共享入口
- `HuaEngine/src/HuaEngine/Core/`：Layer、LayerStack、Window、Input、Log、断言和基础类型
- `HuaEngine/src/HuaEngine/Events/`：事件基类和应用/键鼠事件
- `HuaEngine/src/HuaEngine/GUI/`：`ImguiLayer` 与 ImGui 集成入口
- `HuaEngine/src/Platform/Windows/`：Window 与 Input 的 Windows/GLFW 实现

## 核心子系统概览

- `Application`：单例应用对象、窗口创建、事件入口、主循环、LayerStack 管理
- `Layer` / `LayerStack`：层与 Overlay 的生命周期和顺序控制
- `Event` / `EventDispatcher`：事件类型、分类和分发工具
- `Window` / `WindowsWindow`：窗口抽象、GLFW 回调接线、swap/poll/update
- `Input` / `WindowsInput`：静态查询入口，运行时从当前窗口拉取状态
- `Log` / `LogSink`：核心日志、客户端日志和 editor console 共用缓冲
- `ImguiLayer`：ImGui attach/begin/end，挂在主循环里的 GUI Overlay

## Core Rules

- 统一启动链路是 `main() -> Log::Init() -> CreateApplication() -> Application::Run()`；大部分 runtime 改动都要先判断是在 app 构造阶段、层生命周期还是每帧循环里发生。
- `Application` 构造时会先初始化序列化、再创建窗口、再把 `ImguiLayer` 作为 Overlay 压入 LayerStack；不要假设 GUI 层由 Editor 自己创建。
- `LayerStack` 中普通 Layer 插在前面，Overlay 插在末尾；`Application::OnEvent()` 会按逆序把事件分发给层。
- `Input` 是静态门面，但底层实现依赖 `Application::GetInstance().GetWindow()`；在 app 未完成初始化前不能把它当成独立服务使用。
- `WindowsWindow` 负责把 GLFW 回调包装成 HuaEngine 事件对象并立即回调给 `Application`；窗口层行为改动优先从这里落手。
- `ImguiLayer::End()` 在多 viewport 模式下会保存和恢复 GLFW 当前上下文；涉及平台窗口或 GL context 变更时，必须把这段逻辑一起考虑进去。

## 关键入口文件

- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `HuaEngine/src/HuaEngine/Core/Layer.h`
- `HuaEngine/src/HuaEngine/Core/LayerStack.h`
- `HuaEngine/src/HuaEngine/Core/LayerStack.cpp`
- `HuaEngine/src/HuaEngine/Core/Window.h`
- `HuaEngine/src/HuaEngine/Core/Input.h`
- `HuaEngine/src/HuaEngine/Core/Log.h`
- `HuaEngine/src/HuaEngine/Core/Log.cpp`
- `HuaEngine/src/HuaEngine/Events/Event.h`
- `HuaEngine/src/HuaEngine/Events/ApplicationEvent.h`
- `HuaEngine/src/HuaEngine/Events/KeyEvent.h`
- `HuaEngine/src/HuaEngine/Events/MouseEvent.h`
- `HuaEngine/src/HuaEngine/GUI/ImguiLayer.h`
- `HuaEngine/src/HuaEngine/GUI/ImguiLayer.cpp`
- `HuaEngine/src/Platform/Windows/WindowsWindow.cpp`
- `HuaEngine/src/Platform/Windows/WindowsInput.cpp`

## Navigation

- 想理解启动链路、Layer 顺序、事件分发和日志接线：读 `references/lifecycle-and-events.md`
- 想定位窗口回调、输入查询、GLFW 依赖与 ImGui 帧边界：读 `references/window-input-and-imgui.md`
- 想看 Editor 如何消费这些 runtime 服务：转到 `huaengine-editor-workbench`
- 想看 Scene/System 如何挂到主循环：转到 `huaengine-ecs-scene`
- 想看渲染提交如何被每帧驱动：转到 `huaengine-rendering`

## Cross-Skill Navigation

- 如果问题已经进入 Scene、系统注册、实体更新或编辑器层级树可见性，转到 `huaengine-ecs-scene`；结构先看 `references/runtime-structure.md`。
- 如果问题发生在 Scene 面板、Inspector、Console、Selection 或 EditorLayer 的 Docking/FrameBuffer 管理，转到 `huaengine-editor-workbench`；先看 `references/editor-flow.md`。
- 如果 runtime 层改动影响相机、FrameBuffer、RenderSystem 或渲染主路径，转到 `huaengine-rendering`；先看 `references/runtime-flow.md`。
- 如果是 `Application` 构造时的序列化初始化、`ToJson/FromJson`、backend 注册问题，转到 `huaengine-serialization-reflection`；先看 `references/core-flow.md`。

## Common Pitfalls

- `ImguiLayer` 已经由 `Application` 自动创建并作为 Overlay 压栈；Editor 不需要也不应该再造一套全局 GUI 层。
- 事件是逆序发给 Layer 的，后压入的 Overlay 会先看到事件；排查“事件被吃掉”时先看后面的层。
- `WindowsInput` 直接从当前 GLFW window 查询状态，所以它反映的是即时状态，不是事件队列状态。
- `WindowsWindow` 里维护了 `ms_GLFWWindowCount`，但当前 `Init()` 没有对应自增；涉及窗口销毁或多窗口扩展时要额外核对这段生命周期逻辑。
- `ConsolePanel` 依赖 `LogSink` 缓冲；如果 `Log::Init()` 没跑，editor console 也不会有可靠数据源。