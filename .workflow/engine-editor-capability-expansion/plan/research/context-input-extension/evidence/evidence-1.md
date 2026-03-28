# Evidence E-1

- 主题：上下文菜单、快捷键与拖拽接入面
- 证据类型：本地代码观察
- 来源文件：
  - `Editor/src/EditorLayer.cpp`
  - `Editor/src/Panels/HierarchyPanel.cpp`
- 关键发现：
  - 当前 `EditorLayer` 已经有顶层菜单栏与弹窗交互，但仍是硬编码方式。
  - `HierarchyPanel` 目前没有标准化上下文菜单入口，仅有基本列表与点击行为。
- 结论支持：
  - 当前仓库适合把“输入触发与上下文动作注册”补成正式扩展面，而不是继续在各面板手写散逻辑。

