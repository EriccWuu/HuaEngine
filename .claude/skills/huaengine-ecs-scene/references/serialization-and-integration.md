# 序列化与集成

## 1. SceneSerializer 仍然是手工登记表，不是全自动场景反射

关键文件：

- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`

当前设计是两层桥接：

1. `Serializer<T>`
   - 负责“这个组件类型怎么读写数据”
2. `ComponentSerializers`
   - 负责“这个组件是否进入场景文件”

这两层缺一不可。

## 2. 默认进入场景文件的组件只有四种

当前默认登记表里只有：

- `TransformComponent`
- `Rendering::CameraComponent`
- `Rendering::MaterialComponent`
- `Rendering::MeshComponent`

这意味着：

- 新组件即使有 `Serializer<T>`，没登记也不会进场景文件
- `NativeScriptComponent` 当前不会随着场景保存/加载
- 任何新的正式场景能力都要先确认是否需要进入这个登记表

## 3. 场景文件结构仍是实体数组 + 组件数组

每个实体大致写成：

```json
{
  "entity_id": 1,
  "components": [
    {
      "compId": 123,
      "TransformComponent": { ... }
    }
  ]
}
```

重要事实：

- `compId` 只是当前登记映射的一部分
- 组件名来自显式映射，不是自动推导
- 这个结构是当前 SceneSerializer 的真实协议

## 4. 反序列化不会恢复原始实体句柄

`DeserializeEntity(...)` 当前流程是：

- 读取文件中的 `entity_id`
- 调 `scene.GetEntityManager().CreateEntity()`
- 把组件数据填进新实体

不会做的事：

- 不会恢复原始 `entt::entity`
- 不会保证跨加载的稳定实体句柄

所以：

- `entity_id` 更像记录值
- 不能把它当成长期稳定引用主键

## 5. 正式 Scene 校验边界已经补上

当前正式场景验证不只看“能不能读出来”，还看“能不能进入当前运行时约束”。

`SceneService::ValidateScene(...)` 重点检查：

- 场景是否有名字
- 是否有实体缺失 `TransformComponent`
- 渲染实体是否缺失 `MeshComponent`
- 渲染实体是否缺失 `MaterialComponent`
- 是否仍在使用 legacy `RendererComponent`

这意味着：

- SceneSerializer 的成功不等于运行时可接受
- 现在要把“可读写”和“可运行”区分开看

## 6. 脚本与场景的当前集成边界

脚本生命周期现在已经有正式服务：

- `ScriptService`
- `ApplicationOperations::AttachScriptRuntime`
- `ApplicationOperations::InitializeSceneScripts`
- `ApplicationOperations::UpdateSceneScripts`
- `ApplicationOperations::ShutdownSceneScripts`

但场景持久化边界仍是旧状态：

- `NativeScriptComponent` 默认不随 SceneSerializer 进出文件

所以当前脚本能力更准确的表述是：

- 运行时闭环已存在
- 默认持久化闭环还没有并入正式场景协议

## 7. 与渲染的真实连接点

Scene 到渲染的正式组合条件是：

- `TransformComponent`
- `Rendering::MeshComponent`
- `Rendering::MaterialComponent`

`RendererComponent` 已经不是正式链路的一部分。

所以如果你在排查“为什么场景能加载但看不到东西”，优先按这个顺序查：

1. Scene 文件里是否真的恢复了 `MeshComponent` 和 `MaterialComponent`
2. `SceneService::ValidateScene(...)` 是否已报缺失组件
3. `RenderSystem` 是否真的看到了这组三组件

## 相关 Skill

- 组件字段、反射和 `Serializer<T>` 如何工作：
  - 看 `huaengine-serialization-reflection/references/core-flow.md`
- 渲染链如何消费场景里的 mesh/material/camera：
  - 看 `huaengine-rendering/references/runtime-flow.md`
