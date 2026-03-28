---
name: "engine-project-launcher-separation"
status: approved
version: 1.0.0
created_date: "2026-03-28"
source: "direct user request"
---

# Spec: engine-project-launcher-separation

## Goal

Evolve the current embedded `Project Hub` entry page into a separate launcher-style host, closer to modern engines:

- `ProjectHub.exe` becomes the formal no-project entry
- `Editor.exe` becomes a pure project workbench host
- current oversized launcher shell / undersized card layout is removed from the Editor runtime path

## Functional Requirements

- FR-1: the engine must provide a standalone launcher host for create/open/resume project flows
- FR-2: the Editor must open directly into the workbench when a project is supplied
- FR-3: the Editor must no longer own the full launcher UI surface as its startup mode
- FR-4: launcher-to-editor handoff must use stable command-line project startup
- FR-5: recent-project and session entry behavior must remain discoverable after the split

## Non-Functional Requirements

- NFR-1: startup responsibilities must be clearer than the current mixed embedded approach
- NFR-2: the split must preserve existing project/session/workbench architecture rather than reintroducing raw domain coupling
- NFR-3: the first milestone should establish a usable closure, not a full template marketplace or heavy content launcher

## Scope

- standalone `ProjectHub.exe`
- Editor startup contract cleanup
- launcher/editor ownership boundary
- launcher layout direction

## Non-Goals

- full project template center
- engine version management
- plugin marketplace
- asset import pipeline redesign

*Input snapshot for workflow-plan | 2026-03-28*
