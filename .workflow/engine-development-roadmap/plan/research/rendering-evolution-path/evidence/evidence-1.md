# Evidence E-1

- 主题: rendering-evolution-path
- 来源: `.claude/skills/huaengine-rendering/references/runtime-flow.md`
- 来源类型: 渲染主路径说明
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - 当前真实主路径是 `RenderSystem -> Renderer -> RenderCommand -> RendererAPI(OpenGL)`。
  - `RenderPipeline` 目录存在，但并非当前热路径。
  - Camera、FrameBuffer、MaterialInstance 已形成一条可用但轻量的提交链。
- 对规划的意义:
  - 渲染规划应先沿现有主路径演进，避免过早引入重抽象。
