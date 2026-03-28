---
name: huaengine-core-runtime
description: >
  HuaEngine core runtime navigation. Covers Application, EntryPoint,
  ApplicationServices/ApplicationOperations, Layer/LayerStack, Window/Input,
  Events, Log, host launch support, and ImGui glue. Use when the user asks
  about application lifecycle, event dispatch, host startup, window sizing,
  input polling, layer ordering, logging, or where to modify HuaEngine core runtime behavior.
---

# HuaEngine Core Runtime

## Overview

This Skill is the primary navigation entry for HuaEngine runtime lifecycle and host startup behavior.
Use it when the question is about `Application::Start()` vs `Run()`, shared host boundaries,
window creation, runtime services, or how GUI and non-GUI hosts share the same control surface.

## Module Boundary

- `HuaEngine/src/HuaEngine/Application.*` and `HuaEngine/src/HuaEngine/Application/`: application lifecycle, services, operations, registry
- `HuaEngine/src/HuaEngine/EntryPoint.h`: shared host entry
- `HuaEngine/src/HuaEngine/Core/`: `Layer`, `LayerStack`, `Window`, `Input`, `Log`, `HostLaunch`
- `HuaEngine/src/HuaEngine/Events/`: event base types and dispatch
- `HuaEngine/src/HuaEngine/GUI/`: `ImguiLayer`
- `HuaEngine/src/Platform/Windows/`: Windows/GLFW window and input implementation

## Core Runtime Model

- The shared startup chain is `main() -> Log::Init() -> CreateApplication() -> Application::Start()/Run()`
- `Application` construction is intentionally light; runtime side effects happen in `Start()`
- `ApplicationServices` is the internal composition root
- `ApplicationOperations` is the formal host-facing capability surface
- `ApplicationSpecification` now controls host shell shape:
  - `EnableWindow`
  - `EnableGuiLayer`
  - `WindowWidth`
  - `WindowHeight`
- `HostLaunch` is the shared process-handoff utility used by multi-host flows such as `ProjectHub.exe -> Editor.exe`

## Core Rules

- Hosts should expose capabilities through `Application::GetOperations()`, not by re-exposing raw services
- `ApplicationSpecification` only configures the host shell; it does not redefine domain/service semantics
- Windowed hosts now pass explicit `WindowProps(Name, WindowWidth, WindowHeight)` into `Window::Create(...)`
- `ProjectHub.exe` uses host-specific sizing and should be treated as a dedicated launcher shell, not a full workbench
- `LayerStack` inserts normal layers before overlays; events are dispatched in reverse order
- `Input` is not universal in headless/no-window hosts because it depends on the active runtime window
- `ImguiLayer` is created by `Application` for GUI hosts; downstream GUI hosts should not create a second global ImGui layer

## Key Files

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

## Navigation

- For startup flow, layer ordering, event dispatch, and runtime initialization boundaries, read `references/lifecycle-and-events.md`
- For window callbacks, input polling, GLFW behavior, and ImGui platform integration, read `references/window-input-and-imgui.md`
- For `ProjectHub.exe` / `Editor.exe` host separation and workbench entry flow, go to `huaengine-editor-workbench`
- For scene/system update flow, go to `huaengine-ecs-scene`
- For viewport rendering/update flow, go to `huaengine-rendering`

## Cross-Skill Navigation

- If the question is really about `ProjectHub`, session restore, Project Panel, or Editor workbench composition, switch to `huaengine-editor-workbench`
- If the question is about Scene/System registration or scene update behavior, switch to `huaengine-ecs-scene`
- If the runtime change affects camera/framebuffer/render submission, switch to `huaengine-rendering`
- If the issue is in startup-time serialization initialization or backend registration, switch to `huaengine-serialization-reflection`

## Common Pitfalls

- `Application` no longer implies “one default window shape fits all hosts”; host sizing now comes from `ApplicationSpecification`
- `ProjectHub.exe` is intentionally smaller than the Editor and acts as a launcher shell
- `HostLaunch` only handles process handoff; it is not a richer IPC/session protocol
- `ConsolePanel` depends on `Log::Init()` and the shared log sink; if logging is not initialized, GUI diagnostics will be incomplete
