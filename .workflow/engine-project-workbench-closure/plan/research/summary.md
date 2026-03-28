# 研究汇总: engine-project-workbench-closure

## 研究结论

### 1. 项目工作台的第一性问题不是“多几个面板”，而是“显式项目会话”

只要 Editor 仍以固定本地 workbench 作为隐式上下文，创建工程、打开工程、场景恢复和项目切换就都不会真正成立。因此本次规划必须先建立显式项目会话模型。

### 2. 场景必须升级为文档，而不是继续只当一份 runtime scene

当前场景已经能加载、保存和校验，但 Editor 仍缺少 `path / dirty / source / save target` 这些文档语义。没有文档层，项目工作流无法闭环。

### 3. 初版资产工作区应优先做成项目树

当前阶段不适合直接做重型资源浏览器。围绕工程目录做最小项目树和场景入口，既能支撑闭环，也不会超出当前代码基础。

## 推荐决策

- 采用 `ProjectSession + SceneDocument + WorkbenchState` 三层状态模型
- 采用 Editor 内嵌 `Project Hub` 作为最小工程入口，而不是另外启动复杂欢迎器
- 采用项目树/资产树面板作为当前资产工作区的最小形态

## 研究主题索引

- [project-workbench-topology](project-workbench-topology/research.md)
- [scene-document-lifecycle](scene-document-lifecycle/research.md)
- [asset-workspace-minimum](asset-workspace-minimum/research.md)

---

*Generated during RESEARCH | 2026-03-28*
