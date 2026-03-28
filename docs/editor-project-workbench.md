# Editor Project Workbench

## Goal

The current Editor workbench is now organized around a minimal but complete project workflow:

- create a project
- open an existing project
- create or open a scene inside that project
- edit the scene in the standard workbench panels
- save the scene
- reopen the Editor and restore the last project/session state

This is intentionally a closure-first milestone. It is not a full production content browser yet.

## Current Model

The workbench runtime now has two explicit state layers:

- `ProjectSession`: the active project context bound to a real project root
- `SceneDocument`: the currently edited scene plus document metadata such as path, display name, dirty state, and last validation

These are cached into `EditorWorkbenchState`, which remains the shared summary surface for GUI panels.

## Entry Flow

`Editor` is now the project-bound workbench host.

The preferred no-project entry is the separate `ProjectHub.exe` launcher host.
When `Editor` starts without project context, it only shows a minimal fallback surface that can:

- launch `ProjectHub.exe`
- reopen the last known project
- reset the saved session

Once a project is activated, the Editor transitions into `Workbench Shell`.

The Editor also supports direct project startup from the command line:

- `Editor.exe --project "D:\MyProject"`
- `Editor.exe --project .`
- `Editor.exe --project "D:\MyProject" --scene "Scenes\main.scene"`

Key files:

- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/EditorLayer.cpp`
- `Editor/src/Workbench/ProjectSession.h`
- `Editor/src/Workbench/SceneDocument.h`
- `Editor/src/Workbench/EditorSessionStorage.h`

## Session Persistence

Launcher and Editor currently share the persisted entry session at:

- `%LOCALAPPDATA%/HuaEngine/Editor/session.json`

Persisted fields:

- last project root
- last project name
- last opened scene path

This allows `ProjectHub.exe` to offer resume flows and allows `Editor.exe` to keep a minimal recovery path when launched without project arguments.

## Workbench Layout

The default layout now appears only after a project session is active, and centers around project editing rather than demo bootstrapping:

- left: `Project`
- left-middle: `Scene Hierarchy`
- center: `Scene`
- right: `Inspector`
- bottom: `Console`

`Project Panel` is now the project-facing navigation surface. It shows:

- active project summary
- current scene summary
- latest validation counts
- `Assets/` and `Scenes/` directory summaries
- actions for scene open and project refresh

## Scene Document Workflow

The workbench now supports the minimal scene-document loop:

- `New Scene`
- `Open Scene`
- `Save Scene`
- `Save Scene As`
- dirty-state tracking
- switch confirmation when unsaved changes exist

When scene state changes in Inspector-driven edits, the active `SceneDocument` is marked dirty.

## Panel Consumption Rules

The main panels are now aligned around session/document summaries:

- `Scene Hierarchy` reads current project/scene summary and latest operation state from `EditorWorkbenchState`
- `Inspector` edits the selected entity and marks the scene dirty when component fields change
- `Console` shows diagnostics from workbench state and raw runtime logs in separate tabs
- `Project Panel` is the dedicated project/session surface and light scene navigator

## Operation Boundary

The GUI workbench continues to consume formal engine capabilities through `ApplicationOperations`.

That means:

- project creation/open/status does not bypass the formal control layer
- scene create/load/save/validate uses the same engine operations exposed to other hosts
- the Editor remains a consumer of formal engine capabilities, not a parallel domain owner

## Smoke Coverage

The project workbench closure is covered by `ProjectWorkbenchSmoke`.

The smoke verifies the main chain:

1. create project
2. open/check project
3. create scene
4. edit scene content
5. save scene
6. persist editor session
7. reopen scene
8. verify edited content survives reload

## Known Limits

This milestone is still intentionally light:

- there is no full asset browser yet
- project tree operations are minimal
- the Editor still focuses on single-scene editing
- recent-project management is only session-based, not a full launcher database
- `ProjectHub.exe` is a first-step launcher, not yet a template center or version manager

The purpose of this stage is to make the engine usable as a real project workbench before adding heavier tooling.
