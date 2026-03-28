# Evidence E-1

- 主题：选择、多选与批量编辑模型
- 证据类型：本地代码观察
- 来源文件：
  - `Editor/src/Selection.h`
  - `Editor/src/Panels/HierarchyPanel.cpp`
  - `Editor/src/Panels/InspectorPanel.cpp`
- 关键发现：
  - `Selection` 当前是单个 `Entity` 的全局静态状态。
  - `HierarchyPanel` 当前仅支持单击后设置单个选中对象。
  - `InspectorPanel` 当前完全建立在单选对象上。
- 结论支持：
  - 当前选择模型不足以直接承载多选、批量删除和批量操作。

