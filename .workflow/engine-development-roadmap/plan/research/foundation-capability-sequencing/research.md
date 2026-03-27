# 调研报告: foundation-capability-sequencing

## SCOPE

- 调研目标: 明确基础阶段为什么必须优先建设资产、序列化和场景基础闭环。
- 核心问题:
  - 当前基础能力缺口集中在哪里。
  - 哪些现有模块可以复用，哪些边界需要补齐。
- 评估维度:
  - 是否符合已批准 spec
  - 是否能最小化返工
  - 是否能为后续渲染阶段提供稳定输入

## GATHER

- Evidence: [E-1](evidence/evidence-1.md) [E-2](evidence/evidence-2.md)

## ANALYZE

| 观察项 | 现状 | 影响 |
|--------|------|------|
| 基础阶段目标 | 由 spec 明确指定 | 必须先服务基础能力，而不是渲染增强 |
| 模块骨架 | 架构层已具备 Core / Scene / Rendering / Serialization 分层 | 可以沿现有结构演进 |
| 关键缺口 | 资产边界不统一 | Scene、Material、Mesh 之间的持久化和运行时引用不稳定 |

## COMPARE

| 方案 | 优点 | 问题 |
|------|------|------|
| 先补基础能力闭环，再增强渲染 | 与 spec 一致，返工少，能形成稳定输入 | 前期看起来“进展不炫” |
| 先增强渲染，再回补基础能力 | 短期视觉收益高 | 资产、序列化、脚本边界会反复返工 |

```mermaid
graph LR
    A[Spec 要求] --> B[基础能力优先]
    B --> C[资产/序列化/脚本闭环]
    C --> D[稳定 Scene 与 Rendering 输入]
    D --> E[渲染强化阶段]
```

## RECOMMEND

推荐把基础阶段定义为“统一资源标识、统一持久化入口、统一场景运行时生命周期”的建设窗口，而不是分散修补若干功能点。[E-1][E-2]

## Sources

- [E-1](evidence/evidence-1.md)
- [E-2](evidence/evidence-2.md)
