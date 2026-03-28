---
name: huaengine-rendering
description: >
  HuaEngine 渲染模块导航。覆盖 RenderSystem、Renderer、
  RenderCommand/RendererAPI、OpenGL backend、Camera、FrameBuffer、Shader、
  Material、Mesh，以及通过 ApplicationOperations 暴露的正式渲染缝。适用于回答
  渲染主链、shader/material 配置、framebuffer 问题、mesh 加载和渲染改动落点这类问题。
---

# HuaEngine 渲染模块

## 概览

这个 Skill 是当前渲染路径的主导航入口。

## 模块边界

- `HuaEngine/src/HuaEngine/Rendering/`
  - renderer 抽象与 GPU 资源
- `HuaEngine/src/Platform/OpenGL/`
  - OpenGL backend
- `HuaEngine/src/Module/Rendering/`
  - 面向场景的运行时渲染系统
- `HuaEngine/src/HuaEngine/Application/`
  - 正式渲染缝

当前被维护的宿主消费者是 `Editor`、由 `ProjectHub` 拉起的 `Editor`，以及适用场景下的 `Headless`。
`Sandbox` 已不在正式宿主图中。

## 核心规则

- 实际帧提交按 `RenderSystem -> Renderer -> RenderCommand -> RendererAPI(OpenGL)` 排查
- 宿主应使用 `ApplicationOperations` 暴露的正式渲染缝
- 排查渲染问题时，先看场景 / 组件输入，再怀疑 backend 或 shader

## 关键文件

- `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Renderer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderCommand.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RendererAPI.cpp`
- `HuaEngine/src/Platform/OpenGL/OpenGLRendererAPI.cpp`

## 导航

- 看运行时帧流：读 `references/runtime-flow.md`
- 看材质 / mesh / 资源细节：读 `references/assets-and-materials.md`
- 看宿主拓扑：转 `huaengine-architecture`
