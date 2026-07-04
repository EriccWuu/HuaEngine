# Reflection Runtime Provider P3 设计

## 背景

P2 已经把组件反射和组件序列化主路径切到 `HE::Refl` runtime descriptor facade：

- 组件头不再 include per-source `.generated.h`。
- reflection tool 只生成统一的 `GeneratedReflection.h/.cpp`。
- `ComponentRegistry` 从 `RuntimeTypeDescriptor` 注册组件。
- Scene serialization 通过 registry metadata callback 处理组件。
- `srefl_class` 和 `Refl::reflect<T>()` 仍保留为非组件 fallback / legacy 能力。

新的方向是选择“方案 C”：不是只给字段补 offset，也不是只迁移非组件，而是建立统一 runtime metadata 模型。外部只调用 runtime API；引擎底层可以继续使用静态反射、生成器、手写 provider 等方式生产 metadata。

## 目标

1. 对外统一反射入口：业务、Editor、CLI、Serialization 上层只依赖 `HE::Refl` runtime API。
2. 增强 `RuntimeFieldDescriptor`，支持 offset、size、flags、访问器和字段级序列化/反序列化。
3. 把组件序列化从“每类型生成完整 Serialize/Deserialize 函数”推进到“通用 runtime object serializer 遍历 field descriptor”。
4. 引入 runtime provider/registry 模型，让 generated component provider、static reflection provider、manual provider 可以汇入同一套 runtime type table。
5. 保留静态反射接口作为底层实现能力，而不是外部推荐 API。
6. 先迁移当前组件和一批 legacy 非组件类型，保持 P2 已经通过的序列化策略不回退。

## 非目标

- 不在本阶段删除 `srefl_class`、`type_info<T>` 或 `Refl::reflect<T>()`。
- 不要求一次迁移所有非组件类型。
- 不实现完整 UE 风格 property system、GC、replication、Blueprint 等能力。
- 不把所有字段类型都做成可编辑 UI 类型系统；本阶段只提供基础 field access 和 serialization operation。
- 不改变已有 scene JSON schema，除非测试证明必须修正现有 bug。

## 总体架构

```text
外部调用方
  Editor / CLI / Serialization / 工具
        |
        v
HE::Refl runtime facade
  GetRuntimeTypes()
  FindRuntimeType(...)
  VisitRuntimeFields(...)
  SerializeRuntimeObject(...)
  DeserializeRuntimeObject(...)
        |
        v
Runtime registry / providers
  Generated component provider
  Static reflection provider
  Manual provider
        |
        v
RuntimeTypeDescriptor / RuntimeFieldDescriptor
  field offset / size / flags / accessor / field ops
```

底层来源允许多种：

```text
HE_REFLECT_COMPONENT / HE_REFLECT_FIELD
        -> reflection_tool.py
        -> generated runtime descriptors

srefl_class / Refl::reflect<T>()
        -> static reflection adapter
        -> runtime descriptors

手写特殊类型
        -> manual descriptor provider
```

外部不再区分这些来源。

## RuntimeFieldDescriptor 扩展

当前字段 descriptor 只有：

```cpp
std::string_view Name;
std::string_view Type;
std::string_view DisplayName;
std::string_view Category;
```

P3 扩展为：

```cpp
enum class RuntimeFieldFlags : uint32_t {
    None = 0,
    Serializable = 1 << 0,
    Editable = 1 << 1,
    ReadOnly = 1 << 2,
    ComponentField = 1 << 3,
};

struct RuntimeFieldDescriptor {
    std::string_view Name;
    std::string_view Type;
    std::string_view DisplayName;
    std::string_view Category;
    size_t Offset;
    size_t Size;
    RuntimeFieldFlags Flags;
    const void* (*GetConst)(const void* object);
    void* (*GetMutable)(void* object);
    void (*Serialize)(Serialization::SerializationBackend& backend, const std::string& name, const void* object);
    bool (*Deserialize)(Serialization::SerializationBackend& backend, const std::string& name, void* object);
};
```

字段访问器语义：

- `GetConst(object)` 返回字段 const 地址。
- `GetMutable(object)` 返回字段 mutable 地址；只读字段可返回 `nullptr`。
- `Serialize(...)` 负责读取字段并调用 `Serialization::SerializeValue`。
- `Deserialize(...)` 负责使用临时值读取，成功后才写回字段；字段存在但解析失败返回 `false`。

这样可以统一避免 P2/P2-C 已修过的半写问题。

## RuntimeTypeDescriptor 调整

当前 type descriptor 已有组件 callbacks。P3 保留它们，但语义调整为通用类型 + 可选 ECS 能力：

```cpp
enum class RuntimeTypeKind : uint8_t {
    Struct,
    Component,
    Asset,
    Unknown,
};

struct RuntimeTypeDescriptor {
    std::string_view Name;
    std::string_view QualifiedName;
    RuntimeTypeKind Kind;
    std::string_view DisplayName;
    std::string_view Category;
    ComponentTypeId TypeId;
    size_t Size;
    std::span<const RuntimeFieldDescriptor> Fields;

    void* (*ConstructDefault)();
    void (*Destroy)(void*);
    void* (*Copy)(const void*);

    void (*Serialize)(Serialization::SerializationBackend&, const std::string&, const void*);
    bool (*Deserialize)(Serialization::SerializationBackend&, const std::string&, void*);

    void (*AddCopyToWorld)(World&, EntityId, const void*);
};
```

组件：

- `Kind = Component`
- `TypeId != InvalidComponentTypeId`
- ECS callbacks 非空

普通 struct：

- `Kind = Struct`
- `TypeId = InvalidComponentTypeId`
- ECS callbacks 为空

`Serialize` / `Deserialize` 默认可以指向 generic runtime object serializer，也允许特殊类型覆盖。

## 通用 runtime 操作

新增通用操作：

```cpp
void SerializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object);

bool DeserializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object);
```

序列化逻辑：

```text
BeginObject(name)
for field in type.Fields:
  if Serializable:
    field.Serialize(backend, field.Name, object)
EndObject()
```

反序列化逻辑：

```text
BeginObject(name)
success = true
for field in type.Fields:
  if not Serializable:
    continue
  if backend.HasField(field.Name):
    success &= field.Deserialize(backend, field.Name, object)
  else:
    keep default/current value
EndObject()
return success
```

策略必须保持 P2-C 结果：

- 缺失字段保留默认值并返回成功。
- 未知字段忽略。
- 字段存在但解析失败返回 false。
- 嵌套字段失败不能半写目标对象。

## Provider / Registry 模型

新增 runtime registry 层：

```cpp
class RuntimeRegistry {
public:
    bool RegisterType(const RuntimeTypeDescriptor& type);
    std::span<const RuntimeTypeDescriptor> GetTypes() const;
    const RuntimeTypeDescriptor* FindByQualifiedName(std::string_view name) const;
    const RuntimeTypeDescriptor* FindByComponentTypeId(ComponentTypeId typeId) const;
};
```

对外 facade 仍是：

```cpp
namespace HE::Refl {
    std::span<const RuntimeTypeDescriptor> GetRuntimeTypes();
    const RuntimeTypeDescriptor* FindRuntimeType(std::string_view qualifiedName);
    const RuntimeTypeDescriptor* FindRuntimeType(ComponentTypeId typeId);
}
```

内部 provider：

```text
Generated provider
  注册 reflection_tool.py 生成的 descriptors

Static provider
  把 srefl_class/type_info<T> 转成 RuntimeTypeDescriptor

Manual provider
  给特殊类型注册自定义 descriptor 或 serializer override
```

本阶段可以先用静态数组 provider 实现，不需要复杂插件系统。

## 组件生成器调整

当前生成器为每个组件生成：

```text
ConstructDefault_X
Destroy_X
Copy_X
AddCopyToWorld_X
Serialize_X
Deserialize_X
RuntimeFieldDescriptor[]
RuntimeTypeDescriptor[]
```

P3 后改为：

```text
ConstructDefault_X
Destroy_X
Copy_X
AddCopyToWorld_X

Field getter/setter/serialize/deserialize callbacks
RuntimeFieldDescriptor[]
RuntimeTypeDescriptor[]
```

`RuntimeTypeDescriptor.Serialize/Deserialize` 默认指向 generic runtime object serializer wrapper：

```cpp
static void Serialize_HE__TransformComponent(..., const void* object) {
    Refl::SerializeRuntimeObject(RuntimeTypes[index], backend, name, object);
}
```

这仍然保留 type-level callback 以兼容 `ComponentRegistry`，但字段处理进入通用路径。

## 静态反射 adapter

静态反射保留为底层能力。后续 adapter 可以形如：

```cpp
template<typename T>
const RuntimeTypeDescriptor& MakeStaticRuntimeTypeDescriptor();
```

内部使用：

```cpp
auto info = Refl::reflect<T>();
info.visit_fields(...);
```

它生成或返回与 generated provider 同形状的 `RuntimeFieldDescriptor`。

迁移顺序建议：

1. 先让 generated component provider 支持新 field descriptor。
2. 再为测试 fixture 增加 static provider smoke。
3. 再迁移 `ProjectDescriptor`。
4. 再迁移 `MeshData` 系列。

## 序列化调用链

组件 scene 保存：

```text
SaveScene
  -> Serializer<Scene>::Serialize
  -> ComponentRegistry metadata
  -> metadata.Serialize
  -> Refl::SerializeRuntimeObject(type, backend, name, object)
  -> RuntimeFieldDescriptor.Serialize
  -> Serialization::SerializeValue
```

组件 scene 加载：

```text
LoadScene
  -> Serializer<Scene>::Deserialize
  -> metadata.ConstructDefault
  -> metadata.Deserialize
  -> Refl::DeserializeRuntimeObject(type, backend, name, object)
  -> RuntimeFieldDescriptor.Deserialize
  -> AddCopyToWorld if success
```

普通对象：

```text
Serialization::ToJson(obj)
  -> Serializer<T>
  -> if runtime descriptor exists:
       SerializeRuntimeObject
     else:
       legacy fallback or hand-written serializer
```

## 测试策略

必须保留并扩展现有 smoke：

- `ReflectionGeneratedSmoke`
  - 字段 descriptor 包含 offset、size、flags。
  - field accessor 能读 `TransformComponent.Position`。
  - mutable accessor 能写回字段。

- `ReflectionSmoke`
  - 外部 runtime API 能遍历字段。
  - 不使用 `Refl::reflect<TransformComponent>()`。

- `SerializationSmoke`
  - `ComponentRegistry` metadata 仍能 round-trip `TransformComponent`。

- `ECSSceneSerializationSmoke`
  - scene 保存文本仍包含组件字段。
  - scene 加载字段 round-trip。

- `SerializationPolicySmoke`
  - 缺失字段保留默认值。
  - 坏字段返回 false。
  - 嵌套字段失败不半写。
  - 已知组件坏字段让 `LoadScene` 返回 false。

- `MaterialSerializationSmoke`
  - 确认 material custom serializer 行为不被 runtime generic path 破坏。

- `CLIReflectionSmoke`
  - CLI 反射输出仍可读。

## 风险

- `void*` field accessor 有类型错配风险，必须由 descriptor/type id/registry 保证匹配。
- `Offset` 对非 standard-layout 类型存在 C++ 规则风险；当前已有 `offsetof` 使用，P3 先沿用，后续可替换为 pointer-to-member accessor。
- 字段级 generic serializer 会暴露更多类型未支持问题，例如某些字段没有 `Serializer<T>`。
- 如果 provider registry 初始化顺序处理不好，`GetRuntimeTypes()` 可能依赖静态初始化顺序。首版应使用函数内 static 或 generated static arrays，避免跨 TU 初始化问题。
- `RuntimeTypeKind` 从 string 改 enum 会影响现有测试和 CLI 输出；可以先保留 string `Kind`，新增 enum 字段作为过渡。

## 推荐实施切片

1. 扩展 descriptor API 和 tests。
2. 实现 generic runtime object serializer/deserializer。
3. 修改 generator 输出 field accessors。
4. 让 component type-level serializer 委托 generic runtime object operations。
5. 加 runtime registry/provider 汇总层。
6. 接入 static reflection adapter 的第一个非组件测试类型。
7. 迁移 `ProjectDescriptor` 或 `MeshData` 中一个小类型作为验证。
8. 全量 smoke 验证并更新审计报告。

## 验收标准

- 外部 smoke 不直接使用组件静态反射。
- runtime field descriptor 能读写字段。
- 组件序列化不再依赖 per-type generated field loop，而是走 generic runtime object serializer。
- P2-C 序列化 policy 全部继续通过。
- `reflection_tool.py validate --root .` 无 diagnostics。
- generated files 无 drift。
- `srefl_class` 仍允许作为底层 provider 来源，文档和审计明确这一点。
