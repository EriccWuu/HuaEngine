# Serialization And Integration

## 1. SceneSerializer 的核心结构

关键文件：

- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.h`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`

当前 `SceneSerializer` 不是纯反射自动化方案，而是“两层桥接”：

1. `Serializer<T>` 负责具体组件的数据读写
2. `ComponentSerializers` 负责场景级“这个组件要不要进场景文件”的注册表

## 2. ComponentSerializers 注册表

`SceneSerializer.cpp` 内定义了一个静态注册结构 `ComponentSerializers`，它维护：

- `serializeFuncs`
- `deserializeFuncs`
- `typeIdToName`
- `nameToTypeId`

当前默认注册的组件只有：

- `TransformComponent`
- `Rendering::CameraComponent`
- `Rendering::MaterialComponent`
- `Rendering::MeshComponent`

这意味着：

- 新组件即使有 `Serializer<T>`，如果没注册到这里，也不会出现在场景文件里
- 新组件如果只在这里注册，但没有 `Serializer<T>` 或反射/序列化支持，也无法完成读写

## 3. 场景文件的实体写法

序列化时每个实体会被写成一个对象，核心字段包括：

- `entity_id`
- `components` 数组

每个组件元素里至少有：

- `compId`
- 以组件名命名的组件对象，例如 `TransformComponent`、`MeshComponent`

设计结果：

- 场景文件依赖组件名字符串和 `compId` 的共同配合
- 组件名来自 `ComponentSerializers` 的显式映射，不是自动推导

## 4. 反序列化行为的真实语义

`DeserializeEntity(...)` 的关键事实：

- 先读取文件里的 `entity_id`
- 但随后调用 `scene.GetEntityManager().CreateEntity()` 创建一个全新实体
- 当前并没有把原始 `entity_id` 回填给新实体

所以：

- 文件里的 `entity_id` 更像记录值，而不是当前实现中的稳定恢复 ID
- 任何依赖“跨加载保持同一个 entt 实体句柄”的逻辑都不成立

## 5. 组件扩展时的同步点

新增一个需要进入 Scene 的组件时，至少要同步检查：

1. 组件结构定义，例如 `ECS/Components.h` 或模块自己的组件头
2. 是否有反射声明，例如 `srefl_class(...)`
3. 是否有 `Serialization::Serializer<T>` 支持
4. 是否注册进 `ComponentSerializers::Instance()`
5. 编辑器侧是否需要显示或编辑它

漏掉任意一步，典型后果分别是：

- 运行时能用但不能保存
- 能保存但不能恢复
- 能恢复但编辑器不显示
- 编辑器能改但读档后丢失

## 6. 与 Rendering 的连接点

当前 Scene 最主要的跨模块连接点之一是渲染：

- `RenderSystem` 作为 `System` 被 `Scene::AddSyetem(...)` 注册
- 它通过 `Scene::View<TransformComponent, MeshComponent, MaterialComponent>()` 拿实体
- `CameraComponent`、`MaterialComponent`、`MeshComponent` 也都在场景组件注册表里

因此：

- ECS/Scene 改动经常会直接影响渲染可见性
- 组件名、序列化、默认 Transform 和系统注册，都会间接影响渲染结果

## 7. 具体验证入口

当前仓库里已有一个很直接的验证样例：

- `HuaEngine/src/HuaEngine/Test/SceneSerializationTest.h`

这个测试展示了：

- 创建 Scene 和实体
- 写入多个 `TransformComponent`
- 调用 `Serialization::SaveScene(...)`
- 再读取回新 Scene 并遍历 registry 验证结果

如果要验证新增组件是否进入场景存档，这个测试是最直接的起点之一。

## Related Skills

- 如果场景文件里的组件最终影响渲染可见性、材质或 mesh：转到 `huaengine-rendering/references/assets-and-materials.md`
- 如果你要改的是 `Serializer<T>`、反射宏、GLM 特化或 backend：转到 `huaengine-serialization-reflection/references/core-flow.md`