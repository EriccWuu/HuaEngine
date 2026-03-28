# Architecture Reference

## 1. Build Graph

The root `CMakeLists.txt` now registers these maintained top-level targets:

- `HuaEngine`
- `Editor`
- `ProjectHub`
- `HuaEngineHeadless`
- smoke/regression targets under `Tests/`

`Sandbox` has been removed from the active solution and is no longer part of the supported host graph.

## 2. Host Roles

### ProjectHub

- standalone launcher host
- preferred no-project entry
- creates/opens/resumes projects
- launches `Editor.exe --project [--scene]`

### Editor

- project-bound GUI workbench
- owns scene viewport, hierarchy, inspector, console, and project panel
- consumes engine capabilities through `ApplicationOperations`

### Headless

- formal CLI/headless interface
- consumes the same operation surface
- emits machine-readable JSON

## 3. Engine Core

The engine library still owns:

- `ApplicationServices` / `ApplicationOperations`
- project, scene, asset, script, and validation services
- ECS and scene runtime
- rendering and OpenGL backend
- serialization and reflection

Hosts differ mainly in shell and presentation, not in domain capability ownership.

## 4. Current Reading Order

When navigating the repo:

1. identify the host boundary first
2. check whether the issue belongs to the operation layer or engine internals
3. then drill into scene/rendering/serialization specifics

## 5. Key Files

- `CMakeLists.txt`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Headless/src/main.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
