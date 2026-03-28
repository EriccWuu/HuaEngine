---
name: huaengine-rendering
description: >
  HuaEngine rendering module navigation. Covers RenderSystem, Renderer,
  RenderCommand/RendererAPI, OpenGL backend, Camera, FrameBuffer, Shader,
  Material, Mesh, and the formal rendering seam exposed through
  ApplicationOperations. Use when the user asks about render flow,
  shader/material setup, framebuffer issues, mesh loading, or rendering code layout.
---

# HuaEngine Rendering

## Overview

This Skill is the primary navigation entry for the current rendering path.

## Module Boundary

- `HuaEngine/src/HuaEngine/Rendering/`: renderer abstractions and GPU resources
- `HuaEngine/src/Platform/OpenGL/`: OpenGL backend
- `HuaEngine/src/Module/Rendering/`: scene-facing runtime rendering system
- `HuaEngine/src/HuaEngine/Application/`: formal rendering seam

The maintained host consumers are now `Editor`, `ProjectHub`-launched `Editor`, and `Headless` where applicable.
`Sandbox` is no longer part of the active host topology.

## Core Rules

- Follow `RenderSystem -> Renderer -> RenderCommand -> RendererAPI(OpenGL)` for actual frame submission
- Hosts should use the formal rendering seam exposed through `ApplicationOperations`
- Investigate scene/component inputs before assuming a backend or shader issue

## Key Files

- `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Renderer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RenderCommand.cpp`
- `HuaEngine/src/HuaEngine/Rendering/RendererAPI.cpp`
- `HuaEngine/src/Platform/OpenGL/OpenGLRendererAPI.cpp`

## Navigation

- For runtime frame flow, read `references/runtime-flow.md`
- For material/mesh/asset-facing details, read `references/assets-and-materials.md`
- For host topology, go to `huaengine-architecture`
