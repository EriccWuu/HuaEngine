# 面板与选择

## 1. 选择模型

`Selection` 仍然是全局静态状态，但已经不再是单实体模型：

- 内部是选中 `Entity` 的 vector
- `SetSelection(...)`
- `SetSelections(...)`
- `AddToSelection(...)`
- `ToggleSelection(...)`
- `RemoveFromSelection(...)`
- `GetSelection()`
- `GetPrimarySelection()`
- `GetSelections()`
- `HasSelection()`
- `HasSingleSelection()`
- `ClearSelection()`

单选现在只是“第一个元素 / 主选中”的特例。

## 2. ProjectPanel

`ProjectPanel` 是当前工作台里的项目摘要面。

它消费 `EditorWorkbenchState`，展示：

- 当前项目根目录
- 当前项目名
- 当前场景显示名
- dirty 标记
- 最近校验 warning / error 计数
- 轻量 `Assets/` 与 `Scenes/` 摘要

当前动作：

- `Open Scene`
- `Refresh Project`

它是摘要 / 导航面，不是完整内容浏览器。

## 2.1 View 菜单显隐

当前工作台菜单栏有一个 `View` 菜单，用来控制：

- `Project`
- `Hierarchy`
- `Inspector`
- `Console`
- `Scene`

面板显隐当前由 `EditorLayer` 持有，而不是各面板类自己持有。

## 3. HierarchyPanel

当前 Hierarchy：

- 读取活动 `Scene`
- 通过 `registry.view<TransformComponent>()` 枚举实体
- 把每个条目包成 `Entity(entity, &entityManager)`
- 更新 `Selection`
- 显示来自 `EditorWorkbenchState` 的项目 / 场景摘要、最近操作和校验计数
- 暴露 `hierarchy.window` 和 `hierarchy.entity` 这两个上下文菜单面
- 支持普通点击单选和 `Ctrl+Click` 多选切换
- 点击空白背景时清空选择
- 暴露 hierarchy 条目的拖拽意图面
- 空白处右键：当前注册动作包含创建实体
- 实体上右键：当前注册动作包含创建实体和删除选中

当前只有带 `TransformComponent` 的实体会出现在 Hierarchy。

## 4. InspectorPanel

当前 Inspector：

- 检查 `Selection::HasSelection()`
- 读取主选中实体
- 显示项目 / 场景摘要
- 通过 `ComponentEditorRegistry` 画组件编辑面
- 暴露 `inspector.window` 和 `inspector.entity` 上下文菜单面
- 多选时退化成摘要模式
- 添加组件使用独立浮动 `Add Component` 窗口，而不是内联 popup
- 删除组件只保留在组件头部菜单里，不放在 Inspector 空白处菜单里

`InspectorPanel::OnGuiRender()` 当前会返回“这次是否真的改了组件数据”。
`EditorLayer` 会用这个返回值给当前 `SceneDocument` 打 dirty。

## 5. ComponentEditorRegistry

组件编辑注册表当前负责：

- 按 `std::type_index` 注册组件绘制器
- 为每个组件类型维护显示名和绘制函数
- 按注册顺序遍历可编辑组件
- 通过 `DrawComponentEditor(...)` 走反射驱动字段编辑

当前默认编辑面仍然比较轻，不是每个反射类型都有专门的 rich editor。

## 6. ConsolePanel

`ConsolePanel` 当前有两个面：

- `Diagnostics`：来自 `EditorWorkbenchState` 的工作台诊断和校验历史
- `Logs`：来自共享日志 sink 的运行时日志

因此 Console 既是运行时日志面，也是正式工作台反馈面。

## 7. 共享摘要面

当前主面板对摘要的消费是一致的：

- `ProjectPanel`：项目摘要、场景摘要、校验计数
- `HierarchyPanel`：项目摘要、场景摘要、最近操作、校验计数
- `InspectorPanel`：项目摘要、场景摘要、选中实体编辑
- `ConsolePanel`：诊断与运行时日志

## 8. 交互核心

当前第一批编辑器交互模型集中在 `Editor/src/Interaction/`：

- `EditorInteractionHost`：绑定工作台状态、项目会话和场景文档
- `EditorCommandStack`：undo/redo 历史和 dirty 跟踪
- `ContextMenuRegistry`：右键菜单注册面
- `ShortcutRegistry`：内置和未来自定义快捷键注册面
- `DragDropIntentRegistry`：拖拽意图注册面
- `EditorSceneCommands`：首批实体 / 组件命令实现

当前内置快捷键：

- `Ctrl+Z`
- `Ctrl+Y`
- `Ctrl+Shift+N`
- `Delete`
- `Ctrl+S`

## 8.1 共享写边界

当前 Editor 交互的正式意图是：

- 面板负责渲染和采集输入
- 交互宿主负责路由命令
- 命令栈负责 undo/redo 和 dirty 状态
- 具体场景 / 实体 / 组件写操作统一走 `ApplicationOperations`

不要把实体 / 组件写逻辑重新塞回 panel-local 的 registry 直接改写里。

## 相关 Skill

- 看反射驱动编辑和序列化规则：转 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 看场景 / 组件运行时结构：转 `huaengine-ecs-scene/references/runtime-structure.md`
- 看 Inspector / Scene 面板会遇到的渲染资源状态：转 `huaengine-rendering/references/assets-and-materials.md`
- 看 Console / 输入 / 窗口胶水层：转 `huaengine-core-runtime/references/window-input-and-imgui.md`
