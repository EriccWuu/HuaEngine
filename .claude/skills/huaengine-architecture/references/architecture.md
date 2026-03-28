# 架构参考

## 1. 构建图

根 `CMakeLists.txt` 当前维护的顶层目标是：

- `HuaEngine`
- `Editor`
- `ProjectHub`
- `HuaEngineHeadless`
- `Tests/` 下的 smoke / 回归目标

`Sandbox` 已经从正式宿主图中移除。

## 2. 宿主职责

### ProjectHub

- 独立启动器宿主
- 首选无项目入口
- 负责创建 / 打开 / 恢复项目
- 负责拉起 `Editor.exe --project [--scene]`

### Editor

- 项目绑定的 GUI 工作台
- 持有场景视口、Hierarchy、Inspector、Console、Project 面板
- 通过 `ApplicationOperations` 消费引擎正式能力

### Headless

- 正式 CLI / 无 GUI 入口
- 消费和 GUI 相同的正式操作面
- 输出机器可读 JSON

## 3. 引擎核心

引擎库当前仍然负责：

- `ApplicationServices / ApplicationOperations`
- 项目、场景、资产、脚本、验证服务
- ECS 与场景运行时
- 渲染与 OpenGL 后端
- 序列化与反射

不同宿主的差异主要在 shell 和呈现方式，不在领域能力所有权上。

## 4. 当前阅读顺序

排查问题时，建议按这个顺序：

1. 先识别宿主边界
2. 再判断问题属于正式操作层还是引擎内部实现
3. 最后再深入场景 / 渲染 / 序列化细节

## 5. 关键文件

- `CMakeLists.txt`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Headless/src/main.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
