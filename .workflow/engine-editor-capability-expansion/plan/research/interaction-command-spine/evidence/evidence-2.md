# Evidence E-2

- 主题：编辑器交互命令骨架
- 证据类型：本地接口观察
- 来源文件：
  - `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
  - `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- 关键发现：
  - `ApplicationOperations` 当前覆盖项目、场景、资产、脚本、渲染、验证等正式领域能力。
  - 但实体增删、组件增删、编辑器撤销重做、快捷键、上下文菜单等仍然不在统一交互层中。
  - 这说明“领域操作面”已经存在，但“编辑器交互操作面”还没有建立。
- 结论支持：
  - 需要在 Editor 侧补一层面向 GUI 的交互命令骨架，而不是把所有行为都硬塞进 `ApplicationOperations`。

