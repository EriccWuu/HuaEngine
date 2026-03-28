# 调研汇总：engine-editor-capability-expansion

本轮调研共覆盖 3 个主题：

- `interaction-command-spine`
- `selection-and-batch-edit`
- `context-input-extension`

## 汇总结论

### 1. 需要新增 Editor 侧交互命令骨架

当前仓库已有正式领域操作面 `ApplicationOperations`，但没有 Editor 自身的交互命令层。  
因此 `Undo/Redo`、菜单动作、快捷键、批量删除和拖拽都不应继续散落在面板代码中，而应统一归口到 Editor 侧命令骨架。

### 2. 选择模型必须从单选升级为集合模型

当前 `Selection` 仍是单个实体静态状态，无法可靠承载多选与批量删除。  
本阶段应把单选定义为选择集合大小为 `1` 的特例，并让 `Inspector` 在多选场景下先退化为摘要 / 批量动作面，而不是强做完整联合编辑。

### 3. 上下文菜单、快捷键、拖拽都应先形成轻量扩展面

本阶段不适合继续走纯硬编码路径。  
更合理的做法是建立轻量注册面：

- 右键菜单按上下文注册
- 快捷键按命令注册
- 拖拽按交互意图注册

这样第一批功能可以先落在 `Hierarchy / Inspector`，同时保留未来继续扩展的通路。

## 对技术计划的直接影响

- 计划必须包含编辑器交互核心层，而不是只改 UI 面板
- 计划必须定义选择模型升级路径
- 计划必须把 `Hierarchy / Inspector` 作为首批能力接入点
- 计划必须明确哪些能力本阶段要完整落地，哪些只搭扩展缝
