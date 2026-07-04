# HuaEngine Reflection Runtime Facade P2 设计

## 背景

P1 已经把组件反射标记迁移到 `HE_REFLECT_COMPONENT` / `HE_REFLECT_FIELD`，并生成 `GeneratedReflection.*`。但当前组件序列化仍然间接依赖旧的 `Serialization::Serializer<T>`，默认实现会调用 `HE::Refl::reflect<T>()`，因此组件头仍需要引入 generated `srefl_class` 兼容头。

这不符合目标：组件作者只应该写 marker，不应该手动 include generated reflection 文件。

## 目标

P2 按两个连续阶段执行：

1. **P2-A/B：统一反射入口并让组件序列化脱离 `srefl_class`**
   - `HE::Refl` 成为上层统一 runtime facade。
   - 生成器输出 runtime type descriptor 和 typed component serializer。
   - `ComponentRegistry` 注册组件时使用 generated serializer，不再回落到 `Serializer<T>`。
   - 组件头删除 generated include，只保留 marker。

2. **P2-C：A/B 完成后立即整理序列化语义**
   - 明确字段缺失、未知字段、未知组件策略。
   - 审计并修复 `Ref<T>` serializer、Scene 序列化稳定性、JSON backend 风险。
   - 审计剩余 `srefl_class` 使用点，决定迁移或保留为 legacy。

## 非目标

- P2-A/B 不删除旧 `srefl_class` 静态反射库。
- P2-A/B 不一次性迁移 `MeshData`、Material 等非核心组件/资源类型。
- P2-A/B 不重写 JSON parser；该工作归入 P2-C。

## 架构设计

### 统一入口

新增 runtime 反射 API，仍放在 `HE::Refl` 命名空间：

```cpp
namespace HE::Refl {
    struct FieldDescriptor;
    struct TypeDescriptor;

    std::span<const TypeDescriptor> GetRuntimeTypes();
    const TypeDescriptor* FindRuntimeType(std::string_view qualifiedName);
    const TypeDescriptor* FindRuntimeType(ComponentTypeId typeId);
}
```

旧 `HE::Refl::reflect<T>()` 保留，作为 legacy compile-time path。上层组件系统、Scene serialization、CLI/Editor 后续均应优先使用 runtime descriptor。

### Runtime Descriptor

`TypeDescriptor` 描述一个反射类型：

- `Name`
- `QualifiedName`
- `Kind`
- `DisplayName`
- `Category`
- `ComponentTypeId`
- `Fields`
- `ConstructDefault`
- `Destroy`
- `Copy`
- `Serialize`
- `Deserialize`
- `AddCopyToWorld`

`FieldDescriptor` 描述字段元数据：

- `Name`
- `Type`
- `DisplayName`
- `Category`

字段读写不走通用 runtime offset 写入。生成器为每个组件生成类型化 serializer，避免字符串驱动的脆弱访问。

### 生成代码

`reflection_tool.py generate` 输出：

- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`

生成文件包含：

- runtime descriptor 数组
- `HE::Refl::GetRuntimeTypes`
- `HE::Refl::FindRuntimeType`
- typed serialize/deserialize 函数
- `HE::Generated::RegisterGeneratedComponents`

组件字段序列化示意：

```cpp
static void SerializeTransformComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::TransformComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "Position", component.Position);
    Serialization::SerializeValue(backend, "Rotation", component.Rotation);
    Serialization::SerializeValue(backend, "Scale", component.Scale);
    backend.EndObject();
}
```

### ComponentRegistry 集成

保留现有 `ComponentRegistry::Register<T>()`，继续用于未迁移类型。

新增 descriptor 注册入口：

```cpp
bool Register(const Refl::TypeDescriptor& descriptor);
```

`RegisterGeneratedComponents()` 调用该入口。Scene serialization 仍通过 `ComponentMetadata.Serialize/Deserialize`，但这些函数来自 generated runtime descriptor，不再来自默认 `Serializer<T>`。

### 删除组件头 generated include

P2-A/B 完成后：

- `Components.h` 不再 include `HuaEngine/Generated/Reflection/HuaEngine_ECS_Components.generated.h`
- `RenderingComponent.h` 不再 include `HuaEngine/Generated/Reflection/Module_Rendering_RenderingComponent.generated.h`
- 生成器停止生成 per-source `.generated.h`
- `ReflectionToolSmoke` 验证组件头不含 generated include

## P2-C 连续任务

A/B 完成并通过 smoke 后，立即进入 P2-C，不延期：

- 字段缺失策略：决定是保持默认值并 warning，还是 fail 整个组件。
- 未知字段策略：决定是否忽略、记录 diagnostics、或保留到 future metadata。
- 未知组件策略：Scene load 不能静默吞掉，应至少记录 diagnostics。
- `Ref<T>` serializer：审计 Begin/EndObject 后再二次 Deserialize 的可疑流程。
- Scene 输出稳定性：实体顺序、组件顺序、字段顺序应稳定，方便 diff。
- JSON backend：审计 parser、number 类型、null、错误报告。
- 剩余 `srefl_class`：列出所有使用点，迁移核心路径，保留必要 legacy。

## 验收标准

P2-A/B：

- 5 个核心组件头不包含 generated reflection include。
- `ReflectionSmoke` 改为验证 `HE::Refl` runtime descriptor。
- `ReflectionGeneratedSmoke` 验证 runtime descriptor 和 generated registration。
- `SerializationSmoke`、`ECSSceneSerializationSmoke` 通过。
- `CLIReflectionSmoke` 通过。
- `reflection_tool.py validate --root .` diagnostics 为空。

P2-C：

- 序列化异常语义有明确测试覆盖。
- 未知组件/字段不再无声失败。
- `Ref<T>` serializer 有回归测试。
- Scene serialization 输出稳定性有 smoke 覆盖。
