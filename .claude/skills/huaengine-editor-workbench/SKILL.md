---
name: huaengine-editor-workbench
description: >
  HuaEngine Editor 工作台导航。覆盖 EditorApp、EditorLayer、ProjectHub 交接、
  ProjectSession、SceneDocument、EditorWorkbenchState、ProjectPanel、Selection、
  Hierarchy、Inspector、Console 和编辑器交互核心。适用于回答 Editor 启动、
  项目工作台流程、场景文档生命周期、面板行为、撤销重做、快捷键、右键菜单、
  选择模型以及编辑器改动落点这类问题。
---

# HuaEngine Editor 工作台

## 概览

这个 Skill 是当前 Editor 工作台实现的主导航入口。
当问题是：

- Editor 怎么启动并进入工作台
- GUI 面板如何消费项目与场景状态
- Editor 如何通过 `ApplicationOperations` 驱动正式能力
- 工作台交互如何接入命令栈、快捷键、右键菜单

优先从这里进入。

## 模块边界

- `Editor/src/EditorApp.cpp`
  - Editor 进程入口和 `CreateApplication()`
- `Editor/src/EditorLayer.*`
  - 最小 fallback、Workbench Shell、视口壳、场景文档流
- `Editor/src/Interaction/`
  - 命令栈、快捷键注册、右键菜单注册、拖拽意图注册、场景命令工厂
- `Editor/src/Workbench/`
  - `ProjectSession`、`SceneDocument`、`EditorWorkbenchState`、`EditorSessionStorage`
- `Editor/src/Panels/`
  - `ProjectPanel`、`HierarchyPanel`、`InspectorPanel`、`ConsolePanel`
- `Editor/src/Selection.*`
  - 全局选择状态
- `Editor/src/ComponentEditor*.h`
  - 组件编辑注册表与反射驱动编辑
- `ProjectHub/src/*`
  - 独立启动器宿主与启动器界面

## 当前工作台模型

- 首选无项目入口是 `ProjectHub.exe`
- `Editor.exe` 现在主要是项目绑定工作台宿主
- Editor fallback 不再承担完整的新建 / 打开启动器体验
- `ProjectHub.exe` 使用较小启动器窗口和全窗口启动器布局
- 项目激活后建立 `ProjectSession`
- 场景编辑围绕 `SceneDocument`
- 面板摘要缓存到 `EditorWorkbenchState`
- 最近会话通过 `EditorSessionStorage` 持久化
- GUI 动作统一通过 `ApplicationOperations`
- 对象编辑动作走 Editor 交互核心和命令栈
- 场景 / 实体 / 组件写操作现在与 Headless 共用同一正式操作面

## 核心规则

- `EditorApp` 仍然是薄宿主，只负责 push `EditorLayer`
- `EditorLayer` 负责 fallback、Workbench Shell、视口绑定和面板协同
- GUI 不直接拥有项目或场景领域逻辑，而是消费 `ApplicationOperations`
- `ProjectSession` 是当前 GUI 项目上下文
- `SceneDocument` 负责当前场景文档的路径 / 显示名 / dirty / 校验语义
- `EditorWorkbenchState` 是面板、诊断和最近结果的共享摘要面
- `Selection` 仍是全局静态状态，但现在是集合模型，单选只是 primary-selection 特例
- Inspector 编辑会把 dirty 状态回写到当前 `SceneDocument`
- 第一批编辑动作应表达为 editor commands，这样才能进入 undo/redo 和 dirty tracking
- `Hierarchy` / `Inspector` 每帧读取可以直读运行时场景，但共享写逻辑不能再回到面板内部

## 关键文件

- `Editor/src/EditorApp.cpp`
- `Editor/src/EditorLayer.h`
- `Editor/src/EditorLayer.cpp`
- `ProjectHub/src/ProjectHubApp.cpp`
- `ProjectHub/src/ProjectHubLayer.h`
- `ProjectHub/src/ProjectHubLayer.cpp`
- `Editor/src/Workbench/ProjectSession.h`
- `Editor/src/Workbench/SceneDocument.h`
- `Editor/src/Workbench/EditorWorkbenchState.h`
- `Editor/src/Workbench/EditorSessionStorage.h`
- `Editor/src/Workbench/EditorSessionStorage.cpp`
- `Editor/src/Interaction/EditorInteractionHost.h`
- `Editor/src/Interaction/EditorCommandStack.h`
- `Editor/src/Interaction/EditorSceneCommands.h`
- `Editor/src/Interaction/ShortcutRegistry.h`
- `Editor/src/Interaction/ContextMenuRegistry.h`
- `Editor/src/Interaction/DragDropIntentRegistry.h`
- `Editor/src/Panels/ProjectPanel.h`
- `Editor/src/Panels/ProjectPanel.cpp`
- `Editor/src/Panels/HierarchyPanel.h`
- `Editor/src/Panels/HierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.h`
- `Editor/src/Panels/InspectorPanel.cpp`
- `Editor/src/Panels/ConsolePanel.h`
- `Editor/src/Panels/ConsolePanel.cpp`

## 导航

- 看启动流、启动器交接、会话恢复、场景文档流：读 `references/editor-flow.md`
- 看面板行为、选择模型、dirty tracking、工作台摘要：读 `references/panels-and-selection.md`
- 看 GUI 背后的场景 / 实体 / 组件运行时事实：转 `huaengine-ecs-scene`
- 看视口渲染行为：转 `huaengine-rendering`
- 看运行时循环、ImGui 层、窗口和输入胶水层：转 `huaengine-core-runtime`

## 跨 Skill 导航

- 如果问题本质上是场景序列化、组件所有权或场景运行时事实：转 `huaengine-ecs-scene`
- 如果问题本质上是视口渲染、FrameBuffer resize、EditorCamera 或材质 / mesh 可见性：转 `huaengine-rendering`
- 如果问题本质上是应用循环、窗口生命周期或运行时日志 / 输入：转 `huaengine-core-runtime`
- 如果 Inspector 的反射编辑或字段绘制出了问题：转 `huaengine-serialization-reflection`

## 常见误区

- `ProjectHub.exe` 已经是首选无项目入口，不要再假设完整启动器体验仍在 Editor 启动里
- `ProjectPanel` 只是轻量项目摘要面，不是完整资产浏览器
- `HierarchyPanel` 仍通过 `TransformComponent` 枚举实体
- `Selection` 是全局静态状态，场景切换处理不好仍然会出现 stale handle
- `Hierarchy` 的拖拽目前只是 intent surface，不是真正的父子层级系统
- `Inspector` 的多选仍是摘要模式，不是完整批量编辑
- `Inspector` 的组件增删交互已经分开：空白处右键打开浮动 `Add Component` 窗口，组件删除在组件头部菜单里
- GUI 摘要看起来正确，不代表底层运行时就一定正确；需要和 `ApplicationOperations`、`ProjectWorkbenchSmoke`、`EditorInteractionSmoke` 一起验证
