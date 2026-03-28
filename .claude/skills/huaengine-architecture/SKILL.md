---
name: huaengine-architecture
description: >
  HuaEngine repository architecture navigation. Covers the build graph,
  HuaEngine/ProjectHub/Editor/Headless target relationships, startup flow,
  Application service/control layers, and subsystem boundaries. Use when the
  user asks about overall architecture, project structure, build targets,
  startup flow, or where to modify engine/editor/headless behavior.
---

# HuaEngine Architecture

## Overview

This Skill is the top-level navigation entry for the repository.
Use it when the question is about where a change belongs, what the current host topology is,
 or how build/runtime/control surfaces are organized.

## Host Topology

- `HuaEngine/`: core static library
- `ProjectHub/`: standalone no-project launcher host
- `Editor/`: project-bound GUI workbench host
- `Headless/`: formal CLI/headless host

`Sandbox` has been removed from the active build graph and is no longer a maintained host target.

## Core Rules

- Start at root `CMakeLists.txt` and host entry points before going into subsystem internals
- Formal host-facing capabilities should be exposed through `ApplicationOperations`
- GUI, headless, and automation hosts should not reintroduce raw domain-service entry points
- Rendering, scene, asset, script, and validation behavior still live in the engine library, not in hosts

## Key Files

- `CMakeLists.txt`
- `HuaEngine/CMakeLists.txt`
- `Editor/CMakeLists.txt`
- `ProjectHub/CMakeLists.txt`
- `Headless/CMakeLists.txt`
- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Headless/src/main.cpp`
- `Headless/src/HeadlessCommandRunner.cpp`

## Navigation

- For build surface and runtime output layout, read `references/build-and-run.md`
- For subsystem layering and host boundaries, read `references/architecture.md`
- For launcher/workbench behavior, go to `huaengine-editor-workbench`
- For rendering flow, go to `huaengine-rendering`
- For core runtime lifecycle, go to `huaengine-core-runtime`
