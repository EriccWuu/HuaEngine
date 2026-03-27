---
name: huaengine-serialization-reflection
description: >
  HuaEngine 序列化与反射模块导航，覆盖 Reflection 宏、Serializer<T>、
  SerializationBackend、SerializationManager、自研 JSON backend，以及 Scene/Project/Material/Mesh
  等上层接入点。 Use when the user asks about serialization architecture, reflection macros,
  JSON format handling, custom Serializer<T> design, field traversal, or where to extend HuaEngine
  serialization and reflection behavior.
---

# HuaEngine Serialization Reflection

## Overview

这个 Skill 用于定位 HuaEngine 当前“反射元信息 + 通用序列化模板 + 自研 JSON backend + 上层专用序列化器”的真实结构，适合回答“一个类型为什么不能被保存”“该写 `srefl_class(...)` 还是 `Serializer<T>`”“为什么项目和场景都依赖同一套 JSON backend”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/Reflection/`
  - 编译期反射元信息与字段访问
- `HuaEngine/src/HuaEngine/Serialization/`
  - `SerializationBackend`
  - `SerializationManager`
  - 通用 `Serializer<T>`
  - `JsonSerializationBackend`
  - GLM 和 `Ref<T>` 特化
- 上层消费方
  - `ProjectService`
  - `SceneSerializer`
  - `MaterialSerializer`
  - `Mesh` 相关序列化

## 核心事实

- `srefl_class(...)` 只提供反射元信息，不等于自动进入正式持久化协议
- 默认 `Serializer<T>` 依赖 `Refl::reflect<T>().visit_fields(...)`
- 当前真正注册到运行时的 backend 只有 JSON
- `InitializeSerialization()` 仍然只注册 `SerializationFormat::JSON`
- `JsonSerializationBackend` 是自研轻量实现，不是第三方 JSON 库
- `ProjectDescriptor`、`Scene`、`Material`、`Mesh` 都会消费这套能力，但不是同一种接入深度
- 复杂类型经常需要手工 `Serializer<T>` 或更高层专用序列化器，不能只靠反射

## 关键入口文件

- `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- `HuaEngine/src/HuaEngine/Serialization/Serialization.h`
- `HuaEngine/src/HuaEngine/Serialization/Serialization.cpp`
- `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
- `HuaEngine/src/HuaEngine/Serialization/SerializationManager.h`
- `HuaEngine/src/HuaEngine/Serialization/JsonSerializationBackend.h`
- `HuaEngine/src/HuaEngine/Serialization/JsonSerializationBackend.cpp`
- `HuaEngine/src/HuaEngine/Serialization/GLMSerializer.h`
- `HuaEngine/src/HuaEngine/Project/ProjectContext.h`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Material/MaterialSerializer.h`
- `HuaEngine/src/HuaEngine/Rendering/Mesh/MeshCore.h`

## Navigation

- 想看 backend、默认模板、JSON 栈模型和初始化链：
  - 读 `references/core-flow.md`
- 想判断新类型该走反射默认模板还是手工 `Serializer<T>`：
  - 读 `references/extension-and-integration.md`
- 想看场景侧如何登记组件并形成场景文件：
  - 转到 `huaengine-ecs-scene`
- 想看材质、mesh、渲染资源如何叠加专用序列化：
  - 转到 `huaengine-rendering`

## Cross-Skill Navigation

- 问题如果落在场景组件登记、实体恢复、场景协议或 Editor 场景恢复：
  - 转到 `huaengine-ecs-scene`
- 问题如果落在材质参数、mesh 数据、纹理路径或渲染资源恢复：
  - 转到 `huaengine-rendering`
- 问题如果先要确认 backend 在哪里初始化、哪个宿主消费它、哪条启动链负责注册：
  - 转到 `huaengine-architecture`

## Common Pitfalls

- 只写 `srefl_class(...)` 不代表类型就一定能正确序列化
- 默认 `Serializer<T>` 更适合纯数据对象，不适合资源句柄、运行时缓存或依赖上下文的对象
- 代码里虽然有 `JSON / YAML / Binary` 枚举，但当前只真正注册了 JSON
- `JsonSerializationBackend` 的能力边界是“引擎内部受控结构”，不要按成熟通用 JSON 库去假设它的鲁棒性
- 上层对象经常还要额外过一层显式登记，例如 `SceneSerializer` 的组件注册表
