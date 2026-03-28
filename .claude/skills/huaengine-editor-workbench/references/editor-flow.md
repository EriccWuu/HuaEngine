# Editor 流程

## 1. 产品入口

当前产品入口已经拆成两个 GUI 宿主：

- `ProjectHub.exe`：权威无项目启动器
- `Editor.exe`：项目绑定工作台

Editor 入口仍然是：

- `Editor/src/EditorApp.cpp`
- `EditorApp : Application`
- `PushLayer(new EditorLayer(spec))`
- 共享 `main()` 仍来自 `HuaEngine/EntryPoint.h`

## 2. 启动状态

在 `Editor.exe` 内，当前主要 GUI 状态只有两种：

- 无项目时的最小 fallback / redirect 面
- 项目激活后的 `Workbench Shell`

完整的新建 / 打开 / 恢复启动器体验已经不再放在 `EditorLayer` 里。

## 3. ProjectHub 宿主形态

`ProjectHub.exe` 不再是“大窗口里居中一张卡片”的旧样子。

当前启动器行为：

- 独立 GUI 宿主
- 较小的启动器默认窗口
- 全窗口启动器工作区
- 双栏布局：
  - 左侧：恢复和状态
  - 右侧：项目路径、项目名、创建 / 打开动作

它应该被理解成独立启动器，而不是嵌在 Editor 里的面板。

## 4. 会话恢复

`ProjectHub.exe` 和 `Editor.exe` 都会读取 `EditorSessionStorage`。

如果启动参数包含 `--project <path>`，Editor 会跳过启动器行为直接打开项目。
如果还带了 `--scene <path>`，则会在项目激活后继续打开该场景。

持久化字段包括：

- `LastProjectRoot`
- `LastProjectName`
- `LastScenePath`

存储位置：

- `%LOCALAPPDATA%/HuaEngine/Editor/session.json`

## 5. 项目激活

项目激活是正式状态切换，而不是临时 demo 初始化。

当前主链：

1. 启动器或命令行先解析项目目标
2. 建立 `ProjectSession`
3. 初始化工作台 shell
4. 恢复最近场景或打开指定场景
5. 持久化当前活动会话

## 6. 场景文档生命周期

Editor 当前围绕 `SceneDocument` 工作，而不是裸 `Ref<Scene>`。

当前文档动作包括：

- `New Scene`
- `Open Scene`
- `Save Scene`
- `Save Scene As`
- `Validate Scene`

文档自身负责：

- 场景路径
- 显示名
- dirty 状态
- 最近一次校验结果

## 7. 交互流程

第一批编辑器交互链路是：

1. 面板输入、菜单或快捷键进入 `EditorLayer`
2. `EditorLayer` 通过 `EditorInteractionHost` 路由
3. `EditorCommandStack` 执行命令并维护 undo/redo 历史
4. `SceneDocument` dirty 状态和命令历史同步
5. `EditorWorkbenchState` 与诊断面刷新

当前首批动作包括：

- 创建实体
- 删除选中实体
- 添加组件
- 删除组件
- Undo
- Redo
- 保存当前场景文档

当前内置快捷键包括：

- `Ctrl+Z`
- `Ctrl+Y`
- `Ctrl+Shift+N`
- `Delete`
- `Ctrl+S`

## 8. OnGuiRender 组合

`OnGuiRender()` 当前组合的是：

- 无项目时的最小 fallback / redirect 面
- 有项目时的 DockSpace 和 Workbench Shell
- Project 面板
- Scene 面板
- Hierarchy
- Inspector
- Console

## 9. 默认布局

当前默认工作台布局是：

- 左侧：`Project`
- 左中：`Hierarchy`
- 中间：`Scene`
- 右侧：`Inspector`
- 底部：`Console`

## 相关 Skill

- 看运行时启动和宿主 shell：转 `huaengine-core-runtime/references/lifecycle-and-events.md`
- 看视口渲染流：转 `huaengine-rendering/references/runtime-flow.md`
- 看场景 / 实体 / 组件运行时事实：转 `huaengine-ecs-scene/references/runtime-structure.md`
