# Build And Run

## 标准入口

项目当前首选的人工/Agent 构建入口是仓库根统一主脚本：

```bash
Build.bat --generate-project
Build.bat --config debug
Build.bat --config release --target editor
Build.bat --target sandbox --launch-after-build
```

参数语义：

- `--generate-project`：生成 Visual Studio 2022 x64 工程
- `--config debug|release`：指定构建配置，默认 `debug`
- `--target huaengine|editor|sandbox|all`：指定构建目标，默认 `all`
- `--launch-after-build`：构建后自动启动；当前只会真正启动 `editor` 或 `sandbox`

脚本组织：

- 根入口：`Build.bat`
- 子脚本目录：`Scripts/`
- `Scripts/BuildMain.bat`：参数解析和总调度
- `Scripts/GenerateProject.bat`：工程生成
- `Scripts/EnsureProject.bat`：缺失工程时自动补生成
- `Scripts/RunBuild.bat`：统一调用 `cmake --build`
- `Scripts/LaunchTarget.bat`：启动目标程序
- `Scripts/FilterBuildOutput.bat`：构建日志过滤

## 底层命令

项目约定的标准构建方式来自仓库根 `CLAUDE.md`：

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
cmake --build build --config Dist
```

构建单个目标：

```bash
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target Sandbox
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target HuaEngineHeadless
```

这些 `cmake` 命令仍然是底层真实行为；`Build.bat` 只是把它们收束成单一入口。

## 输出位置

根 `CMakeLists.txt` 和四类子工程共同决定输出目录：

- 可执行文件输出到 `build/bin/<Config>-Windows-x64/`
- `HuaEngine` 静态库输出到 `build/bin/<Config>-Windows-x64/HuaEngine/`

常见示例：

- `build/bin/Debug-Windows-x64/Editor.exe`
- `build/bin/Debug-Windows-x64/Sandbox.exe`
- `build/bin/Debug-Windows-x64/HuaEngineHeadless.exe`

## 资源复制行为

- `Editor/CMakeLists.txt` 在构建后会先把 `Editor/assets/` 复制到目标目录下的 `assets/`，再把 `Sandbox/assets/` 合并复制进去，保证 Editor 默认示例场景能直接消费 sandbox 材质、mesh、shader 和纹理资源
- `Sandbox/CMakeLists.txt` 在构建后会把 `Sandbox/assets/` 复制到目标目录下的 `assets/`
- `Headless` 不依赖 GUI 资源复制；排查 headless 行为时优先看命令参数、工作目录和结构化 JSON 输出

因此运行资源相关问题时，先确认：

- 对应目标是否已重新构建
- 目标输出目录下的 `assets/` 是否是最新内容
- 运行工作目录是否仍指向构建输出目录

## 运行入口

- `Editor` 的应用入口在 `Editor/src/EditorApp.cpp`
- `Sandbox` 的应用入口在 `Sandbox/src/SandboxApp.cpp`
- `Headless` 的应用入口在 `Headless/src/main.cpp`
- 三者都通过同一套 `Application` runtime 共享启动约束

统一脚本的启动行为：

- `Build.bat --target editor --launch-after-build` 会启动 `Editor.exe`
- `Build.bat --target sandbox --launch-after-build` 会启动 `Sandbox.exe`
- `Build.bat --target huaengine --launch-after-build` 和 `--target all --launch-after-build` 会跳过启动，因为这两个目标没有单一可执行入口

## Headless 与 Smoke

当前仓库已经有一组正式 smoke targets，可直接验证控制面是否回归：

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

## 构建排查顺序

1. 先确认修改落在哪个目标：`HuaEngine`、`Editor`、`Sandbox` 还是 `Headless`
2. 只改应用层时，优先单独构建对应应用目标
3. 改动引擎头文件、渲染层、序列化层或公共 API 时，至少重新构建 `HuaEngine` 和受影响宿主/测试目标
4. 如果是控制面或 host consistency 问题，优先补跑 `HeadlessWorkflowSmoke`、`AgentHostAdapterSmoke`、`HostConsistencySmoke`
5. 如果是资源或运行时异常，构建后检查输出目录里的 `assets/` 是否同步

## 常见注意点

- 这是 Visual Studio 多配置工程，实际配置以 `--config Debug/Release/Dist` 为准，不要只依赖单配置生成器思路。
- `Build.bat` 当前支持参数乱序，只要值仍然紧跟在 `--config` 或 `--target` 后面即可，例如 `Build.bat --target editor --config debug`。
- 根 `CMakeLists.txt` 还设置了 `VS_STARTUP_PROJECT Editor`，所以 IDE 中默认启动项通常是 `Editor`。
- `HuaEngine` 使用预编译头 `src/enginepch.h`；新增高频头依赖时可以检查是否需要同步到预编译头策略中。
- `HuaEngineHeadless` 的 stdout 是正式机器可读接口；排查 CLI/AI/automation 回归时，优先看 JSON 结果而不是 console 日志。
