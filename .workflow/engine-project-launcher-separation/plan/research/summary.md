# Research Summary: engine-project-launcher-separation

## Topics

- `launcher-topology`
- `entry-surface-boundary`

## Combined Conclusions

1. The repository already has a viable launcher-to-editor handoff contract through `Editor.exe --project [--scene]`, so the first standalone launcher slice does not need IPC.
2. The current Project Hub layout problem is a symptom of keeping launcher UX inside the Editor host, not of the workbench panel model itself.
3. The recommended first milestone is:
   - add `ProjectHub.exe`
   - make it the authoritative no-project entry
   - keep `Editor.exe` project-bound
   - preserve only a minimal fallback handoff/error state inside Editor

## Research Index

- [launcher-topology](launcher-topology/research.md)
- [entry-surface-boundary](entry-surface-boundary/research.md)

