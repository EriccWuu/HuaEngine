# 调研报告: rendering-evolution-path

## SCOPE

- 调研目标: 明确渲染强化阶段应沿哪条现有主路径演进，以及为什么必须以后置阶段出现。
- 核心问题:
  - 当前主路径是什么。
  - 进入渲染强化阶段前必须稳定哪些上游输入。
- 评估维度:
  - 与当前实现的一致性
  - 与基础能力阶段的解耦程度
  - 后续扩展空间

## GATHER

- Evidence: [E-1](evidence/evidence-1.md) [E-2](evidence/evidence-2.md)

## ANALYZE

| 观察项 | 现状 | 影响 |
|--------|------|------|
| 主渲染链 | 已存在真实热路径 | 可以直接作为中期演进基础 |
| RenderPipeline | 不是当前热路径 | 不适合作为当前阶段重构中心 |
| 资源依赖 | Mesh / Material / Texture 与序列化强相关 | 上游基础不稳会污染渲染问题定位 |

## COMPARE

| 方案 | 优点 | 问题 |
|------|------|------|
| 先稳定资产和场景输入，再增强渲染 | 便于定位问题，阶段清楚 | 视觉成果后置 |
| 先重构 RenderPipeline | 理论上抽象更完整 | 与当前实现脱节，返工风险高 |

```mermaid
graph LR
    A[资产/场景输入稳定] --> B[渲染主路径稳定]
    B --> C[渲染能力增强]
    C --> D[未来 RenderPipeline / 更强抽象]
```

## RECOMMEND

推荐把渲染强化定义为基础阶段之后的独立阶段，先围绕现有 `RenderSystem -> Renderer -> OpenGL` 主链补齐能力边界，再在该基础上考虑更强的渲染抽象。[E-1][E-2]

## Sources

- [E-1](evidence/evidence-1.md)
- [E-2](evidence/evidence-2.md)
