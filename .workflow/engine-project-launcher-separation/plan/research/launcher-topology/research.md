# Research: launcher-topology

## Scope

Decide the host topology for separating a standalone launcher from the Editor workbench.

## Gather

- [E-1] The repository already supports `Editor.exe --project` and optional `--scene`.
- [E-2] Launcher-style entry and workbench runtime are still owned by `EditorLayer`.

## Analyze

Three candidate topologies were considered:

| Option | Description | Pros | Risks |
|--------|-------------|------|-------|
| A | Keep embedded Project Hub inside `Editor.exe` | lowest change cost | keeps host responsibility mixed |
| B | Add standalone `ProjectHub.exe`, keep Editor project-bound | clear product boundary, modern shape, reuses existing `--project` handoff | requires new target and shared startup/session boundary |
| C | Add standalone launcher and remove all Editor fallback/recovery entry | cleanest long-term separation | higher risk if launcher binary is missing or handoff fails |

## Compare

Option B is the best first-step topology.

- It delivers the product shape the user asked for.
- It reuses the current command-line startup contract [E-1].
- It avoids a brittle all-or-nothing removal of Editor-side recovery behavior [E-2].

## Recommend

Recommend **Option B**:

- `ProjectHub.exe` becomes the authoritative no-project launcher.
- `Editor.exe` becomes the authoritative project workbench host.
- `Editor.exe` keeps only a minimal fallback handoff path, not a full embedded launcher UI.

This gives the repository a modern launcher/workbench split without introducing IPC or heavy startup infrastructure in the first slice. [E-1][E-2]

## Sources

- [E-1](evidence/evidence-1.md)
- [E-2](evidence/evidence-2.md)

