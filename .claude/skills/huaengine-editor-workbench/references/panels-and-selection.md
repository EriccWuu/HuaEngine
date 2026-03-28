# Panels And Selection

## 1. Selection Model

`Selection` is still implemented as global static state, but it is no longer single-entity only:

- a vector of selected `Entity`
- `SetSelection(...)`
- `SetSelections(...)`
- `AddToSelection(...)`
- `ToggleSelection(...)`
- `RemoveFromSelection(...)`
- `GetSelection()`
- `GetPrimarySelection()`
- `GetSelections()`
- `HasSelection()`
- `HasSingleSelection()`
- `ClearSelection()`

Single selection is now the first-item / primary-selection case.
This keeps panel communication simple, but scene/project transitions must still clear selection aggressively.

## 2. Project Panel

`ProjectPanel` is the new project-facing workbench panel.

It consumes `EditorWorkbenchState` summaries and shows:

- current project root
- current project name
- current scene display name
- dirty marker
- latest validation warning/error counts
- light `Assets/` and `Scenes/` discovery

Current actions:

- `Open Scene`
- `Refresh Project`

It is intentionally a summary/navigation surface, not a full content browser.

## 2.1 View Menu Visibility

The main workbench menu bar now exposes a `View` menu.

It controls visibility for:

- `Project`
- `Hierarchy`
- `Inspector`
- `Console`
- `Scene`

Panel visibility is currently owned by `EditorLayer`, not by the individual panel classes.

## 3. HierarchyPanel

The hierarchy panel currently:

- reads the active `Scene`
- enumerates entities through `registry.view<TransformComponent>()`
- wraps each item as `Entity(entity, &entityManager)`
- updates `Selection`
- shows project/scene summary and last operation/validation counts from `EditorWorkbenchState`
- exposes registered context-menu entries for `hierarchy.window` and `hierarchy.entity`
- supports plain click single selection and `Ctrl+Click` toggle multi-selection
- clears selection when clicking empty background
- exposes a drag/drop intent surface for hierarchy entries

The hierarchy only shows entities that carry `TransformComponent`.

## 4. InspectorPanel

The inspector currently:

- checks `Selection::HasSelection()`
- reads the primary selected entity
- shows project/scene summary
- delegates component editing to `ComponentEditorRegistry`
- exposes registered context-menu entries for `inspector.window` and `inspector.entity`
- falls back to a summary-only mode when multiple entities are selected

`InspectorPanel::OnGuiRender()` now returns whether edits actually changed component data.
That changed flag is used by `EditorLayer` to mark the active `SceneDocument` dirty.

## 5. ComponentEditorRegistry

The component editor registry:

- registers component drawers by `std::type_index`
- keeps a display name and draw function for each registered component type
- walks registered component types in registration order
- uses reflection-driven field drawing through `DrawComponentEditor(...)`

The default editor surface is still intentionally small. Not every reflected type has a rich custom editor yet.

## 6. ConsolePanel

`ConcolePanel` now has two distinct surfaces:

- `Diagnostics`: workbench diagnostics and validation-oriented event history from `EditorWorkbenchState`
- `Logs`: raw runtime logs from the log sink

This makes the console both a runtime log surface and a formal workbench feedback surface.

## 7. Shared Summary Surface

The main panels now consume session/document summaries consistently:

- `ProjectPanel`: project summary, scene summary, validation counts
- `HierarchyPanel`: project summary, scene summary, last op, validation counts
- `InspectorPanel`: project summary, scene summary, selected-entity editing
- `ConcolePanel`: diagnostics and runtime logs

## 8. Interaction Core

The current first-batch editor interaction model is centralized in `Editor/src/Interaction/`:

- `EditorInteractionHost`: binds workbench state, project session, and scene document
- `EditorCommandStack`: undo/redo history and dirty tracking
- `ContextMenuRegistry`: registered context-menu entry surface
- `ShortcutRegistry`: built-in and future custom shortcut registration surface
- `DragDropIntentRegistry`: drag/drop intent registration surface
- `EditorSceneCommands`: concrete first-batch entity/component commands

Current built-in shortcuts:

- `Ctrl+Z`
- `Ctrl+Y`
- `Ctrl+Shift+N`
- `Delete`

## Related Skills

- For reflection-driven editor or serializer rules, go to `huaengine-serialization-reflection/references/extension-and-integration.md`
- For scene/component runtime structure, go to `huaengine-ecs-scene/references/runtime-structure.md`
- For rendering-visible state used by inspector or scene panel, go to `huaengine-rendering/references/assets-and-materials.md`
- For runtime glue affecting console/input behavior, go to `huaengine-core-runtime/references/window-input-and-imgui.md`
