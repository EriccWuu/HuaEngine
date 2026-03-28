---
name: "engine-project-launcher-separation"
status: approved
version: 1.0.0
created_date: "2026-03-28"
plan_version: "1.0"
spec_version: "1.0"
parallel_config:
  max_concurrent: 3
  prefer_critical_path: true
  auto_schedule: true
review_mode: "local"
---

# 任务列表: engine-project-launcher-separation

## 1. 任务总览

| 指标 | 值 |
|------|----|
| 总任务数 | 6 |
| 关键路径长度 | 5 |
| 可并行任务组数 | 1 |
| 预估总时长 | 18h |

### 需求映射

| ID | 内容 |
|----|------|
| FR-1 | 提供独立 `ProjectHub.exe` 作为无项目入口 |
| FR-2 | `Editor.exe` 收成纯项目工作台宿主 |
| FR-3 | 彻底移除 Editor 中完整 launcher 表面的长期职责 |
| FR-4 | Launcher 通过稳定的 `--project [--scene]` 契约拉起 Editor |
| FR-5 | Recent/Resume 入口在 Launcher 分离后仍然可用 |

## 2. 任务清单

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T1.1 | 提取共享宿主启动桥与最小 shared entry-state 能力，支持宿主间进程拉起 | P0 | 3h | - | [R] | Shared Host Bridge | [x] |
| T1.2 | 新增 `ProjectHub` 可执行目标与 Launcher Layer，接入 create/open/resume 项目主链 | P0 | 4h | T1.1 | [R] | ProjectHub Host | [x] |
| T1.3 | 将 `Editor` 的无项目启动面收缩为最小 fallback/redirect，而非完整 launcher UI | P0 | 3h | T1.1 | [R] | Editor Runtime | [x] |
| T1.4 | 调整构建与脚本入口，使 `ProjectHub` 成为一等可构建/可启动目标 | P1 | 2h | T1.2 | [P] | Build Surface | [x] |
| T1.5 | 更新工作台/启动文档与模块 Skill，收口新的 Launcher -> Editor 产品模型 | P1 | 2h | T1.2, T1.3 | [P] | Documentation | [x] |
| T1.6 | 完成构建与进程级回归验证，覆盖 `ProjectHub` 启动、`Editor --project` 启动与 fallback 启动 | P0 | 4h | T1.2, T1.3, T1.4 | [R] | Validation | [x] |

## 3. 关键路径

```text
T1.1 -> T1.2 -> T1.3 -> T1.4 -> T1.6
```

## 4. 并行任务组

| 组 ID | 任务 | 前置依赖 | 说明 |
|-------|------|----------|------|
| G1 | T1.4, T1.5 | T1.2 / T1.2,T1.3 | 构建入口和文档更新可在核心宿主改动完成后并行收口 |

## 5. 实施结论

- 本轮实现聚焦“独立 launcher 宿主 + 纯工作台 Editor”的最小闭环
- 不扩展模板中心、版本管理或重型资源浏览器
- 继续保持 `ApplicationOperations` 作为正式能力边界

*Prepared by workflow-task | 2026-03-28*
