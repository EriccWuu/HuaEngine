# Runtime Structure

## 1. EntityManager 与 Entity 包装

关键文件：

- `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- `HuaEngine/src/HuaEngine/ECS/Entity.h`

当前设计是薄包装：

- `EntityManager` 持有唯一的 `entt::registry`
- `CreateEntity()` 调用 `registry.create()` 后，立即返回 `Entity` 包装对象
- 新实体默认会 `AddComponent<TransformComponent>()`
- `DestroyEntity()` 直接 `registry.destroy(entity)`

`Entity` 自身只保存：

- `entt::entity m_EntityHandle`
- `EntityManager* m_EntityManager`
- 一个当前并未被创建流程正确维护的 `m_Name`

`Entity` 上的模板操作本质都是直接转发到 `m_EntityManager->m_Registry`：

- `AddComponent<T>()`
- `GetComponent<T>()`
- `HasComponent<T>()`
- `RemoveComponent<T>()`

这意味着：

- 如果 `Entity` 的 manager 指针失效，包装对象本身没有额外保护层
- 这个模块更偏“便于调用的语法糖”，不是强约束的领域模型

## 2. 默认组件与实体可见性

当前仓库里，`TransformComponent` 有两个特殊地位：

- `CreateEntity()` 会自动添加它
- `SceneHierarchyPanel` 使用 `reg.view<TransformComponent>()` 枚举实体

结果是：

- 没有 Transform 的实体不会被编辑器层级树显示
- 很多场景逻辑默认把“有 Transform”当作实体存在的基本前提

## 3. Scene 与 System 更新链路

关键文件：

- `HuaEngine/src/HuaEngine/Scene/Scene.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.cpp`
- `HuaEngine/src/HuaEngine/ECS/Syetem.h`

当前 Scene 很轻：

- 持有 `m_Name`
- 持有一个 `EntityManager`
- 持有一个 `std::vector<Ref<System>> m_Systems`

`Scene::Update()` 的逻辑只有一件事：

- 遍历 `m_Systems`
- 调用每个 system 的 `Update()`

这意味着：

- Scene 本身没有生命周期调度器、脚本调度器、固定更新、分阶段系统排序
- 系统执行顺序就是注册顺序
- 新系统必须显式 `AddSyetem(...)`，否则不会自动运行

## 4. View/Get 如何使用 EnTT

`Scene` 提供了两个核心模板入口：

- `View<Type, Other...>()`
- `Get<Type...>()`

它们直接转发到 `EntityManager.GetRegistry()`：

- `View(...)` 返回 `registry.view<...>()`
- `Get(...)` 返回 `registry.get<...>()`

所以：

- 性能和语义基本就是 EnTT 原生行为
- 调试复杂查询时可以直接回到 registry 视角理解，而不必寻找更高层 DSL

## 5. 脚本接口当前状态

`Components.h` 与 `ScriptableEntity.h` 中可以看到：

- `NativeScriptComponent` 持有 `Instance`、`InstanceFunc`、`DestoryFunc`
- `ScriptableEntity` 暴露 `OnCreate()`、`OnUpdate()`、`OnDestory()` 等钩子

但当前仓库事实是：

- 我没有找到真正驱动这些脚本实例的 Scene 更新逻辑
- 也没有看到脚本组件在运行时系统中被统一遍历与触发

因此处理脚本相关任务时，应先判断你是在“补全未完成能力”，还是“修已有链路中的 bug”。当前更像前者。

## 6. 编辑器消费路径

最直接的编辑器入口：

- `Editor/src/Panels/SceneHierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`

`SceneHierarchyPanel`：

- 从 `Scene` 拿 `EntityManager`
- 用 `view<TransformComponent>()` 遍历实体
- 为每个实体构造临时 `Entity(entity, &entityManager)` 包装对象
- 用 `entity.GetUid()` 和 `entity.GetName()` 画树节点

`InspectorPanel`：

- 从全局选择里拿 `Entity`
- 直接访问 `selection.m_EntityManager->GetRegistry()` 与 `selection.m_EntityHandle`
- 通过 `ComponentEditorRegistry` 渲染组件编辑 UI

这说明当前编辑器面板对 `Entity` 的封装边界并不强，仍会直接下钻 registry。

## Related Skills

- 如果系统注册后的主要消费方是渲染路径：转到 `huaengine-rendering/references/runtime-flow.md`
- 如果组件字段、反射声明或 JSON 读写本身出了问题：转到 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 如果需要先回到仓库级入口分层：转到 `huaengine-architecture/references/architecture.md`