# 调研: project-workbench-topology

## 主题

Editor 如何从固定 workbench 壳演进为显式项目工作台。

## 当前代码观察

- `EditorLayer::InitializeWorkbenchContext()` 当前总是创建/复用 `%LOCALAPPDATA%/HuaEngine/Workbench`
- `ProjectService` 已有正式 `InitializeProject / ResolveProjectContext / CheckProjectStatus`
- `ApplicationOperations` 已公开 GUI 可直接消费的项目操作

## 结论

### 1. Editor 需要显式的“项目入口模式”

当前 Editor 只有一个隐式启动模式: 自动进固定 workbench。要建立闭环，至少需要 3 个模式:

- `NoProject`: 尚未打开工程，只显示项目入口界面
- `ProjectLoaded`: 已经打开工程，但场景文档尚未绑定
- `SceneLoaded`: 工程与场景文档都已经装载

### 2. 项目上下文必须成为 Workbench 一级状态

如果仍然把 `ProjectContext` 隐藏在初始化流程里，后续“切换工程、关闭工程、显示工程信息、恢复上次工程”都会继续绕。

### 3. 当前阶段适合“嵌入式 Project Hub”，不适合另起复杂启动器

为了最小闭环，最便宜的路径是在现有 Editor 主壳内提供一个 `Project Hub / Project Panel` 状态，而不是单独做一个完整启动器程序或复杂欢迎页系统。

## 对规划的约束

- 工程入口必须显式化
- 当前工程必须是 Editor 工作台的一等对象
- 关闭/切换工程时需要有明确的工作台状态重置路径

---

*Research note | 2026-03-28*
