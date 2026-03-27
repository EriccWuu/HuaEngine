# Evidence E-2

- 主题: script-runtime-integration
- 来源: `.claude/skills/huaengine-core-runtime/references/lifecycle-and-events.md`
- 来源类型: Runtime 生命周期说明
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - `Application::Run()` 已形成稳定主循环顺序。
  - Layer 更新和 GUI 渲染顺序固定，Scene 更新可作为脚本调度的稳定宿主。
  - 序列化初始化发生在应用构造期，为脚本状态持久化创造了统一启动点。
- 对规划的意义:
  - 脚本运行时应附着在 Scene/System 生命周期，而不是散落在 Layer 或 Editor 逻辑里。
