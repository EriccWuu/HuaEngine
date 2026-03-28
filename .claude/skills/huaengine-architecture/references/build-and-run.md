# Build And Run

## Standard Entry

The preferred human/agent build entry is the repository root script:

```bash
Build.bat --generate-project
Build.bat --config debug
Build.bat --config release --target editor
Build.bat --target projecthub --launch-after-build
```

Argument rules:

- `--generate-project`: generate the Visual Studio 2022 x64 project
- `--config debug|release`: build configuration, default `debug`
- `--target huaengine|editor|projecthub|all`: build target, default `all`
- `--launch-after-build`: launch the built host when applicable

The script supports argument reordering as long as values still immediately follow `--config` or `--target`.

## Script Layout

- root entry: `Build.bat`
- child scripts: `Scripts/`
- `Scripts/BuildMain.bat`: argument parsing and orchestration
- `Scripts/GenerateProject.bat`: project generation
- `Scripts/EnsureProject.bat`: lazy project generation
- `Scripts/RunBuild.bat`: unified `cmake --build`
- `Scripts/LaunchTarget.bat`: host launch
- `Scripts/FilterBuildOutput.bat`: build log filtering

## Low-Level Commands

The real underlying build commands are still:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
cmake --build build --config Dist
```

Single-target examples:

```bash
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target ProjectHub
cmake --build build --config Debug --target HuaEngineHeadless
```

## Output Layout

Executables are emitted to:

- `build/bin/<Config>-Windows-x64/`

Smoke targets are emitted to:

- `build/bin/<Config>-Windows-x64/smoke/`

Common examples:

- `build/bin/Debug-Windows-x64/Editor.exe`
- `build/bin/Debug-Windows-x64/ProjectHub.exe`
- `build/bin/Debug-Windows-x64/HuaEngineHeadless.exe`
- `build/bin/Debug-Windows-x64/smoke/ProjectWorkbenchSmoke.exe`

## Asset Copy Behavior

- `Editor/CMakeLists.txt` and `ProjectHub/CMakeLists.txt` copy required shared resources into runtime output directories as needed
- Headless hosts do not depend on GUI asset-copy behavior

When debugging runtime asset issues, verify:

- the host was rebuilt
- the output `Resources/` directory is current
- the process is running from the expected output directory

## Editor Project Workbench

The preferred product entry is now `ProjectHub.exe`.

Current launcher/workbench flow:

- start in `ProjectHub.exe`
- create/open/resume a project through the launcher host
- launch `Editor.exe --project <path> [--scene <path>]`
- enter `Workbench Shell`
- create/open/save/save-as scene documents through the same formal control layer
- persist the last active session to `%LOCALAPPDATA%/HuaEngine/Editor/session.json`

Default host shell sizing currently differs by host:

- `ProjectHub.exe`: smaller launcher-oriented default window
- `Editor.exe`: large workbench-oriented default window

The primary closure smoke for this flow is `ProjectWorkbenchSmoke`.

## Smoke Targets

Current formal smoke coverage includes:

- `ProjectServiceSmoke`
- `SceneServiceSmoke`
- `AssetServiceSmoke`
- `ScriptServiceSmoke`
- `ValidationServiceSmoke`
- `ApplicationServicesSmoke`
- `ApplicationOperationsSmoke`
- `AgentHostAdapterSmoke`
- `HeadlessHostSmoke`
- `HeadlessWorkflowSmoke`
- `RenderingOperationsSmoke`
- `HostConsistencySmoke`
- `ProjectWorkbenchSmoke`

## Common Notes

- This is a multi-config Visual Studio project; actual configuration comes from `--config`
- `VS_STARTUP_PROJECT` is set to `ProjectHub`
- `HuaEngine` uses the precompiled header `src/enginepch.h`
- `HuaEngineHeadless` stdout is a formal machine-readable interface
