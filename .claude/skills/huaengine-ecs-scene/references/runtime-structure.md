# Runtime Structure

## 1. 原始 ECS 层和正式操作层是两回事

当前代码里有两层语义：

- 原始层
  - `EntityManager`
  - `Entity`
  - `Scene`
  - `System`
- 正式操作层
  - `SceneService`
  - `ScriptService`
  - `ApplicationOperations`

定位问题时先分清：

- 你是在修 `registry` 层事实
- 还是在修宿主可消费的正式操作面

如果是 CLI、Editor、Agent 会用到的能力，默认优先沿 `ApplicationOperations` 往下找，而不是直接从宿主触碰 `Scene` 内部。

## 2. EntityManager 和 Entity 仍然是薄包装

关键文件：

- `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- `HuaEngine/src/HuaEngine/ECS/Entity.h`

当前设计仍然很薄：

- `EntityManager` 持有唯一 `entt::registry`
- `CreateEntity()` 调 `registry.create()`
- 创建后默认 `AddComponent<TransformComponent>()`
- `DestroyEntity()` 直接销毁实体

`Entity` 本身主要只是语法糖：

- `AddComponent<T>()`
- `GetComponent<T>()`
- `HasComponent<T>()`
- `RemoveComponent<T>()`

这些调用本质都直接下钻到 `m_EntityManager->m_Registry`。

## 3. 实体命名当前仍不可靠

`EntityManager::CreateEntity(const std::string& name)` 目前仍然忽略传入的 `name`。

结果是：

- `Entity::m_Name` 仍然保持默认值 `"Entity"`
- Editor 面板里看到的名字不是正式持久化名称体系
- 如果你想做真正的实体命名功能，不能把当前实现误判为“已有但有小 bug”

## 4. Scene 更新只做系统遍历

关键文件：

- `HuaEngine/src/HuaEngine/Scene/Scene.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.cpp`

`Scene` 当前很轻：

- 一个名字
- 一个 `EntityManager`
- 一个 `std::vector<Ref<System>> m_Systems`

`Scene::Update()` 的核心行为就是：

- 遍历 `m_Systems`
- 逐个调 `Update()`

这意味着：

- 没有注册的系统不会自动执行
- 系统执行顺序就是注册顺序
- 固定更新、多阶段调度、复杂生命周期都不在这一层

## 5. 脚本运行时已经不是纯占位接口

原始组件层仍然在：

- `NativeScriptComponent`
- `ScriptableEntity`

但正式运行时闭环已经落到了：

- `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`

当前应该这样理解：

- 脚本组件定义仍在 ECS 层
- 脚本生命周期推进已经转到 `ScriptService`
- Headless `script status/initialize/update/shutdown` 和上层宿主都应该走正式服务

不要再把脚本系统当成“完全没接线的预留接口”。

## 6. Editor 如何消费 Scene 事实

最直接的消费入口：

- `Editor/src/Panels/HierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`

当前特点：

- `HierarchyPanel` 仍通过 `view<TransformComponent>()` 枚举实体
- `InspectorPanel` 仍会直接使用底层 registry 事实
- Editor 上层状态和提示语义已经转移到 `EditorWorkbenchState`

所以排查 GUI 异常时，经常要同时检查：

- Scene / Entity 事实
- Inspector 的绘制注册
- Workbench 最近一次 ResultEnvelope 或 ValidationReport

## 7. Headless 如何消费 Scene 能力

当前 headless 入口：

- `scene create`
- `scene validate`
- `script status`
- `script initialize`
- `script update`
- `script shutdown`

它们都不是直接碰 `Scene` 的内部 helper，而是沿：

- `HeadlessCommandRunner`
- `ApplicationOperations`
- `SceneService` / `ScriptService`

这层关系决定了：

- CLI 能力边界优先服从正式服务
- 如果你在 Scene 内加了一个 helper，但没通过正式操作层暴露，CLI 不会自动得到它

## Related Skills

- 场景里的组件最终如何进入渲染：
  - 看 `huaengine-rendering/references/runtime-flow.md`
- 组件字段和 JSON 读写本身出了问题：
  - 看 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 宿主和服务层的正式边界：
  - 看 `huaengine-architecture/references/architecture.md`
