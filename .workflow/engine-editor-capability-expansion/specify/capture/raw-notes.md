# Raw Notes

## Request Snapshot

- User wants to improve the current editor, which is still considered rough for day-to-day use.
- The immediate goal is not a fully mature toolchain.
- The priority is to establish a better base editor capability set that makes the engine more usable.
- The editor must still leave clear room for future expansion rather than hard-coding a dead-end workflow.
- The user explicitly asked to start from `workflow-specify`.
- Review method is fixed to `agent`.
- Review rounds requested: `1`.

## Current Product Context

- `ProjectHub.exe` is now the preferred no-project launcher host.
- `Editor.exe` is now the project-bound workbench host.
- The Editor already supports project create/open, scene create/open/save/save-as, session restore, hierarchy, inspector, console, and scene viewport.
- The current workbench has a minimal usable loop, but many editor interactions are still lightweight or rough.
- The current project-facing surface is still intentionally small and not yet a full content browser or advanced multi-tool editor.

## Relevant Existing Module Context

- `huaengine-editor-workbench`
  - Editor entry split between `ProjectHub` and `Editor`
  - `ProjectSession`, `SceneDocument`, `EditorWorkbenchState`
  - panels: `Project`, `Hierarchy`, `Inspector`, `Console`, `Scene`
- `huaengine-ecs-scene`
  - scene/entity/component model behind hierarchy and inspector
- `huaengine-rendering`
  - scene viewport rendering path
- `huaengine-core-runtime`
  - window, input, imgui loop, logging

## Stakeholders / User Roles

- Engine maintainer
- Editor user / content creator
- Future feature developer extending the editor

## Extracted Functional Needs

- The editor should present a more complete baseline workbench rather than a thin demo-oriented shell.
- Project-level and scene-level editing should feel coherent and discoverable.
- Common editing actions should be easier to find and complete.
- The editor should expose enough foundational capabilities to be used as the primary daily GUI host.
- The editor should preserve room for future panels, tools, actions, and workflows without requiring a product reset.

## Extracted Non-Functional Needs

- Usability should improve over the current rough baseline.
- Existing minimal workbench closure should be preserved rather than broken apart.
- Scope should stay focused on foundational capabilities, not a full production-grade DCC/editor suite.
- Future editor growth should remain possible without invalidating the first-stage UX.

## Constraints

- Stay at the requirement/spec level only.
- Do not prescribe architecture, patterns, or technology solutions here.
- Build on the already-established project workbench direction.
- Do not redefine the engine into a full toolchain milestone in this phase.

## Ambiguity Assessment

- Ambiguity score: `1`
- Main ambiguity:
  - "基础功能集合" is broad, but the current repository context strongly suggests the baseline should center on project, scene, panel, viewport, selection/editing, and user feedback flows.

## Proposed Clarifying Assumption

- Treat this phase as "editor foundation expansion" rather than "full editor maturity".
- Focus on making the editor reliably usable as the main workbench for core engine workflows.
- Reserve advanced asset browser depth, heavy import pipelines, and advanced tooling ecosystems as later work.
