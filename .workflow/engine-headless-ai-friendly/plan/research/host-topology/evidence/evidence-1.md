# Evidence E-1

- 主题: host-topology
- 来源: `.workflow/engine-headless-ai-friendly/ai-friendly-headless-blueprint.md`
- 来源类型: 统一蓝图文档
- 证据等级: A
- 访问时间: 2026-03-25
- 关键发现:
  - 蓝图明确要求 GUI 只是可选客户端，CLI/headless 是正式宿主。
  - 蓝图推荐核心域 + 应用服务层 + 多宿主客户端结构。
- 对规划的意义:
  - 技术规划必须把宿主与核心能力彻底分层，而不是在 Editor 上叠加命令入口。
