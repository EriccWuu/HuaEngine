# Review Round 2

## Scope

- T2.1

## Initial findings

- Critical: `OnAttach()` still auto-opened the fixed LOCALAPPDATA workbench path, so Project Hub was not the real entry
- Major: Open Project still forced a transient new scene, coupling project activation with scene lifecycle

## Fix applied

- Removed startup auto-open from `OnAttach()`
- Kept Project Hub as the primary entry state
- Split project activation from scene creation
- Allowed workbench shell to run with no active scene document
- Added a no-scene placeholder in the Scene panel

## Final verdict

- PASS
- No blocking issues remain after re-review

## Validation

- `cmake --build build --config Debug --target Editor`
- Editor process stayed alive for 3 seconds during smoke launch
