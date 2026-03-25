---
name: huaengine-ecs-scene
description: >
  HuaEngine ECS 与 Scene 模块导航，覆盖 Entity/EntityManager、Component、System、Scene、SceneSerializer、
  编辑器侧实体浏览，以及它们与 rendering 和 serialization 的连接方式。Use when the user asks about
  ECS architecture, scene structure, entity/component access, scene serialization, system update flow,
  editor entity inspection, or where to modify ECS and scene behavior in HuaEngine.
---

# HuaEngine ECS And Scene

## Overview

这个 Skill 用于定位 HuaEngine 当前 ECS 与 Scene 模块的真实实现，适合回答“实体和组件怎么包装 EnTT”“Scene 每帧做什么”“组件序列化怎么注册”“编辑器为什么读不到实体名字”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/ECS/`：Entity 包装、EntityManager、基础组件、System 基类、脚本实体接口
- `HuaEngine/src/HuaEngine/Scene/`：Scene 容器与 SceneSerializer
- `Editor/src/Panels/`：Scene Hierarchy 和 Inspector 直接消费 Scene / Entity 的编辑器入口
- `Module/Rendering/RenderSystem` 是 Scene 系统的一个实际使用方，但不属于这个模块本体

## 核心子系统概览

- `EntityManager`：持有 `entt::registry`，负责创建与销毁实体
- `Entity`：对 `entt::entity` 的轻量包装，暴露 Add/Get/Has/RemoveComponent
- `Components.h`：基础 `Component`、`TransformComponent`、`NativeScriptComponent`
- `System`：场景系统基类，当前只有 `Update()` 抽象
- `Scene`：持有场景名、EntityManager 和系统列表，并负责逐帧调用系统
- `SceneSerializer`：场景与组件序列化桥梁，负责组件类型注册表和实体数组读写

## Core Rules

- 这个模块本质上是 “EnTT registry + 薄包装 + 手工序列化注册表”；排查问题时不要把它当成完整 ECS 框架层。
- `EntityManager::CreateEntity(...)` 当前会默认给新实体加上 `TransformComponent`；很多编辑器和场景逻辑都隐含依赖这一点。
- `Scene::Update()` 只遍历 `m_Systems` 并调用 `Update()`；没有注册进场景的系统不会自动执行。
- `SceneSerializer` 只会处理在 `ComponentSerializers::Instance()` 里显式注册过的组件；新增组件后不补注册，场景保存/加载不会覆盖它。
- 实体名目前不是可靠的事实源：`CreateEntity(name)` 收了 `name` 参数，但当前实现没有把它写回 `Entity::m_Name`。
- `NativeScriptComponent` / `ScriptableEntity` 目前更多是预留接口，不是已经完整接入的运行时脚本系统。

## 关键入口文件

- `HuaEngine/src/HuaEngine/ECS/Entity.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- `HuaEngine/src/HuaEngine/ECS/Components.h`
- `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h`
- `HuaEngine/src/HuaEngine/ECS/Syetem.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.h`
- `HuaEngine/src/HuaEngine/Scene/Scene.cpp`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.h`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `Editor/src/Panels/SceneHierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`

## Navigation

- 想理解 Entity/Scene 包装、系统更新链路、编辑器读写方式：读 `references/runtime-structure.md`
- 想定位组件注册、场景存档格式、组件增删后的同步点：读 `references/serialization-and-integration.md`
- 想看系统如何被实际使用，例如渲染系统如何挂到 Scene：结合 `huaengine-rendering`
- 想先看全仓库分层：先读 `huaengine-architecture`

## Cross-Skill Navigation

- 如果 ECS/Scene 问题已经进入 `RenderSystem`、`CameraComponent`、`MeshComponent`、材质实例绑定或 FrameBuffer 行为，转到 `huaengine-rendering`；一帧主路径看 `references/runtime-flow.md`，资源和材质侧影响看 `references/assets-and-materials.md`。
- 如果新增组件后涉及 `srefl_class(...)`、`Serializer<T>`、Scene 读写、GLM 字段、JSON backend 或复杂资源字段，转到 `huaengine-serialization-reflection`；扩展步骤优先看 `references/extension-and-integration.md`。
- 如果你还在判断问题属于“仓库分层 / 启动入口 / Editor 面板 / Sandbox 验证”中的哪一类，先转到 `huaengine-architecture`；总览看 `references/architecture.md`。
- 场景相关问题常常同时跨到渲染和序列化：先确认实体与组件事实，再分别检查渲染消费方和存档消费方。

## Common Pitfalls

- `EntityManager::CreateEntity(name)` 当前忽略了传入的 `name`；编辑器看到的名字会一直是默认值 `"Entity"`，除非后续代码手动改写。
- `SceneSerializer` 反序列化时会新建实体，而不是恢复原始 `entity_id`；不要把文件里的实体 ID 当成稳定持久 ID。
- `ScriptableEntity.h` 里有明显未接顺的接口痕迹；涉及脚本系统时先确认真实调用点，不要默认它已可用。
- `SceneHierarchyPanel` 通过 `TransformComponent` 视图枚举实体，所以缺少 Transform 的实体默认不会出现在层级树里。
- 组件序列化既依赖 `Serializer<T>`，也依赖 `ComponentSerializers` 注册；只满足其中一层是不够的。
