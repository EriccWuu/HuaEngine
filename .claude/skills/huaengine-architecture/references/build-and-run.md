# Build And Run

## 标准命令

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
```

## 输出位置

根 `CMakeLists.txt` 和三个子工程共同决定输出目录：

- 可执行文件输出到 `build/bin/<Config>-Windows-x64/`
- `HuaEngine` 静态库输出到 `build/bin/<Config>-Windows-x64/HuaEngine/`

常见示例：

- `build/bin/Debug-Windows-x64/Editor.exe`
- `build/bin/Debug-Windows-x64/Sandbox.exe`

## 资源复制行为

- `Editor/CMakeLists.txt` 在构建后会把 `Editor/assets/` 复制到目标目录下的 `assets/`
- `Sandbox/CMakeLists.txt` 在构建后会把 `Sandbox/assets/` 复制到目标目录下的 `assets/`

因此运行资源相关问题时，先确认：

- 对应目标是否已重新构建
- 目标输出目录下的 `assets/` 是否是最新内容
- 运行工作目录是否仍指向构建输出目录

## 运行入口

- `Editor` 的应用入口在 `Editor/src/EditorApp.cpp`
- `Sandbox` 的应用入口在 `Sandbox/src/SandboxApp.cpp`
- 两者都通过 `HuaEngine/EntryPoint.h` 共享 `main()`

## 构建排查顺序

1. 先确认修改落在哪个目标：`HuaEngine`、`Editor` 或 `Sandbox`
2. 只改应用层时，优先单独构建对应应用目标
3. 改动引擎头文件、渲染层、序列化层或公共 API 时，至少重新构建 `HuaEngine` 和受影响应用
4. 如果是资源或运行时异常，构建后检查输出目录里的 `assets/` 是否同步

## 常见注意点

- 这是 Visual Studio 多配置工程，实际配置以 `--config Debug/Release/Dist` 为准，不要只依赖单配置生成器思路。
- 根 `CMakeLists.txt` 还设置了 `VS_STARTUP_PROJECT Editor`，所以 IDE 中默认启动项通常是 `Editor`。
- `HuaEngine` 使用预编译头 `src/enginepch.h`；新增高频头依赖时可以检查是否需要同步到预编译头策略中。
