---
name: "engine-project-hub-shell-separation"
status: approved
version: 1.0.0
created_date: "2026-03-28"
plan_ref: "engine-project-workbench-closure/plan/plan.md"
review_mode: "local"
---

# 任务列表: engine-project-hub-shell-separation

## 1. 任务总览

| 指标 | 值 |
|------|----|
| 总任务数 | 3 |
| 关键路径长度 | 3 |
| 可并行任务组数 | 0 |
| 预估总时长 | 5h |

### 需求映射

| ID | 内容 |
|----|------|
| FR-1 | 未打开项目时，Editor 只显示独立 Project Hub，而不是完整工作台壳 |
| FR-2 | 打开项目后，Editor 才进入完整 Workbench Shell |
| FR-3 | 文档与工作流台账需要同步当前入口模型 |

## 2. 任务清单

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T1.1 | 将 `ProjectHub` 从全局 DockSpace 渲染路径中剥离，建立独立入口态外壳 | P0 | 2h | - | [R] | Editor Runtime | [x] |
| T1.2 | 保证只有 `WorkbenchShell` 模式才渲染主工作台布局与工作台面板 | P0 | 2h | T1.1 | [R] | Editor Runtime | [x] |
| T1.3 | 更新工作台文档并完成启动验证 | P1 | 1h | T1.2 | [R] | Documentation / Validation | [x] |

## 3. 关键路径

```text
T1.1 -> T1.2 -> T1.3
```

## 4. 实施结论

- `Project Hub` 现在是独立入口态，不再与 `Hua Engine` 主 DockSpace 同时出现
- `Workbench Shell` 只在项目真正打开后才出现
- 文档已经同步到新的入口模型

*Prepared by workflow-task | 2026-03-28*
