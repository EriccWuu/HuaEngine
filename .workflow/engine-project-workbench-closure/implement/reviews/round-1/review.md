# Review Round 1

## Scope

- T1.1
- T1.2
- T1.4

## Initial finding

- Blocker: global `Selection` was not reset when switching scenes or returning to hub, which could leave panels holding stale entity handles across scene/project transitions.

## Fix applied

- Added `Selection::ClearSelection()`
- Cleared selection on scene-context switches and when entering project hub
- Tightened `Entity::IsValid()` to check both handle validity and registry membership

## Final verdict

- PASS
- No blocking issues remain after re-review

## Validation

- `cmake --build build --config Debug --target HuaEngine Editor`
- Editor process stayed alive for 3 seconds during smoke launch
