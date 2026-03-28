# 调研: scene-document-lifecycle

## 主题

当前场景加载/保存逻辑如何演进成正式 `SceneDocument` 工作流。

## 当前代码观察

- `EditorLayer` 当前只持有 `Ref<Scene> m_Scene`
- `BootstrapDemoScene()` 会尝试读场景文件，但没有显式 `CurrentScenePath / Dirty / NewScene` 模型
- `SceneService` 和 `ApplicationOperations` 已支持 `CreateScene / LoadScene / SaveScene / ValidateScene`

## 结论

### 1. 需要引入显式 SceneDocument 概念

哪怕初版结构很轻，也需要能明确表达:

- 当前场景对象
- 当前场景路径
- 当前场景名
- 当前场景是否未保存
- 当前场景来源是新建还是加载

### 2. 保存路径必须围绕当前工程展开

当前阶段不应该允许 Editor 继续以散乱路径写场景。最小闭环里，`Save` 和 `Save As` 都应该默认围绕 `ProjectContext.GetSceneRootPath()` 工作。

### 3. 场景切换和工程切换必须统一处理 dirty state

只要引入工程关闭/切换与场景切换，就会遇到“未保存修改如何处理”。初版可以先做到:

- 脏文档显示在工作台
- 切换前弹确认
- 未实现自动恢复，不做复杂 undo/redo

## 对规划的约束

- Editor 的场景工作流不能再只有裸 `Ref<Scene>`
- 当前场景文档必须是工程会话的一部分
- 保存、验证、渲染绑定需要围绕文档上下文统一刷新

---

*Research note | 2026-03-28*
