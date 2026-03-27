---
name: huaengine-rendering
description: >
  HuaEngine 渲染模块导航，覆盖 RenderSystem、Renderer、RenderCommand/RendererAPI、OpenGL 后端、
  Camera、FrameBuffer、Shader、Material、Mesh，以及宿主通过 ApplicationOperations 接入渲染的正式 seam。
  Use when the user asks about rendering architecture, render flow, shader/material setup, framebuffer issues,
  mesh loading, camera rendering, OpenGL backend behavior, or where to modify rendering code in HuaEngine.
---

# HuaEngine Rendering

## Overview

这个 Skill 用于定位 HuaEngine 当前渲染模块的真实实现入口，适合回答“渲染一帧怎么走”“宿主应该通过哪条 seam 接渲染”“FrameBuffer/Shader 应该改哪一层”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/Rendering/`：渲染抽象层，包含 Renderer、RenderCommand、RendererAPI、Camera、FrameBuffer、Shader、Material、Mesh、RenderPipeline
- `HuaEngine/src/Platform/OpenGL/`：OpenGL 后端实现，负责 Shader、Texture、VertexArray、Buffer、FrameBuffer、RendererAPI 的实际落地
- `HuaEngine/src/Module/Rendering/`：运行时渲染系统入口，负责从 `Scene` 和组件数据组织一帧提交
- `HuaEngine/src/HuaEngine/Application/`：宿主接入渲染的正式 seam，当前由 `rendering.attach_scene_viewport / rendering.render_scene_viewport` 暴露
- `Editor`、`Sandbox` 和 `Headless` 都只是使用者；真正的渲染行为主要还是沿着上述几层展开

## 核心子系统概览

- `RenderSystem`：按场景中的相机和渲染组件组织提交
- `Renderer`：维护当前 Camera，并把提交下发到 `RenderCommand`
- `RenderCommand + RendererAPI`：抽象绘制、清屏、viewport 设置；当前固定走 OpenGL
- `Shader / Texture / Buffer / VertexArray / FrameBuffer`：底层 GPU 资源抽象
- `Material / MaterialInstance`：基于参数表和 `std::variant` 进行 uniform / texture 绑定
- `Mesh / MeshManager / MeshData`：网格资源、惰性 GPU 上传、默认几何体注册与序列化

## Core Rules

- 先分清问题属于“场景提交层”“渲染抽象层”还是“OpenGL 后端层”；不要一上来只搜 `Rendering/`
- 一帧主路径当前以 `RenderSystem -> Renderer -> RenderCommand -> RendererAPI(OpenGL)` 为主，`RenderPipeline` 目前不是主热路径
- 宿主如果要接入 scene viewport 或未来 rendering capability，优先走 `ApplicationOperations` 暴露的 rendering seam；不要在 GUI 层重新发明直连 `RenderSystem` 的入口
- `MeshComponent` 只有 `MeshAssetName` 时依赖 `MeshManager` 先加载资源；没注册 mesh 时，`GetVertexArray()` 会返回空
- `MaterialInstance` 只存参数覆盖；真正的 uniform 和 texture 绑定仍通过其 `BaseMaterial` 的 shader 与参数定义完成
- 搜索 FrameBuffer 实现时要注意真实文件名是 `FranmeBuffer.cpp`；搜索系统注册时要注意仓库里仍是 `AddSyetem(...)`
- 当前 `RenderSystem` 会遍历所有 `CameraComponent`，并没有使用 `Primary` 过滤；涉及多相机场景时先按现状排查

## 关键入口文件

- `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Renderer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderCommand.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RendererAPI.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Camera.h`
- `HuaEngine/src/HuaEngine/Rendering/EditorCamera.cpp`
- `HuaEngine/src/HuaEngine/Rendering/FrameBuffer.h`
- `HuaEngine/src/HuaEngine/Rendering/FranmeBuffer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Shader/Shader.h`
- `HuaEngine/src/HuaEngine/Rendering/Material/MaterialCore.h`
- `HuaEngine/src/HuaEngine/Rendering/Mesh/MeshManager.cpp`
- `HuaEngine/src/Platform/OpenGL/OpenGLRendererAPI.cpp`

## Navigation

- 想理解一帧提交流程、模块分层、工厂选择和 Camera/Framebuffer 行为：读 `references/runtime-flow.md`
- 想定位材质、参数绑定、Mesh 资源、默认几何体和序列化入口：读 `references/assets-and-materials.md`
- 想先看仓库级整体分层，而不是只看渲染模块：先读 `huaengine-architecture`

## Cross-Skill Navigation

- 如果渲染异常的根因落在实体创建、组件缺失、系统未注册、Scene 更新顺序或编辑器层级树可见性，先转到 `huaengine-ecs-scene`
- 如果问题和场景、材质或 mesh 的持久化有关，转到 `huaengine-serialization-reflection`
- 如果要先确认入口来自 `Editor`、`Sandbox` 还是 `Headless`，转到 `huaengine-architecture`
- `RenderSystem`、`CameraComponent`、`MeshComponent`、`MaterialComponent` 跨模块联动明显时，优先按“Scene 供数 -> Rendering 提交 -> Serialization 持久化”顺序串联排查

## Common Pitfalls

- `RenderSystem::RenderSingleCamera()` 直接调用 `m_Framebuffer->Bind()`；如果 framebuffer 没先设置，会直接出错
- `Renderer::Begin()` 只保存当前 Camera，`Renderer::End()` 目前为空；不要假设这里已经有批处理或状态回收逻辑
- `EditorCamera::UpdateProjectionMat()` 直接用 `m_Viewport.x / m_Viewport.y`；调用侧必须保证高度非零
- `OpenGLShader::Unbind()` 目前是空实现；排查状态泄漏时要按现状看
- `RenderPipeline/` 目前还不是主热路径，做现有 bug 定位时不要先把问题归因到它
