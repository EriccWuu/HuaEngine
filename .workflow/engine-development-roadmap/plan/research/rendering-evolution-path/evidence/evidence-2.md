# Evidence E-2

- 主题: rendering-evolution-path
- 来源: `.claude/skills/huaengine-rendering/references/assets-and-materials.md`
- 来源类型: 资源与材质说明
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - Mesh/Material 当前已同时涉及 CPU 数据、GPU 资源、路径解析和序列化。
  - `MeshManager`、`MaterialSerializer` 和路径恢复目前仍依赖显式注册与名称/路径约定。
  - 资源读档错误常常不是 draw call 问题，而是上游资产恢复问题。
- 对规划的意义:
  - 渲染强化前必须先稳定资产边界，否则渲染问题会与资源问题混在一起。
