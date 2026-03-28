# Analysis: engine-project-launcher-separation

> Analysis date: 2026-03-28
> Spec version: 1.0.0

## 1. Functional Requirements Summary

| ID | Requirement | Source | Technical Impact | Acceptance Direction | Priority | Needs Research |
|----|-------------|--------|------------------|----------------------|----------|----------------|
| FR-1 | Provide standalone `ProjectHub.exe` for create/open/resume flows | user request | new host target, startup split, launcher state | launcher exists and owns no-project entry | P0 | yes |
| FR-2 | Make `Editor.exe` a pure project workbench host | user request | remove embedded full launcher mode from Editor | Editor opens workbench when project is known | P0 | yes |
| FR-3 | Stop rendering launcher and workbench as mixed startup surfaces | user request | startup state machine simplification | startup surfaces are clearly separated | P0 | yes |
| FR-4 | Preserve command-line handoff using `--project` / `--scene` | existing code + user direction | keep stable contract between hosts | launcher can open Editor without hidden coupling | P1 | yes |
| FR-5 | Keep recent-project/session entry discoverable after the split | existing workbench closure | move or share session metadata surface | launcher still supports resume flows | P1 | yes |

## 2. Non-Functional Requirements Summary

| ID | Requirement | Quality Attribute | Measurement | Source | Acceptance Direction | Priority |
|----|-------------|------------------|-------------|--------|----------------------|----------|
| NFR-1 | Clear startup responsibilities | maintainability | no mixed launcher/workbench ownership in one render path | user request | distinct host boundaries | P0 |
| NFR-2 | Preserve formal operation boundary | architecture integrity | GUI still consumes `ApplicationOperations` | existing architecture | no return to raw service coupling | P0 |
| NFR-3 | Closure-first milestone | delivery scope | no heavy launcher subsystems in first slice | user request | minimal but usable launcher/editor split | P1 |

## 3. Technical Constraints

- The repository already has a working `Editor.exe --project <path>` startup path and should reuse it.
- Current project/session/workbench model is already centered on `ProjectSession`, `SceneDocument`, and `EditorWorkbenchState`.
- The repository uses CMake multi-target Windows executables; a new launcher host should fit that model.
- Current GUI stack is ImGui; the first launcher slice should stay on ImGui instead of introducing another UI stack.

## 4. Scope and Non-Goals

### In Scope

- separate launcher executable
- cleanup of Editor startup ownership
- recent-project / resume ownership decision
- launcher surface layout direction

### Out of Scope

- template marketplace
- package manager
- remote project discovery
- asset browser redesign

## 5. Decision Points

- [ ] Decision 1: should `ProjectHub.exe` be the only no-project entry, or should `Editor.exe` still keep a fallback embedded entry state?
- [ ] Decision 2: where should recent-project/session metadata be owned after the split?
- [ ] Decision 3: what is the minimum launcher-to-editor handoff contract for the first milestone?

## 6. Research Topics

| Topic | Why | Priority | Status |
|-------|-----|----------|--------|
| launcher-topology | decide host split and ownership boundary between launcher and workbench | P0 | completed |
| entry-surface-boundary | decide how much UI remains in Editor vs launcher, including current layout issue | P0 | completed |

## 7. Assumptions and Risks

| ID | Assumption / Risk | Impact | Mitigation |
|----|-------------------|--------|------------|
| AR-1 | Splitting hosts may accidentally duplicate startup/session logic | High | centralize shared launch/session models and keep Editor project-bound |
| AR-2 | Over-designing the launcher could delay a usable result | Medium | keep first slice limited to create/open/resume handoff |
| AR-3 | Removing embedded hub too early could hurt recovery paths | Medium | keep a minimal Editor-side fallback handoff path, not a full launcher UI |

## 8. Existing Codebase Analysis

### 8.1 Relevant Modules

- `Editor/src/EditorApp.cpp`: parses `--project` and `--scene`, currently directly boots `EditorLayer`
- `Editor/src/EditorLayer.*`: owns Project Hub and Workbench Shell render modes today
- `Editor/src/Workbench/*`: already defines `ProjectSession`, `SceneDocument`, and persisted session storage
- `Docs/editor-project-workbench.md`: current workbench closure description

### 8.2 Reusable Components

- `ApplicationOperations`: remains the formal GUI capability entry
- `EditorSessionStorage`: can remain the persisted session source or move behind a shared launcher-facing adapter
- existing command-line startup contract in `EditorApp`

### 8.3 Files Likely to Change

- `Editor/src/EditorApp.cpp`: startup ownership and fallback path
- `Editor/src/EditorLayer.cpp`: remove or reduce embedded launcher responsibilities
- `Editor/CMakeLists.txt`: split or share launcher/workbench target setup
- new launcher target under a dedicated directory

*Generated by workflow-plan (ANALYZE) | 2026-03-28*
