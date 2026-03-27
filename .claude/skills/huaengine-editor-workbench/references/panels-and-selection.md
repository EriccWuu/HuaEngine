# Panels And Selection

## 1. Selection 模型

`Selection` 当前实现仍然非常直接：

- 一个静态 `Entity m_Selection`
- `SetSelection(...)`
- `GetSelection()`
- `HasSelection()`

这意味着：

- 面板间通信靠全局静态共享
- 没有复杂状态树，也没有订阅机制
- Entity 失效后的状态一致性需要调用方自己小心

## 2. SceneHierarchyPanel

当前流程：

- 从 `Scene` 拿 `EntityManager`
- 用 `registry.view<TransformComponent>()` 枚举实体
- 为每一个实体构造临时 `Entity(entity, &entityManager)`
- 点选时调用 `Selection::SetSelection(entity)`

影响：

- 实体是否出现在层级树，取决于它是否有 `TransformComponent`
- 显示名来自 `Entity::GetName()`
- 面板顶部还会显示最近一次正式操作和最近一次 validation 汇总，数据来自 `EditorWorkbenchState`

## 3. InspectorPanel

`InspectorPanel` 当前不维护复杂上下文，而是：

- 检查 `Selection::HasSelection()`
- 取当前选择实体
- 显示最近一次 validation 状态与摘要
- 显示实体名
- 调 `ComponentEditorRegistry::Instance().DrawComponents(...)`

它仍然会直接访问：

- `selection.m_EntityManager->GetRegistry()`
- `selection.m_EntityHandle`

所以改 `Entity` 封装时，要留意 editor 侧还存在这类 friend 级访问。

## 4. ComponentEditorRegistry 与反射编辑器

`ComponentEditorRegistry`：

- 用 `std::type_index` 做组件类型索引
- 注册 `displayName + drawFunc`
- 按 `registeredTypes` 顺序遍历绘制

`DrawComponentEditor(...)`：

- 通过 `Refl::reflect<T>()` 遍历字段
- 用字段偏移拿到成员地址
- 对支持的字段类型调用 `DrawFieldEditor(...)`

当前现状：

- 默认内建注册了 `TransformComponent`、`CameraComponent`、`RendererComponent`、`MeshComponent`
- 字段编辑器现在主要对 `glm::vec3` 有像样 UI，其它类型多半仍是文本占位

## 5. ConsolePanel

`ConcolePanel` 现在分两类数据源：

- `Diagnostics` tab 读取 `EditorWorkbenchState::GetEventHistory()`
- `Logs` tab 继续读取 `Log::GetLogSink()->GetBuffer()`

它还会：

- 根据 `DiagnosticSeverity` 或 spdlog level 做颜色映射
- 支持 `Clear Logs` 与 `AutoScroll`

这说明 editor console 不再只是 runtime log 的消费者视图，也承担正式结果/诊断语义的可视化出口。

## Related Skills

- 如果你要补字段反射、自动组件编辑能力或 `Serializer<T>` 相关规则：转到 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 如果你要确认选择实体背后的真实 Scene/Component 结构：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 如果 Inspector/Scene Panel 里显示出的结果和渲染不一致：转到 `huaengine-rendering/references/assets-and-materials.md`
- 如果 Console 或输入交互问题更像 runtime glue：转到 `huaengine-core-runtime/references/window-input-and-imgui.md`
