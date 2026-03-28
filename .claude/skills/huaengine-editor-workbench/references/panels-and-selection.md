# Panels And Selection

## 1. Selection Model

`Selection` is still implemented as global static state:

- one selected `Entity`
- `SetSelection(...)`
- `GetSelection()`
- `HasSelection()`
- `ClearSelection()`

This keeps panel communication simple, but scene/project transitions must clear selection aggressively.

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

The hierarchy only shows entities that carry `TransformComponent`.

## 4. InspectorPanel

The inspector currently:

- checks `Selection::HasSelection()`
- reads the selected entity
- shows project/scene summary
- delegates component editing to `ComponentEditorRegistry`

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

## Related Skills

- For reflection-driven editor or serializer rules, go to `huaengine-serialization-reflection/references/extension-and-integration.md`
- For scene/component runtime structure, go to `huaengine-ecs-scene/references/runtime-structure.md`
- For rendering-visible state used by inspector or scene panel, go to `huaengine-rendering/references/assets-and-materials.md`
- For runtime glue affecting console/input behavior, go to `huaengine-core-runtime/references/window-input-and-imgui.md`
