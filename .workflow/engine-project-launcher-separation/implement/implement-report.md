# Implement Report

## Outcome

`engine-project-launcher-separation` is now complete. The no-project entry surface has been separated from the Editor workbench host.

The implemented closure is:

1. launch `ProjectHub.exe` as the authoritative no-project entry
2. create or open a project in the launcher
3. resume the last saved project from the launcher
4. hand off to `Editor.exe --project [--scene]`
5. keep `Editor.exe` focused on project-bound workbench behavior
6. retain a minimal fallback redirect surface when `Editor.exe` is started without a project

## Major Changes

- Added shared host launch utilities in `HostLaunch`
- Added `Application::RequestShutdown()` for host handoff and clean self-exit
- Added a standalone `ProjectHub` target and launcher layer
- Reused `ApplicationOperations` as the authoritative project create/open/status boundary
- Reduced the Editor no-project entry to a minimal redirect surface
- Extended build scripts and CMake target surface to treat `ProjectHub` as a first-class host
- Updated workbench and build/run documentation plus module Skills

## Validation Summary

- Build validated: `HuaEngine`, `Editor`, `ProjectHub`
- Runtime validated: `ProjectHub.exe` stayed alive during launch smoke
- Runtime validated: `Editor.exe` stayed alive in fallback mode during launch smoke
- Runtime validated: `Editor.exe --project .` from `Tests` stayed alive during launch smoke
- Local review result: PASS

## Residual Risks

- Launcher and Editor are separated by process handoff only; there is no richer IPC/session bridge yet
- The Editor still keeps a minimal fallback entry for resilience when started without `--project`
- GUI interaction beyond process-liveness smoke has not been automated yet
