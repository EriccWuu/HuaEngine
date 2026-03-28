---
name: huaengine-ecs-scene
description: >
  HuaEngine ECS 与 Scene 导航。覆盖 Entity/EntityManager、场景组件所有权、
  Scene/SceneSerializer/SceneService，以及 Editor 和 Headless 如何消费场景能力。
  适用于回答实体与组件行为、场景持久化、场景修改边界以及 ECS/Scene 改动落点这类问题。
---

# HuaEngine ECS 与场景

## 概览

这个 Skill 是当前 ECS 与 Scene 行为的主导航入口。
适合回答：

- 实体和组件是怎么存储、枚举和消费的
- 哪些场景修改已经是正式共享操作
- 场景是怎么序列化和校验的
- Editor 与 Headless 应该怎样消费场景能力

## 模块边界

- `HuaEngine/src/HuaEngine/ECS/`
  - `Entity`、`EntityManager`、基础组件、脚本桥接类型
- `HuaEngine/src/HuaEngine/Scene/`
  - `Scene`、`SceneSerializer`、`SceneService`
- `HuaEngine/src/HuaEngine/Script/`
  - 消费 `Scene` 和 `NativeScriptComponent` 的脚本生命周期服务
- `Editor/src/Panels/`
  - `Hierarchy` 和 `Inspector` 是最直接的 GUI 消费面
- `Editor/src/Interaction/`
  - editor 侧场景命令，当前已把共享写操作委托到 `ApplicationOperations`
- `Headless/src/HeadlessCommandRunner.cpp`
  - `scene.*` 与 `script.*` 是正式 headless 场景消费入口

## 核心事实

- 原始运行时存储仍然是 `EntityManager + entt::registry + Scene`
- 共享写操作已经上移到 `SceneService` 和 `ApplicationOperations`
- `Editor` 的实体 / 组件命令不再拥有单独写路径，而是调用与 headless 相同的正式操作
- `Hierarchy` 和 `Inspector` 仍会为了每帧 GUI 渲染直接读取运行时场景
- 这种直读例外只属于 GUI 渲染路径，不代表共享查询应继续长在面板里
- `EntityManager::CreateEntity(...)` 仍默认挂 `TransformComponent`
- `SceneSerializer` 现在应只持久化 live entity 和真实组件状态；tombstone 和删除后的空壳实体都应视为 bug
- `RendererComponent` 已经是 legacy/unsupported，不应再作为正式渲染契约扩展

## 关键文件

- `HuaEngine/src/HuaEngine/ECS/Entity.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- `HuaEngine/src/HuaEngine/ECS/Components.h`
- `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.cpp`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `HuaEngine/src/HuaEngine/Scene/SceneService.h`
- `HuaEngine/src/HuaEngine/Scene/SceneService.cpp`
- `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- `Editor/src/Interaction/EditorSceneCommands.cpp`
- `Editor/src/Panels/HierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`
- `Headless/src/HeadlessCommandRunner.cpp`

## 导航

- 看运行时所有权、面板消费方式、正式层和原始层边界：读 `references/runtime-structure.md`
- 看场景文件结构、序列化期望、脚本和渲染集成边界：读 `references/serialization-and-integration.md`
- 看视口渲染与 render-facing 组件：转 `huaengine-rendering`
- 看反射和 serializer 机制：转 `huaengine-serialization-reflection`

## 跨 Skill 导航

- 如果问题落在视口渲染、`CameraComponent`、`MeshComponent`、`MaterialComponent` 或 framebuffer 可见结果：转 `huaengine-rendering`
- 如果问题落在 `Serializer<T>`、反射字段或 JSON backend 行为：转 `huaengine-serialization-reflection`
- 如果问题是宿主 / 控制层边界，或者某能力应不应该进 `ApplicationOperations`：转 `huaengine-architecture`
- 如果问题本质上是 Editor 工作台的选择、右键菜单、快捷键或围绕实体编辑的撤销重做：转 `huaengine-editor-workbench`

## 常见误区

- `Hierarchy` 仍通过 `TransformComponent` 枚举实体；没 transform 的实体不会显示出来
- GUI 摘要看起来正确，不代表运行时场景一定正确；要结合 `ApplicationOperations`、`SceneService` 和 smoke 一起判断
- 场景删除残留问题往往来自序列化枚举到了 tombstone / 无效实体，而不一定是 panel 本身的问题
- 新增一个组件不代表它就正式进了引擎能力链；还需要同步处理序列化、校验、Editor 消费，必要时还要补正式操作面
