---
name: huaengine-architecture
description: >
  HuaEngine 仓库整体架构导航。覆盖构建图、HuaEngine/ProjectHub/Editor/Headless
  目标关系、启动链路、Application 控制层，以及各子系统边界。适用于回答整体架构、
  目录结构、构建目标、宿主拓扑、启动流和“改动应该落在哪一层”这类问题。
---

# HuaEngine 整体架构

## 概览

这个 Skill 是当前仓库的顶层导航入口。
当问题是：

- 这个改动应该落在哪一层
- 当前宿主拓扑是什么
- 构建、运行和控制面是怎么组织的

优先从这里进入。

## 宿主拓扑

- `HuaEngine/`：核心静态库
- `ProjectHub/`：独立的无项目启动器宿主
- `Editor/`：项目绑定的 GUI 工作台宿主
- `Headless/`：正式 CLI / 无 GUI 宿主

`Sandbox` 已经从当前构建图中移除，不再是受维护宿主。

## 核心规则

- 先看根 `CMakeLists.txt` 和宿主入口，再深入子系统内部
- 对宿主暴露的正式能力应统一通过 `ApplicationOperations`
- GUI、headless、自动化宿主不应重新暴露 raw domain-service 入口
- 渲染、场景、资产、脚本、验证这些真实行为仍然属于引擎库，而不是宿主
- 共享写操作必须维持唯一权威入口，不能再让 `Editor` 和 `Headless` 各长一套
- GUI 每帧读取可以按需直读运行时状态，但可复用的共享查询不应继续长在面板里

## 关键文件

- `CMakeLists.txt`
- `HuaEngine/CMakeLists.txt`
- `Editor/CMakeLists.txt`
- `ProjectHub/CMakeLists.txt`
- `Headless/CMakeLists.txt`
- `HuaEngine/src/HuaEngine/Application.h`
- `HuaEngine/src/HuaEngine/Application.cpp`
- `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- `HuaEngine/src/HuaEngine/EntryPoint.h`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.cpp`
- `Headless/src/main.cpp`
- `Headless/src/HeadlessCommandRunner.cpp`

## 导航

- 看构建入口、输出目录、运行方式：读 `references/build-and-run.md`
- 看子系统分层和宿主边界：读 `references/architecture.md`
- 看仓库级开发约束：读 `docs/development-guidelines.md`
- 看启动器和工作台行为：转 `huaengine-editor-workbench`
- 看渲染主链：转 `huaengine-rendering`
- 看运行时生命周期：转 `huaengine-core-runtime`
