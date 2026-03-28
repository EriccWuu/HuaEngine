---
name: "engine-project-workbench-closure"
status: approved
version: 1.0.0
created_date: "2026-03-28"
plan_version: "1.0"
spec_version: "1.0"
parallel_config:
  max_concurrent: 4
  prefer_critical_path: true
  auto_schedule: true
---

# 任务列表: engine-project-workbench-closure

## 1. 任务总览

| 指标 | 值 |
|------|----|
| 总任务数 | 15 |
| 关键路径长度 | 9 |
| 可并行任务组数 | 4 |
| 预估总时长 | 44h |

### 需求映射基线

| ID | 内容 |
|----|------|
| FR-1 | Editor 必须支持创建新工程并切换到该工程上下文 |
| FR-2 | Editor 必须支持打开已有工程并解析正式 `ProjectContext` |
| FR-3 | Editor 必须建立“当前工程 + 当前场景文档”的明确运行时模型 |
| FR-4 | Editor 必须支持工程内新建、打开、保存场景 |
| FR-5 | GUI 工作台必须展示当前工程的最小项目/资产视图 |
| FR-6 | 工程级和场景级诊断必须能在工作台中统一查看 |
| FR-7 | GUI 新能力必须通过 `ApplicationOperations` 消费正式能力 |
| FR-8 | 项目会话必须允许下次打开时恢复到正式工程 |

## 2. 任务清单

### 阶段 1: Session Foundation

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T1.1 | 新增 `ProjectSession` / `SceneDocument` / `EditorWorkbenchState` 扩展的基础模型 | P0 | 4h | - | [R] | Editor Session | [x] |
| T1.2 | 将 `EditorLayer` 启动流程重构为 `Project Hub -> Workbench Shell` 双状态入口 | P0 | 4h | T1.1 | [R] | Editor Runtime | [x] |
| T1.3 | 建立工程会话切换、关闭、重置与未就绪工作台回退路径 | P1 | 3h | T1.1, T1.2 | [P][R] | Editor Runtime | [x] |
| T1.4 | 统一项目/场景/验证/操作结果写入 `EditorWorkbenchState` 的事件归口 | P0 | 2h | T1.1 | [R] | Workbench Telemetry | [x] |

### 阶段 2: Project Lifecycle

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T2.1 | 在 GUI 中接入 `Create Project` / `Open Project` 入口并绑定 `ApplicationOperations` | P0 | 4h | T1.2 | [T][R] | Project Hub | [x] |
| T2.2 | 实现项目会话持久化与启动恢复，替代固定 `%LOCALAPPDATA%/HuaEngine/Workbench` 逻辑 | P0 | 3h | T2.1 | [R] | Project Hub | [x] |
| T2.3 | 为工程状态、当前工程信息和项目诊断建立正式工作台展示 | P1 | 3h | T2.1, T2.2, T1.4 | [P] | Workbench State | [x] |

### 阶段 3: Scene Document Workflow

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T3.1 | 围绕 `SceneDocument` 重绑当前场景上下文、渲染绑定与验证刷新 | P0 | 4h | T1.1, T1.2, T1.4 | [R] | Scene Document | [x] |
| T3.2 | 实现 `New Scene / Open Scene / Save / Save As` 的最小闭环 | P0 | 5h | T3.1, T2.1 | [T][R] | Scene Document | [x] |
| T3.3 | 加入脏文档标记和切换前确认策略 | P1 | 3h | T3.2 | [P][R] | Scene Document | [x] |

### 阶段 4: Project Tree Workspace

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T4.1 | 新增最小 `Project Panel`，展示工程根、Assets/Scenes 目录树和当前文档信息 | P1 | 4h | T2.1, T3.1 | [R] | Project Panel | [x] |
| T4.2 | 在项目树中接入场景文件打开、刷新和基础操作结果回显 | P1 | 3h | T4.1, T3.2, T1.4 | [P] | Project Panel | [x] |

### 阶段 5: Closure and Validation

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T5.1 | 让 `Scene Hierarchy / Inspector / Console` 消费新的 session/document 摘要 | P1 | 2h | T1.1, T3.1, T2.3, T1.4 | [P] | Editor Panels | [x] |
| T5.2 | 新增项目工作台 smoke / regression 场景，覆盖 create/open/edit/save/reopen 主链 | P0 | 4h | T2.2, T3.2, T4.2, T5.1 | [T][R] | Validation Surface | [x] |
| T5.3 | 更新项目工作台文档与模块 Skill，使新工作流可发现、可复用 | P2 | 3h | T5.2 | [P] | Documentation | [x] |

## 3. 关键路径

```text
T1.1 -> T1.4 -> T1.2 -> T2.1 -> T2.2 -> T3.1 -> T3.2 -> T4.1 -> T4.2 -> T5.2
```

## 4. 并行任务组

| 组 ID | 任务 | 前置依赖 | 说明 |
|-------|------|----------|------|
| G1 | T1.3, T1.4 | T1.1,T1.2 / T1.1 | 启动回退与事件归口可并行推进 |
| G2 | T2.3, T3.1 | T2.1,T2.2,T1.4 / T1.1,T1.2,T1.4 | 工程状态展示与场景文档重绑可并行推进 |
| G3 | T3.3, T4.1, T5.1 | T3.2 / T2.1,T3.1 / T1.1,T3.1,T2.3,T1.4 | 脏文档处理、项目树和面板适配可分组并行 |
| G4 | T4.2, T5.3 | T4.1,T3.2 / T5.2 | 项目树操作补强与文档收口 |

## 5. 实施结论

- 当前任务树围绕最小可用项目工作台闭环组织
- 关键路径优先保证“能创建、能打开、能编辑、能保存、能重新打开”
- 重型内容浏览器、模板中心、复杂导入链都延后
- 所有 GUI 接线继续通过 `ApplicationOperations`
- 当前 15 个任务已经全部完成

*Prepared by workflow-task | 2026-03-28*
