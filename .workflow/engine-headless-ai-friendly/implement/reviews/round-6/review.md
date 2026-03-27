# Review Round 6

- Review method: independent agent
- Reviewer: Hume
- Date: 2026-03-26
- Scope: T2.2 (`SceneService`)

## Findings

### Blocker (resolved before closure)
- `ValidateScene()` initially treated legacy `RendererComponent` as a valid replacement for `MaterialComponent`, which did not match the formal `SceneSerializer` / `RenderSystem` scene contract and could allow a scene to validate successfully before losing renderability after round-trip.

## Resolution Applied

- Added `EntitiesUsingLegacyRenderer` to `SceneValidationReport`.
- Tightened `ValidateScene()` so only `MeshComponent + MaterialComponent` counts as the formal render pair; any entity using `RendererComponent` is now reported as requiring manual intervention.
- Added `scene.render_entities.legacy_renderer` diagnostics to the result envelope.
- Extended `SceneServiceSmoke` to cover a valid formal render entity, save/load round-trip, explicit legacy renderer rejection, and the existing missing-transform degradation path.
- Rebuilt the affected targets and reran `SceneServiceSmoke.exe` successfully.

## Verdict

- No blocking issues.
- T2.2 can be considered closed.

## Residual Risk

- Validation remains structural rather than resource-resolvable; missing mesh assets and null material instances are out of scope for this task and belong to later asset/validation work.
- Failure-branch coverage for corrupted scene files and illegal save destinations is still deferred to later scene/validation tasks.
