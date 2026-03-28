# Implement Report: engine-project-hub-shell-separation

## Outcome

- `Project Hub` was detached from the global DockSpace render path.
- The full `Workbench Shell` now renders only after a project session is active.
- The editor workbench documentation now reflects the new startup model.

## Verification

- `cmake --build build --config Debug --target Editor`
- launch `Editor.exe` without `--project` and verify process stays alive in entry mode
- launch `Editor.exe --project .` from a project root and verify process stays alive in workbench mode

## Notes

- This refinement is a follow-up slice on top of `engine-project-workbench-closure`.
