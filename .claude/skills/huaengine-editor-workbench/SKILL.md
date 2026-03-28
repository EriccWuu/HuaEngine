---
name: huaengine-editor-workbench
description: >
  HuaEngine Editor workbench navigation. Covers EditorApp, EditorLayer, ProjectHub handoff,
  ProjectSession, SceneDocument, EditorWorkbenchState, ProjectPanel, Selection,
  Hierarchy, Inspector, and Console. Use when the user asks about Editor startup,
  project workbench flow, scene document lifecycle, panel behavior, or where to modify
  HuaEngine editor behavior.
---

# HuaEngine Editor Workbench

## Overview

This Skill is the primary navigation entry for the current Editor workbench implementation.
Use it when the question is about how the Editor starts, how it enters the project workbench,
how GUI panels consume project and scene state, or how the Editor drives the formal engine
control surface through `ApplicationOperations`.

## Module Boundary

- `Editor/src/EditorApp.cpp`: Editor process entry and `CreateApplication()`
- `Editor/src/EditorLayer.*`: minimal fallback entry, Workbench Shell, viewport shell, scene document flow
- `Editor/src/Workbench/`: `ProjectSession`, `SceneDocument`, `EditorWorkbenchState`, `EditorSessionStorage`
- `Editor/src/Panels/`: `ProjectPanel`, `HierarchyPanel`, `InspectorPanel`, `ConcolePanel`
- `Editor/src/Selection.*`: global selection state
- `Editor/src/ComponentEditor*.h`: component editor registry and reflection-driven editing
- `ProjectHub/src/*`: standalone launcher host and launcher UI

## Core Runtime Model

- Preferred no-project entry is `ProjectHub.exe`
- `Editor.exe` is now primarily a project-bound workbench host
- Editor fallback no longer owns full create/open launcher UX
- `ProjectHub.exe` uses a smaller dedicated launcher window and full-window launcher layout
- Project activation creates a `ProjectSession`
- Scene editing is centered on `SceneDocument`
- Panel summaries are cached in `EditorWorkbenchState`
- The last active session is persisted through `EditorSessionStorage`
- GUI actions route through `ApplicationOperations`

## Core Rules

- `EditorApp` remains a thin host that only pushes `EditorLayer`
- `EditorLayer` is the orchestration point for Editor fallback, Workbench Shell, scene viewport binding, and panel coordination
- GUI does not own project or scene domain logic directly; it consumes formal engine capabilities via `ApplicationOperations`
- `ProjectSession` describes the active project and should be treated as the authoritative GUI project context
- `SceneDocument` describes the active scene document and owns path/display/dirty/validation semantics
- `EditorWorkbenchState` is the shared summary surface for panels, diagnostics, and last-operation visibility
- `Selection` is still global static state and must be cleared on project or scene transitions
- Inspector edits propagate dirty-state back into the active `SceneDocument`

## Key Files

- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.h`
- `Editor/src/EditorLayer.cpp`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.h`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/Workbench/ProjectSession.h`
- `Editor/src/Workbench/SceneDocument.h`
- `Editor/src/Workbench/EditorWorkbenchState.h`
- `Editor/src/Workbench/EditorSessionStorage.h`
- `Editor/src/Workbench/EditorSessionStorage.cpp`
- `Editor/src/Panels/ProjectPanel.h`
- `Editor/src/Panels/ProjectPanel.cpp`
- `Editor/src/Panels/HierarchyPanel.h`
- `Editor/src/Panels/HierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.h`
- `Editor/src/Panels/InspectorPanel.cpp`
- `Editor/src/Panels/ConsolePanel.h`
- `Editor/src/Panels/ConsolePanel.cpp`

## Navigation

- For startup flow, launcher handoff, session restore, and scene-document flow, read `references/editor-flow.md`
- For panel behavior, selection, dirty tracking, and workbench summaries, read `references/panels-and-selection.md`
- For scene/entity/component runtime facts behind the GUI, go to `huaengine-ecs-scene`
- For scene viewport rendering behavior, go to `huaengine-rendering`
- For runtime loop, ImGui layer, and window/input glue, go to `huaengine-core-runtime`

## Cross-Skill Navigation

- If the question is really about scene serialization, component ownership, or scene runtime facts, switch to `huaengine-ecs-scene`
- If the question is really about viewport rendering, framebuffer resize, editor camera, or material/mesh visibility, switch to `huaengine-rendering`
- If the question is really about the application loop, window lifecycle, or runtime logging/input behavior, switch to `huaengine-core-runtime`
- If Inspector auto-editing or reflected field drawing is broken, switch to `huaengine-serialization-reflection`

## Common Pitfalls

- `ProjectHub.exe` is now the preferred no-project entry; do not assume full create/open launcher UX still lives inside Editor startup
- `ProjectPanel` is a light project-facing surface, not a full asset browser
- `HierarchyPanel` still enumerates entities through `TransformComponent`
- `Selection` is global static state; stale entity handles are still a real risk when transitions are mishandled
- `ConcolePanel` / `Concole` spelling is still the repository reality
- GUI summaries may be correct while underlying runtime state is wrong; verify against `ApplicationOperations` and `ProjectWorkbenchSmoke`
