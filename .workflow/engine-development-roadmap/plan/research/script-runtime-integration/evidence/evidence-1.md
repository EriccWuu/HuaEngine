# Evidence E-1

- 主题: script-runtime-integration
- 来源: `.claude/skills/huaengine-ecs-scene/references/runtime-structure.md`
- 来源类型: ECS/Scene 结构说明
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - `Scene::Update()` 当前只遍历已注册 system。
  - `ScriptableEntity` / `NativeScriptComponent` 已存在，但没有发现统一调度链路。
  - 当前脚本能力更像未完成能力而非已接通功能。
- 对规划的意义:
  - 脚本继承若要成为基础能力，必须先纳入 Scene 生命周期和系统调度。
