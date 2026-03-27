---
name: huaengine-ecs-scene
description: >
  HuaEngine ECS 与 Scene 模块导航，覆盖 Entity/EntityManager、Component、Scene、
  SceneSerializer、SceneService，以及 Editor/Headless 如何消费这些能力。
  Use when the user asks about ECS architecture, scene structure, entity/component access,
  scene persistence, script attachment on scenes, or where to modify ECS and scene behavior in HuaEngine.
---

# HuaEngine ECS And Scene

## Overview

这个 Skill 用于定位 HuaEngine 当前 ECS 和 Scene 相关实现的真实边界，适合回答“实体和组件怎么包 EnTT”“正式场景操作该走哪一层”“场景为什么能加载但不能进运行时”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/ECS/`
  - `Entity`、`EntityManager`、基础组件、脚本实体桥接
- `HuaEngine/src/HuaEngine/Scene/`
  - `Scene`、`SceneSerializer`、`SceneService`
- `HuaEngine/src/HuaEngine/Script/`
  - 脚本生命周期服务会直接消费 `Scene` 和 `NativeScriptComponent`
- `Editor/src/Panels/`
  - Scene Hierarchy 和 Inspector 是当前最直接的 GUI 消费方
- `Headless/src/HeadlessCommandRunner.cpp`
  - `scene.*` 和 `script.*` 命令是当前正式 headless 消费面

## 核心事实

- 原始运行时层仍然是 `EntityManager + entt::registry + Scene`
- 正式操作层已经上移到 `SceneService` 和 `ApplicationOperations`
- `EntityManager::CreateEntity(...)` 仍会默认添加 `TransformComponent`
- `EntityManager::CreateEntity(name)` 目前仍未把 `name` 写回 `Entity::m_Name`
- `Scene::Update()` 只遍历已注册进 `m_Systems` 的系统
- `SceneSerializer` 目前只登记了：
  - `TransformComponent`
  - `Rendering::CameraComponent`
  - `Rendering::MaterialComponent`
  - `Rendering::MeshComponent`
- 场景反序列化会重建新实体，不会恢复文件中的原始 `entity_id`
- `NativeScriptComponent` 已经有正式运行时服务，但它当前不在 `SceneSerializer` 的默认组件登记表内
- `SceneService::ValidateScene(...)` 已把 `RendererComponent` 视为 legacy/unsupported，不再把它当作正式渲染配对

## 关键入口文件

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
- `Editor/src/Panels/SceneHierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`

## Navigation

- 想看原始 ECS 包装、Scene 更新链和 Editor 面板如何读写实体：
  - 读 `references/runtime-structure.md`
- 想看场景文件结构、组件登记、脚本/渲染接入边界：
  - 读 `references/serialization-and-integration.md`
- 想看场景组件如何进入正式渲染消费：
  - 转到 `huaengine-rendering`
- 想看反射和 `Serializer<T>` 如何影响场景读写：
  - 转到 `huaengine-serialization-reflection`

## Cross-Skill Navigation

- 如果问题已经进入 `RenderSystem`、`CameraComponent`、`MeshComponent`、`MaterialComponent` 或 viewport 渲染：
  - 转到 `huaengine-rendering`
- 如果问题落在 `srefl_class(...)`、`Serializer<T>`、JSON backend 或复杂组件字段：
  - 转到 `huaengine-serialization-reflection`
- 如果问题是“这条调用应该从 Editor、Headless 还是 ApplicationOperations 进入”：
  - 先看 `huaengine-architecture`
- 如果问题是 GUI 面板如何消费场景、选择状态或统一结果语义：
  - 转到 `huaengine-editor-workbench`

## Common Pitfalls

- `Entity::GetName()` 现在不是可靠事实源，因为创建流程没有回填传入名称
- `SceneHierarchyPanel` 仍按 `TransformComponent` 视图枚举实体，没有 Transform 的实体默认不会出现在树里
- 场景能保存不代表脚本绑定也能持久化；`NativeScriptComponent` 当前没有进入默认场景组件登记表
- `entity_id` 只是文件内记录值，不是跨加载稳定 ID
- 新组件想进入正式场景能力，至少要同时检查：
  - 组件定义
  - `Serializer<T>` 或可用反射路径
  - `SceneSerializer` 的组件登记
  - Editor 是否需要绘制它
