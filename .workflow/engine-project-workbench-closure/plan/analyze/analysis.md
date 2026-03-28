# ANALYZE: engine-project-workbench-closure

## 输入概览

- 输入规范: `../specify/spec.md`
- 当前目标: 让 Editor 具备最小可用的项目工作台闭环，而不是继续依赖固定 workbench 演示壳
- 用户优先级: 先让引擎能用起来，再继续补完整工具链
- 审查方式: 独立 Agent

## 当前仓库事实

### 已有能力

- `ProjectService` 已支持初始化工程、解析工程上下文、检查工程状态
- `SceneService` 已支持创建、加载、保存、校验场景
- `AssetService` 已支持最小资产登记、解析与校验
- `ApplicationOperations` 已把这些能力收口为统一 GUI/headless 可消费操作面
- `Editor` 已具备 `Scene Hierarchy / Inspector / Console / Scene View` 这 4 个基础工作台区域

### 当前缺口

- Editor 启动仍写死到 `%LOCALAPPDATA%/HuaEngine/Workbench`
- Editor 没有正式“创建工程/打开工程”入口
- 当前场景只有 runtime `Ref<Scene>`，缺少明确的 `SceneDocument` 语义
- 当前工作台状态主要是结果回显，不是项目会话模型
- 缺少最小项目/资产浏览视图，用户不知道当前工程内容边界

## 关键设计问题

1. Editor 的启动路径如何从“固定 workbench”转为“显式项目会话”
2. 当前工作台需要什么最小状态对象，才能稳定承载项目与场景文档
3. 最小项目浏览器和资产视图应该做到什么边界，既能闭环又不把范围炸开
4. 当前保存/加载/验证如何组织成用户可理解的 GUI 工作流

## 规划判断

### 判断 1: 项目会话必须先于更多面板能力

没有 `CurrentProject` / `CurrentSceneDocument` 边界，后续再加资产视图、菜单项或最近工程都只会叠在临时 workbench 上，最终继续拖累 Editor。

### 判断 2: 最小闭环不需要先做原生文件对话框

当前阶段更重要的是正式闭环，而不是体验打磨。初版可以接受路径输入、工程面板和固定约定位置，只要它针对的是“任意正式工程”，而不是“唯一内部 workbench”。

### 判断 3: 资产工作区先做“项目树 + 场景入口 + 基础元信息”即可

当前仓库已有 `AssetService`，但没有成熟导入流水线。初版资产面板应该先承担“项目内容上下文可见性”，而不是做完整内容浏览器。

### 判断 4: 工作台状态需要从“事件回放”扩展到“项目会话 + 文档会话”

现有 `EditorWorkbenchState` 只够承接 `ResultEnvelope` 与 `ValidationReport`。要做正式工作台，需要它能表达当前工程、当前场景路径、当前场景是否脏、最近一次保存目标等会话状态。

## 研究主题

1. `project-workbench-topology`
2. `scene-document-lifecycle`
3. `asset-workspace-minimum`

## 预期计划产物

- 一份新的 Editor 项目工作台架构计划
- 明确的阶段路线: 项目入口 -> 场景文档 -> 项目/资产视图 -> 闭环验证
- 明确哪些能力现在做，哪些能力延后

---

*Generated during ANALYZE | 2026-03-28*
