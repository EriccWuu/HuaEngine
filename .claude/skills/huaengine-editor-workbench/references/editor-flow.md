# Editor Flow

## 1. Product Entry

The product entry is now split across two GUI hosts:

- `ProjectHub.exe`: authoritative no-project launcher host
- `Editor.exe`: project-bound workbench host

The Editor entry remains:

- `Editor/src/EditorApp.cpp`
- `EditorApp : Application`
- `PushLayer(new EditorLayer(spec))`
- shared `main()` from `HuaEngine/EntryPoint.h`

## 2. Startup States

Inside `Editor.exe`, the relevant GUI states are now:

- minimal fallback / redirect surface when no project is active
- `Workbench Shell` when a project is active

The full create/open/resume launcher UX no longer lives inside `EditorLayer`.

## 3. ProjectHub Host Shape

`ProjectHub.exe` is no longer just a centered card inside an oversized shell.

Current launcher behavior:

- standalone GUI host
- smaller launcher-oriented default window size
- full-window launcher workspace
- two-column layout:
  - left: resume and status
  - right: project root, project name, create/open actions

This host is meant to feel like a dedicated launcher, not like the Editor with a launcher panel embedded inside it.

## 4. Session Restore

`ProjectHub.exe` and `Editor.exe` both read `EditorSessionStorage`.

If startup arguments contain `--project <path>`, the Editor bypasses launcher behavior and opens the project directly.
If `--scene <path>` is also provided, the Editor opens that scene after project activation.

Persisted session fields:

- `LastProjectRoot`
- `LastProjectName`
- `LastScenePath`

Storage location:

- `%LOCALAPPDATA%/HuaEngine/Editor/session.json`

## 5. Project Activation

Project activation is a formal transition:

1. launcher or direct command-line resolves the project target
2. build a `ProjectSession`
3. initialize the workbench shell
4. restore the last scene or open the requested scene
5. persist the active session

## 6. Scene Document Lifecycle

The Editor scene flow is centered on `SceneDocument`, not a raw `Ref<Scene>`.

Current document operations:

- `New Scene`
- `Open Scene`
- `Save Scene`
- `Save Scene As`
- `Validate Scene`

The document owns:

- scene path
- display name
- dirty state
- last validation result

## 7. Interaction Flow

The first-batch editor interaction path is now:

1. panel input or menu/shortcut trigger enters `EditorLayer`
2. `EditorLayer` routes the action through `EditorInteractionHost`
3. `EditorCommandStack` executes the command and updates undo/redo history
4. `SceneDocument` dirty state is synchronized from command history
5. `EditorWorkbenchState` and diagnostics surfaces are refreshed

Concrete first-batch actions currently include:

- create entity
- delete selected entities
- add component
- remove component
- undo
- redo

The current built-in shortcut surface includes:

- `Ctrl+Z`
- `Ctrl+Y`
- `Ctrl+Shift+N`
- `Delete`

## 8. OnGuiRender Composition

`OnGuiRender()` now composes:

- minimal fallback / redirect surface when no project is active
- DockSpace and Workbench Shell when a project is active
- Project panel
- Scene panel
- Scene hierarchy
- Inspector
- Console

## 9. Dock Layout

The default workbench layout is:

- left: `Project`
- left-middle: `Hierarchy`
- center: `Scene`
- right: `Inspector`
- bottom: `Console`

## Related Skills

- For runtime startup and host shell behavior, go to `huaengine-core-runtime/references/lifecycle-and-events.md`
- For viewport rendering flow, go to `huaengine-rendering/references/runtime-flow.md`
- For scene/entity/component runtime facts, go to `huaengine-ecs-scene/references/runtime-structure.md`
