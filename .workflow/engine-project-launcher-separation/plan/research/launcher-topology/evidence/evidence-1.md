# E-1: Current Editor startup contract

- Time: 2026-03-28
- Source type: repository code
- Confidence: A

## Observation

`Editor/src/EditorApp.cpp` already parses `--project` and `--scene` and forwards them into `EditorLayerSpecification`.

## Key Finding

The repository already has a stable launcher-to-editor contract candidate:

- `Editor.exe --project <path>`
- optional `--scene <path>`

This means a separate launcher does not need IPC for the first milestone. It can hand off through command-line process launch.

