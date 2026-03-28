# Evidence E-1

- 主题：编辑器交互命令骨架
- 证据类型：本地代码观察
- 来源文件：
  - `Editor/src/EditorLayer.cpp`
  - `Editor/src/Panels/HierarchyPanel.cpp`
  - `Editor/src/Panels/InspectorPanel.cpp`
- 关键发现：
  - 当前 `EditorLayer` 已经承担工作台菜单、场景文档动作、面板显隐等编排职责。
  - `HierarchyPanel` 和 `InspectorPanel` 目前主要是直接 UI 行为，没有统一命令层。
  - `InspectorPanel` 只返回 `changed` 布尔值给 `EditorLayer`，说明当前编辑动作仍然是“局部 UI 改值 -> 外部补 dirty”，没有抽象成统一编辑命令。
- 结论支持：
  - 需要新增编辑器内部命令骨架，把 UI 触发动作统一抽象出来，避免继续在各面板内散写交互逻辑。

