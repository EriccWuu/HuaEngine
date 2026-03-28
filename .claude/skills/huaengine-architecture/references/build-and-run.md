# 构建与运行

## 1. 标准入口

当前推荐的人类 / Agent 构建入口是仓库根脚本：

```bash
Build.bat --generate-project
Build.bat --config debug
Build.bat --config release --target editor
Build.bat --target projecthub --launch-after-build
```

参数规则：

- `--generate-project`：生成 VS2022 x64 工程
- `--config debug|release`：构建配置，默认 `debug`
- `--target huaengine|editor|projecthub|all`：构建目标，默认 `all`
- `--launch-after-build`：在适用时启动目标宿主

脚本支持参数乱序，但 `--config` 和 `--target` 的值仍需紧跟参数本身。

## 2. 脚本结构

- 根入口：`Build.bat`
- 子脚本目录：`Scripts/`
- `Scripts/BuildMain.bat`：参数解析与总调度
- `Scripts/GenerateProject.bat`：生成工程
- `Scripts/EnsureProject.bat`：惰性补生成
- `Scripts/RunBuild.bat`：统一 `cmake --build`
- `Scripts/LaunchTarget.bat`：启动宿主
- `Scripts/FilterBuildOutput.bat`：过滤构建输出

## 3. 底层构建命令

底层真实构建命令仍然是：

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
cmake --build build --config Dist
```

单目标示例：

```bash
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target ProjectHub
cmake --build build --config Debug --target HuaEngineHeadless
```

## 4. 输出布局

可执行文件输出到：

- `build/bin/<Config>-Windows-x64/`

smoke 目标输出到：

- `build/bin/<Config>-Windows-x64/smoke/`

常见示例：

- `build/bin/Debug-Windows-x64/Editor.exe`
- `build/bin/Debug-Windows-x64/ProjectHub.exe`
- `build/bin/Debug-Windows-x64/HuaEngineHeadless.exe`
- `build/bin/Debug-Windows-x64/smoke/ProjectWorkbenchSmoke.exe`

## 5. 资源复制行为

- `Editor/CMakeLists.txt` 和 `ProjectHub/CMakeLists.txt` 会把所需共享资源复制到运行目录
- `Headless` 不依赖 GUI 的资源复制行为

排查运行时资源问题时，优先确认：

- 宿主是否刚刚重编过
- 输出目录下的 `Resources/` 是否是最新的
- 进程是否从预期输出目录启动

## 6. Editor 项目工作台

当前首选产品入口是 `ProjectHub.exe`。

当前启动器 / 工作台链路：

- 从 `ProjectHub.exe` 启动
- 在启动器里创建 / 打开 / 恢复项目
- 拉起 `Editor.exe --project <path> [--scene <path>]`
- 进入 `Workbench Shell`
- 通过同一正式控制层执行场景创建 / 打开 / 保存 / 另存
- 将最近会话持久化到 `%LOCALAPPDATA%/HuaEngine/Editor/session.json`

默认窗口尺寸当前按宿主区分：

- `ProjectHub.exe`：更小、更像启动器
- `Editor.exe`：更大、更像工作台

这条工作流的主 smoke 是 `ProjectWorkbenchSmoke`。

## 7. 当前 Editor 交互事实

- `Ctrl+S` 是当前活动场景文档的保存快捷键
- 工作台菜单栏有 `View` 菜单，用于控制主面板显隐
- 当前面板名称是 `Hierarchy`，旧的 `SceneHierarchy` 名称已经不再使用
- 如果一个运行时问题看起来已经跨过单一模块边界，先回看 `docs/development-guidelines.md`

## 8. Smoke 目标

当前正式 smoke 覆盖包括：

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

## 9. 常见说明

- 这是 VS 多配置工程，实际配置由 `--config` 决定
- `VS_STARTUP_PROJECT` 当前设为 `ProjectHub`
- `HuaEngine` 使用预编译头 `src/enginepch.h`
- `HuaEngineHeadless` 的 stdout 是正式机器接口，不是临时调试文本
