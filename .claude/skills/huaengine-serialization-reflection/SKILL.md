---
name: huaengine-serialization-reflection
description: >
  HuaEngine 序列化与反射导航。覆盖反射宏、Serializer<T>、
  SerializationBackend/SerializationManager、JSON backend，以及 Project、Scene、
  Material、Mesh 等上层接入点。适用于回答序列化架构、反射驱动字段遍历、schema 形态、
  自定义 Serializer<T> 设计和序列化扩展落点这类问题。
---

# HuaEngine 序列化与反射

## 概览

这个 Skill 是当前反射和序列化行为的主导航入口。
适合回答：

- 一个类型该走默认反射序列化还是手工序列化
- 为什么某个字段能 / 不能持久化
- JSON backend 在项目 / 场景 / 资源持久化里扮演什么角色
- 当前正式 schema 是怎么设计的

## 模块边界

- `HuaEngine/src/HuaEngine/Reflection/`
  - 编译期反射元信息和字段访问
- `HuaEngine/src/HuaEngine/Serialization/`
  - `SerializationBackend`
  - `SerializationManager`
  - 通用 `Serializer<T>`
  - `JsonSerializationBackend`
  - GLM 和 `Ref<T>` 特化
- 上层消费者
  - `ProjectService`
  - `SceneSerializer`
  - `MaterialSerializer`
  - mesh 持久化相关代码

## 核心事实

- `srefl_class(...)` 只提供反射元信息，不代表正式持久化自动成立
- 默认 `Serializer<T>` 依赖反射字段遍历
- JSON 仍然是唯一正式注册的运行时后端
- 当前 JSON backend 是引擎受控序列化器，不是通用外部 JSON 库
- 上层系统接入深度并不相同，`Project`、`Scene`、`Material`、`Mesh` 不能按同一种方式持久化
- 静态字段不应再引入冗余的逐字段 `type`
- 动态载荷在必要时仍可保留显式类型标记
- 当前正式 schema 才是目标，旧字段名和旧包装结构已经不是兼容目标

## 关键文件

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

## 导航

- 看后端注册、通用序列化流程、初始化链：读 `references/core-flow.md`
- 看该走默认反射还是手工集成：读 `references/extension-and-integration.md`
- 看场景组件注册和场景文件结构：转 `huaengine-ecs-scene`
- 看材质 / mesh / 渲染资源持久化：转 `huaengine-rendering`

## 跨 Skill 导航

- 如果问题本质上是场景组件注册、实体恢复或场景文件结构：转 `huaengine-ecs-scene`
- 如果问题本质上是材质参数、mesh 数据、纹理或渲染资源恢复：转 `huaengine-rendering`
- 如果问题本质上是后端注册时机或宿主 / 控制层所有权：转 `huaengine-architecture`

## 常见误区

- 写了 `srefl_class(...)` 不等于类型就能安全持久化
- 默认 `Serializer<T>` 更适合数据型对象，不适合资源句柄和运行时缓存
- 代码里虽然有多种序列化格式枚举，但 JSON 才是当前真实运行时后端
- 当前正式 schema 使用 `schema_version`、`material_type` 这类字段；为了“宽松”重新打开旧字段兼容就是架构漂移
