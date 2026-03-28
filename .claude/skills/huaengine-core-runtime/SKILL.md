---
name: huaengine-core-runtime
description: >
  HuaEngine 核心运行时导航。覆盖 Application、EntryPoint、
  ApplicationServices/ApplicationOperations、Layer/LayerStack、Window/Input、
  Events、Log、宿主拉起支持和 ImGui 胶水层。适用于回答应用生命周期、事件派发、
  宿主启动、窗口尺寸、输入轮询、层顺序、日志和运行时修改落点这类问题。
---

# HuaEngine 核心运行时

## 概览

这个 Skill 是 HuaEngine 运行时生命周期和宿主启动行为的主导航入口。
当问题是：

- `Application::Start()` 和 `Run()` 的区别
- 多宿主共享的运行时边界
- 窗口创建和输入轮询
- 日志、Layer、Event、ImGui 胶水层的位置

优先从这里进入。

## 模块边界

- `HuaEngine/src/HuaEngine/Application.*` 与 `Application/`
  - 应用生命周期、服务、操作面、注册表
- `HuaEngine/src/HuaEngine/EntryPoint.h`
  - 共享宿主入口
- `HuaEngine/src/HuaEngine/Core/`
  - `Layer`、`LayerStack`、`Window`、`Input`、`Log`、`HostLaunch`
- `HuaEngine/src/HuaEngine/Events/`
  - 事件定义与派发
- `HuaEngine/src/HuaEngine/GUI/`
  - `ImguiLayer`
- `HuaEngine/src/Platform/Windows/`
  - Windows / GLFW 的窗口和输入实现

## 当前运行时模型

- 共享启动链是 `main() -> Log::Init() -> CreateApplication() -> Application::Start()/Run()`
- `Application` 构造阶段应尽量轻，真正运行时副作用放在 `Start()`
- `ApplicationServices` 是内部组合根
- `ApplicationOperations` 是正式宿主控制面
- `ApplicationSpecification` 控制宿主 shell 形态：
  - `EnableWindow`
  - `EnableGuiLayer`
  - `WindowWidth`
  - `WindowHeight`
- `HostLaunch` 负责宿主之间的进程拉起，例如 `ProjectHub.exe -> Editor.exe`
- `Log` 现在会写入控制台 / 内存 sink，以及 `%LOCALAPPDATA%/HuaEngine/Logs/` 下的宿主日志文件

## 核心规则

- 宿主应通过 `Application::GetOperations()` 暴露能力，而不是重新暴露 raw service
- `ApplicationSpecification` 只配置宿主 shell，不重定义领域语义
- 有窗口宿主通过 `WindowProps(Name, WindowWidth, WindowHeight)` 创建窗口
- `ProjectHub.exe` 是启动器 shell，不是完整工作台
- `LayerStack` 普通层在前，overlay 在后，事件按逆序派发
- 无窗口宿主中的 `Input` 不是普适能力，因为它依赖活动窗口
- `ImguiLayer` 由 `Application` 为 GUI 宿主统一创建，下游 GUI 宿主不应再建第二套全局 ImGui layer

## 关键文件

- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `HuaEngine/src/HuaEngine/Core/HostLaunch.h`
- `HuaEngine/src/HuaEngine/Core/HostLaunch.cpp`
- `HuaEngine/src/HuaEngine/Core/Window.h`
- `HuaEngine/src/HuaEngine/Core/Input.h`
- `HuaEngine/src/HuaEngine/Core/Log.h`
- `HuaEngine/src/HuaEngine/Core/Log.cpp`
- `HuaEngine/src/HuaEngine/Events/Event.h`
- `HuaEngine/src/HuaEngine/GUI/ImguiLayer.cpp`
- `HuaEngine/src/Platform/Windows/WindowsWindow.cpp`
- `HuaEngine/src/Platform/Windows/WindowsInput.cpp`

## 导航

- 看启动流、层顺序、事件派发、初始化边界：读 `references/lifecycle-and-events.md`
- 看窗口回调、输入轮询、GLFW 行为、ImGui 平台胶水层：读 `references/window-input-and-imgui.md`
- 看 `ProjectHub` / `Editor` 分离和工作台入口：转 `huaengine-editor-workbench`
- 看场景 / 系统更新流：转 `huaengine-ecs-scene`
- 看视口渲染和帧更新：转 `huaengine-rendering`

## 跨 Skill 导航

- 如果问题本质上是 `ProjectHub`、会话恢复、Project 面板或工作台组合：转 `huaengine-editor-workbench`
- 如果问题是场景 / 系统注册或场景更新行为：转 `huaengine-ecs-scene`
- 如果运行时改动影响相机、FrameBuffer 或渲染提交：转 `huaengine-rendering`
- 如果问题落在序列化初始化或 backend 注册：转 `huaengine-serialization-reflection`

## 常见误区

- `Application` 不再代表“所有宿主默认一个窗口形态”；窗口尺寸来自 `ApplicationSpecification`
- `ProjectHub.exe` 天生比 `Editor` 小，它就是启动器外壳
- `HostLaunch` 只处理进程拉起，不是富 IPC 协议
- `ConsolePanel` 依赖 `Log::Init()` 和共享日志 sink；如果日志没初始化，GUI 诊断会不完整
- 启动期诊断缺失时，往往先看 `%LOCALAPPDATA%/HuaEngine/Logs/<host>.log` 比盯瞬时控制台更有效
