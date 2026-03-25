---
name: huaengine-serialization-reflection
description: >
  HuaEngine 序列化与反射模块导航，覆盖 Reflection 宏与字段访问、SerializationBackend 抽象、
  SerializationManager、JSON backend、GLM/Ref 泛型特化，以及 Scene/Material/Mesh 等上层序列化连接点。
  Use when the user asks about serialization architecture, reflection macros, JSON format handling,
  custom Serializer<T> design, field traversal, ToJson/FromJson usage, or where to extend HuaEngine
  serialization and reflection behavior.
---

# HuaEngine Serialization Reflection

## Overview

这个 Skill 用于定位 HuaEngine 当前“反射 + 序列化”链路的真实实现，适合回答“`srefl_class` 到底提供了什么”“泛型 `Serializer<T>` 怎么工作”“为什么有些类型要单独写特化”“JSON backend 如何维护上下文”这类问题。

## 模块边界

- `HuaEngine/src/HuaEngine/Reflection/`：编译期反射元信息与字段/函数 traits
- `HuaEngine/src/HuaEngine/Serialization/`：序列化抽象、管理器、JSON backend、GLM 特化、便捷 API
- `SceneSerializer`、`MaterialSerializer`、`Mesh` 序列化是这个模块的上层消费者，但不属于其核心实现目录
- `Application` 启动时会调用 `InitializeSerialization()`，把 JSON backend 接到整个引擎运行链路

## 核心子系统概览

- `Reflection`：`srefl_class`、`field(...)`、`reflect<T>()`、字段访问与遍历能力
- `SerializationBackend`：统一对象/数组/标量读写接口
- `SerializationManager`：注册 backend，并提供 `SerializeToString/ToFile` 等便捷方法
- `Serializer<T>` 泛型模板：基于反射做默认对象字段序列化
- `JsonSerializationBackend`：当前唯一真正注册的后端
- `GLMSerializer` 与 `Ref<T>` 特化：为通用模板补足数学类型和智能指针场景

## Core Rules

- 反射和序列化不是一回事：`srefl_class(...)` 只提供字段元信息；类型要真正进序列化链路，还要看 `Serializer<T>` 是否可用、是否存在专门特化，以及上层是否做了显式注册。
- 默认泛型 `Serializer<T>` 依赖 `Refl::reflect<T>().visit_fields(...)`，按字段名直接展开对象，不会自动写类型标签。
- 当前真正注册到运行时的格式只有 JSON；`YAML` 和 `Binary` 还停留在枚举和 TODO 层。
- 新类型扩展要先判断属于“纯反射即可”的简单结构，还是必须单独写 `Serializer<T>` 的复杂类型，例如资源句柄、variant、上下文敏感对象。
- `Scene`、`Material`、`Mesh` 这类上层对象并不是只靠默认反射模板完成的，它们都叠加了手工序列化逻辑。
- README 不是完全可信的事实源；改动前要优先以当前头文件和实现为准。

## 关键入口文件

- `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- `HuaEngine/src/HuaEngine/Reflection/FieldTraits.h`
- `HuaEngine/src/HuaEngine/Serialization/Serialization.h`
- `HuaEngine/src/HuaEngine/Serialization/Serialization.cpp`
- `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
- `HuaEngine/src/HuaEngine/Serialization/SerializationManager.h`
- `HuaEngine/src/HuaEngine/Serialization/SerializationManager.cpp`
- `HuaEngine/src/HuaEngine/Serialization/JsonSerializationBackend.h`
- `HuaEngine/src/HuaEngine/Serialization/JsonSerializationBackend.cpp`
- `HuaEngine/src/HuaEngine/Serialization/GLMSerializer.h`
- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `HuaEngine/src/HuaEngine/Rendering/Material/MaterialSerializer.h`
- `HuaEngine/src/HuaEngine/Rendering/Mesh/MeshCore.h`
- `HuaEngine/src/HuaEngine/Test/SerializationTest.h`
- `HuaEngine/src/HuaEngine/Test/TestReflection.cpp`

## Navigation

- 想理解反射宏、默认 `Serializer<T>`、backend 抽象和 JSON 上下文模型：读 `references/core-flow.md`
- 想扩展新类型、排查 Scene/Material/Mesh 序列化问题、确认哪些地方需要手工特化：读 `references/extension-and-integration.md`
- 想看场景层怎么把组件登记到场景文件：结合 `huaengine-ecs-scene`
- 想看材质和 mesh 怎么叠加专门序列化：结合 `huaengine-rendering`

## Cross-Skill Navigation

- 如果序列化问题实际发生在场景组件登记、实体重建、`ComponentSerializers`、Scene 文件结构或编辑器场景恢复，转到 `huaengine-ecs-scene`；优先看 `references/serialization-and-integration.md`。
- 如果问题发生在材质参数、mesh 数据、shader path、texture path、渲染资源恢复或运行时提交结果，转到 `huaengine-rendering`；资源相关优先看 `references/assets-and-materials.md`。
- 如果你先要确认 backend 是在哪个启动链路被注册、功能挂在哪个目标程序、改动落在哪个主工程，转到 `huaengine-architecture`；入口结构看 `references/architecture.md`，构建运行看 `references/build-and-run.md`。
- 排查这类问题时，建议先分清“反射元信息”“通用序列化模板”“具体后端”“上层业务序列化器”四层，再决定跳到哪个 Skill。

## Common Pitfalls

- 只写 `srefl_class(...)` 不代表类型就一定能正确序列化；复杂类型通常还要补 `Serializer<T>` 或上层注册表。
- `InitializeSerialization()` 当前只注册 JSON backend；如果你直接走 YAML/Binary，会发现接口有枚举但实现没接上。
- `JsonSerializationBackend` 是自研轻量实现，不是成熟第三方库；复杂 JSON 场景或鲁棒性要求高时要先按当前能力边界评估。
- `README.md` 里提到的一些文件和结构在当前仓库并不存在或已偏离实现，排查时不要只按文档搜索。
- 某些类型特化的 `Deserialize` 签名并不完全一致，修改通用模板前要先核对调用点和现有约定。
