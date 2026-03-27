# Evidence E-2

- 主题: host-topology
- 来源: `.claude/skills/huaengine-core-runtime/references/lifecycle-and-events.md` 与 `.claude/skills/huaengine-editor-workbench/references/editor-flow.md`
- 来源类型: 本地模块资料
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - 当前启动链统一经过 `Application` 和 `EntryPoint.h`。
  - `EditorLayer` 目前同时承担工作台与示例场景装配，说明 GUI 仍是能力入口的一部分。
- 对规划的意义:
  - Headless 宿主应复用 runtime 启动链，但不能继续复用 EditorLayer 的职责耦合方式。
