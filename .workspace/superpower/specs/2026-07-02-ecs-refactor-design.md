# ECS 框架重构设计

## 背景

当前 HuaEngine 的 ECS 层主要是对 EnTT 的轻量包装。`EntityManager` 直接持有 `entt::registry`，`Entity` 直接保存 `entt::entity`，`Scene` 通过 `View/Get` 继续向上层透传 EnTT API。Editor、SceneSerializer、SceneService、ApplicationOperations、RenderSystem、ScriptService 和测试代码中都存在直接访问 `entt::registry`、`entt::entity` 或 `GetRegistry()` 的调用。

这导致 ECS 存储实现泄漏到引擎上层，未来替换 ECS backend 的成本很高。同时当前系统层只有无参 `System::Update()`，缺少统一的 Query、Scheduler、系统阶段、访问声明和资源上下文，难以支撑更完整的运行时调度。

## 目标

本次重构采用一步到位方案，重新定义 ECS 公共 API、Scene 集成、Editor 集成和 scene 序列化 schema。目标是让非 ECS backend 代码不再直接依赖 EnTT，并补齐 ECS 框架的核心组件。

必须达到：

- 公共 API 不暴露 `entt::entity`、`entt::registry`、`GetRegistry()`。
- Editor、Rendering、Script、Scene、ApplicationOperations、Serializer 通过 `World`、`EntityId`、`Query`、`ComponentRegistry`、`Scheduler` 访问 ECS。
- 引入稳定实体身份：运行时 `EntityId` + 持久化 `EntityUuid`。
- 引入注册表驱动的组件元数据中心。
- 引入单线程 Scheduler、系统阶段和访问声明，为未来并行调度留接口。
- 重写 scene schema，不要求兼容旧格式。

## 非目标

- 第一版不实现并行系统调度。
- 第一版不实现 Bevy 风格的函数参数注入系统。
- 第一版不保留旧 `.scene` schema 的加载兼容。
- 第一版不要求完全移除内部 EnTT backend，但 EnTT 只能存在于 ECS backend 内部。

## 总体架构

新架构以 `Scene -> World + Scheduler` 为核心：

```text
Scene
  owns World
  owns Scheduler

World
  owns entity/component storage backend
  exposes EntityId / Entity / Query / ComponentRegistry APIs

Scheduler
  owns ordered system stages
  runs System with SystemContext

SystemContext
  exposes World, delta time, frame info, service handles

ComponentRegistry
  owns component metadata, construction/destruction/copy/serialization/editor hooks

SceneSerializer
  reads/writes scene schema through World + ComponentRegistry
```

`Scene` 不再透传 `View/Get`，也不暴露底层 registry。它只提供生命周期入口和对 `World`、`Scheduler` 的受控访问，例如：

```cpp
World& GetWorld();
const World& GetWorld() const;
Scheduler& GetScheduler();
void OnRuntimeStart();
void OnUpdate(float deltaTime);
void OnRuntimeStop();
```

## 实体身份

公开运行时句柄使用 `EntityId`：

```cpp
struct EntityId {
    uint32_t Index = 0;
    uint32_t Generation = 0;
};
```

持久化身份使用 `EntityUuid`：

```cpp
struct EntityUuid {
    uint64_t High = 0;
    uint64_t Low = 0;
};
```

`EntityId` 用于快速运行时访问，`Generation` 用于识别删除后失效的旧句柄。`EntityUuid` 用于 scene 文件、Editor 选择、复制粘贴和跨系统引用。`World` 维护双向映射：

```text
EntityUuid -> EntityId
EntityId   -> EntityUuid
```

`Entity` 是轻量 facade，内部持有 `World* + EntityId`，方便脚本和编辑器使用。真实数据操作由 `World` 执行。

## World API

`World` 负责实体生命周期、组件增删改查、Query 创建和组件元数据访问。

示例 API：

```cpp
Entity CreateEntity(std::string_view name = "Entity");
Entity CreateEntityWithUuid(EntityUuid uuid, std::string_view name);
void DestroyEntity(EntityId id);

bool IsAlive(EntityId id) const;
EntityUuid GetUuid(EntityId id) const;
EntityId FindEntity(EntityUuid uuid) const;

template<typename T, typename... Args>
T& AddComponent(EntityId id, Args&&... args);

template<typename T>
T* TryGetComponent(EntityId id);

template<typename T>
bool HasComponent(EntityId id) const;

template<typename T>
void RemoveComponent(EntityId id);

template<typename... Terms>
Query<Terms...> Query();

void ForEachEntity(FunctionRef<void(Entity)> callback);
```

组件仍是普通 C++ struct。组件类型必须注册到 `ComponentRegistry`，注册信息包括：

- 稳定类型名，例如 `HE.TransformComponent`。
- 类型 ID。
- 默认构造、销毁、复制。
- 序列化和反序列化 hook。
- Editor Inspector 绘制 hook。
- 是否允许重复添加。
- 显示名和分类。

## Query

第一版 Query 使用显式模板接口，不做函数参数注入。

```cpp
auto query = context.World().Query<
    TransformComponent,
    Rendering::MeshComponent,
    Rendering::MaterialComponent>();

query.ForEach([](Entity entity,
                 TransformComponent& transform,
                 Rendering::MeshComponent& mesh,
                 Rendering::MaterialComponent& material) {
    // update or render
});
```

只读访问使用 `Read<T>`：

```cpp
auto query = world.Query<TransformComponent, Read<VelocityComponent>>();
```

Query 创建时记录实际访问的组件类型。Debug 构建下，Scheduler 可校验系统声明的读写访问是否覆盖实际 Query 访问。

## System 与 Scheduler

系统接口改为显式描述 + 上下文更新：

```cpp
class System {
public:
    virtual ~System() = default;
    virtual SystemDescriptor Describe() const = 0;
    virtual void Update(SystemContext& context) = 0;
};
```

`SystemDescriptor` 包含：

- `Name`
- `Stage`
- `Before`
- `After`
- `Reads`
- `Writes`
- `ResourceReads`
- `ResourceWrites`
- `Enabled`

第一版 Scheduler 单线程执行，默认阶段：

```text
PreUpdate -> Update -> PostUpdate -> Render
```

Scheduler 负责：

- 按 Stage 分组。
- 根据 Before/After 做排序。
- 检测循环依赖。
- Debug 下校验系统访问声明。
- 为未来并行调度保留读写元数据。

## Scene 序列化

新 scene schema 围绕稳定 UUID 和组件注册表设计，不兼容旧格式。

示例：

```json
{
  "schema_version": 2,
  "scene": {
    "name": "SampleScene"
  },
  "entities": [
    {
      "uuid": "01J00000000000000000000000",
      "name": "Player",
      "components": {
        "HE.TransformComponent": {
          "position": [0, 0, 0],
          "rotation": [0, 0, 0],
          "scale": [1, 1, 1]
        },
        "HE.Rendering.CameraComponent": {}
      }
    }
  ]
}
```

保存流程：

```text
World.ForEachEntity()
  -> ComponentRegistry.ListComponents(entity)
  -> ComponentRegistry.Serialize(component)
```

加载流程：

```text
CreateEntityWithUuid(uuid, name)
  -> ComponentRegistry.Deserialize(typeName, payload)
  -> World.AddComponentByType(entity, typeId, value)
```

## Editor 集成

Editor 纳入重构范围，不能继续直接使用 EnTT。

- `HierarchyPanel` 使用 `World.ForEachEntity()` 构建实体列表和选择。
- `InspectorPanel` 使用 `ComponentRegistry` 枚举实体组件并调用组件 editor hook。
- `ComponentEditorRegistry` 从 `entt::registry&, entt::entity` 改为 `World&, EntityId`。
- `Selection` 保存 `EntityUuid` 作为稳定选择来源，可缓存 `EntityId` 加速运行时访问。
- 增删组件、改名、修改 Transform 继续走命令系统，但命令内部调用 `World` API。
- Undo/redo 命令保存实体 UUID、组件类型名和组件 payload，而不是底层 registry handle。

## 现有模块迁移

需要迁移的主要模块：

- `HuaEngine/ECS`：重建 `World`、`EntityId`、`EntityUuid`、`Entity`、`Query`、`ComponentRegistry`、`System`、`Scheduler`。
- `HuaEngine/Scene`：`Scene` 持有 `World` 和 `Scheduler`，删除 `Scene::View/Get`。
- `Module/Rendering/RenderSystem`：通过 `SystemContext.World().Query<...>()` 获取渲染对象。
- `HuaEngine/Script`：脚本绑定和运行时更新通过 `World` 和 Query 操作 `NativeScriptComponent`。
- `HuaEngine/Application/ApplicationOperations`：实体和组件操作改走 `World` API。
- `HuaEngine/Scene/SceneSerializer`：通过 `ComponentRegistry` 和新 schema 保存加载。
- `Editor`：Hierarchy、Inspector、Selection、ComponentEditor、EditorSceneCommands 迁移到新 ECS API。
- `HuaEngine/Test`：替换直接 registry 测试，新增 ECS 公共 API 测试。

## 验收标准

- 非 ECS backend 代码中不再出现 `entt::`、`entt.hpp`、`GetRegistry()`。
- `HuaEngine` 和 `Editor` Debug 构建通过。
- 支持实体创建/销毁、组件增删改查、Query 遍历、System 调度、Scene 保存/加载、Editor Hierarchy/Inspector 基本操作。
- 新 scene schema 能保存并重新加载当前测试场景。
- 有最小 fake backend 或 storage 测试，证明公共 ECS API 不依赖 EnTT。

## 测试计划

ECS 测试：

- `EntityId` generation 能识别删除后的旧句柄。
- `EntityUuid` 和 `EntityId` 双向映射正确。
- `ComponentRegistry` 支持注册、重复注册检测、类型名查询。
- `World::Add/Get/Has/RemoveComponent` 行为正确。
- Query 能正确遍历读写组件组合。
- Scheduler 能按阶段和 Before/After 顺序执行系统。
- Scheduler 能检测循环依赖。

Scene 测试：

- 保存和加载实体 UUID、名称、Transform、Camera、Mesh、Material。
- 删除实体后序列化结果不包含已删除实体。
- 加载后 Query 结果与保存前一致。

Editor 测试：

- Hierarchy 能列出 World 实体。
- 选中实体后 Inspector 能枚举组件。
- 添加、删除组件命令支持 undo/redo。
- 修改名称和 Transform 命令支持 undo/redo。

## 风险与控制

一步到位重构改动面大，主要风险是长时间构建不可用、Editor 调用点漏迁移、序列化 schema 与组件注册表脱节。

控制方式：

- 实施时按内部步骤推进，但最终目标不保留旧 ECS 公共接口。
- 如必须临时兼容，只允许放在 `ECS/Legacy` 或 `ECS/Backends/Entt`，并列为提交前必须清理项。
- 每完成一个模块迁移就运行对应构建或测试。
- 使用搜索验收 `entt::`、`entt.hpp`、`GetRegistry()` 泄漏。
- 先保证单线程 Scheduler 正确，再扩展并行。

## 后续计划

设计确认后进入 implementation plan 阶段，将本设计拆分为可执行任务，包括 ECS 基础类型、World 存储、组件注册表、Query、Scheduler、Scene 迁移、Serializer 迁移、Editor 迁移和测试补齐。
