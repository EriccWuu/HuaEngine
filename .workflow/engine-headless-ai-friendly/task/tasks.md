---
name: "engine-headless-ai-friendly"
status: approved
version: 1.0.0
created_date: "2026-03-25"
plan_version: "1.0"
spec_version: "headless-blueprint"
parallel_config:
  max_concurrent: 5
  prefer_critical_path: true
  auto_schedule: true
---

# 任务列表: engine-headless-ai-friendly

## 1. 任务总览

| 指标 | 值 |
|------|----|
| 总任务数 | 19 |
| 关键路径长度 | 9 |
| 可并行任务组数 | 5 |
| 预估总时长 | 59h |

### 需求映射基线

| ID | 内容 |
|----|------|
| FR-1 | 核心能力必须具备正式无 GUI 操作路径，GUI 不能再是前提 |
| FR-2 | `Project / Scene / Asset / Script / Validation` 五类能力必须进入最小正式支持范围 |
| FR-3 | `Operation Registry / Application Service Layer` 必须成为唯一公开操作入口 |
| FR-4 | 所有正式操作必须返回统一 `Result Envelope`，并可判定 `成功 / 失败 / 需人工介入` |
| FR-5 | 必须形成正式 `CLI / Headless Host`，可稳定驱动最小闭环 |
| FR-6 | Editor / GUI 必须回收为同一能力面的客户端 |
| FR-7 | Automation / Agent Host 必须消费同一操作面，不能绕过服务层 |
| NFR-1 | 调用必须非交互、可脚本化、可在终端或 CI 中执行 |
| NFR-2 | 结果必须机器可读、结构稳定、适合自动化继续决策 |
| NFR-3 | 任意宿主都不得直接绕过服务层操作核心域 |
| NFR-4 | 渲染等后续能力接入时不得破坏当前热路径和统一操作面约束 |

## 2. 任务清单

### 阶段 1: Control Surface Reset

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T1.1 | 升级构建基线到 `C++20` 并校准三目标编译配置 | P0 | 2h | - | [R] | Build Baseline | [x] |
| T1.2 | 从 `Application` 主链中抽离 Runtime 启动缝与服务注册缝 | P0 | 3h | T1.1 | [R] | Core Runtime | [x] |
| T1.3 | 定义统一 `Result Envelope` 和操作状态协议 | P0 | 3h | T1.2 | [R] | Result Model | [x] |
| T1.4 | 解耦 `EditorLayer` 工作台壳与 demo scene / 直连运行逻辑 | P1 | 3h | T1.2 | [P][R] | Editor Workbench | [x] |

### 阶段 2: Service Extraction

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T2.1 | 建立 `ProjectContext` 与最小 `ProjectService` 操作面 | P0 | 3h | T1.3 | [T][R] | Project Domain | [x] |
| T2.2 | 建立围绕 `Scene / SceneSerializer` 的 `SceneService` | P0 | 4h | T1.3 | [T][R] | Scene Domain | [x] |
| T2.3 | 建立 `AssetHandle / AssetRegistry / AssetService` 最小闭环 | P0 | 5h | T1.3 | [T][R] | Asset Domain | [x] |
| T2.4 | 建立 `ScriptRuntimeSystem` 与 `ScriptService` 生命周期闭环 | P0 | 5h | T1.3 | [T][R] | Script Domain | [x] |
| T2.5 | 建立跨五类能力的 `ValidationService` 与诊断归口 | P0 | 4h | T2.1, T2.2, T2.3, T2.4 | [T][R] | Validation Domain | [x] |

### 阶段 3: Headless Host Formation

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T3.1 | 收拢五类能力服务的注册归口与组合根 | P0 | 2h | T2.1, T2.2, T2.3, T2.4, T2.5 | [R] | Application Layer | [x] |
| T3.2 | 实现 `Operation Registry / Application Service Layer` 统一公开入口 | P0 | 3h | T2.1, T2.2, T2.3, T2.4, T2.5, T1.3 | [R] | Application Layer | [x] |
| T3.3 | 新增正式 `CLI / Headless Host` 与非交互命令分发 | P1 | 5h | T3.1, T3.2 | [T][R] | Headless Host | [x] |
| T3.4 | 构建覆盖五类能力的 headless smoke workflow | P1 | 3h | T3.3, T2.5 | [P][R] | Validation Surface | [x] |

### 阶段 4: GUI Rebinding

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T4.1 | 让 `EditorApp / EditorLayer` 消费统一操作层 | P1 | 5h | T3.1, T3.2, T1.4 | [R] | Editor Workbench | [x] |
| T4.2 | 让 `SceneHierarchy / Inspector / Console` 消费统一结果与诊断 | P1 | 3h | T4.1, T2.5, T1.3 | [P] | Editor Panels | [x] |
| T4.3 | 建立 GUI 与 headless 的最小一致性验证场景 | P2 | 3h | T3.4, T4.2 | [P][R] | Validation Surface | [x] |

### 阶段 5: Capability Expansion Guardrails

| ID | 任务 | 优先级 | 估时 | 依赖 | 标记 | 模块 | 状态 |
|----|------|--------|------|------|------|------|------|
| T5.1 | 为 `RenderSystem` 建立只经操作层接入的能力扩展缝 | P2 | 4h | T3.2, T4.1 | [R] | Rendering Domain | [x] |
| T5.2 | 建立不绕过服务层的 `Automation / Agent Host` 适配契约 | P2 | 3h | T3.2, T3.3 | [P][R] | Agent Host | [x] |
| T5.3 | 同步 roadmap gate、模块 Skill 与 headless 契约文档 | P3 | 2h | T3.4, T4.2, T5.1, T5.2 | [P] | Documentation | [x] |

## 3. 关键路径

```text
T1.1 -> T1.2 -> T1.3 -> T2.1 -> T2.5 -> T3.2 -> T4.1 -> T4.2 -> T5.3
```

## 4. 并行任务组

| 组 ID | 任务 | 前置依赖 | 说明 |
|-------|------|----------|------|
| G1 | T1.4 | T1.2 | Editor 壳与 demo 解耦可和结果协议整理并行 |
| G2 | T2.1, T2.2, T2.3, T2.4 | T1.3 | 四条主服务可并行抽取 |
| G3 | T3.1, T3.2 | T2.1, T2.2, T2.3, T2.4, T2.5 | 服务归口与公开操作层可并行收束 |
| G4 | T3.3, T4.1 | T3.1, T3.2, T1.4 | headless 宿主与 GUI 回绑可并行推进 |
| G5 | T3.4, T4.2, T5.1, T5.2 | T3.3, T4.1, T2.5 | smoke、面板语义、渲染守卫和 Agent 适配可并行 |

## 5. 实施结论

- 19 / 19 任务已完成
- 关键路径任务全部收口
- 最终控制面保持为 `Application::GetOperations()`
- GUI、Headless、Agent 已统一到同一套结果协议与操作边界

*Finalized by workflow-implement | 2026-03-27*
