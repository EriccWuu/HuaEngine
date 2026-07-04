# Reflection Runtime Field Operations 设计

## 背景

当前反射链路已经完成了几项关键迁移：

- 组件使用 `HE_REFLECT_COMPONENT` / `HE_REFLECT_FIELD` marker，由 `reflection_tool.py` 生成 `GeneratedReflection.h/.cpp`。
- `RuntimeTypeDescriptor` / `RuntimeFieldDescriptor` 已包含字段 offset、size、flags、accessor 和字段级序列化回调。
- `ComponentRegistry` 已从生成的 runtime descriptor 注册组件。
- Scene serialization 和 Editor Inspector 已开始消费 runtime metadata。
- 旧的 Inspector `ComponentEditorRegistry` / `REGISTER_COMPONENT_EDITOR` 已删除。

但当前 runtime 反射仍主要是“字段描述表”。上层仍需要自己解释 `field.Type` 字符串，例如 Editor 自己判断 `glm::vec3`、`std::string`、`bool`。这会导致能力分散：序列化、Inspector、后续命令系统都可能各自实现一套 field type 判断和可编辑性规则。

本设计目标是把 `HE::Refl` runtime facade 推进为统一的“字段语义 + 字段操作 + enum metadata”入口。外部调用者不再直接把字段类型字符串当作主要行为分派依据，而是通过 runtime API 查询字段值分类、可编辑性和 enum 元数据，并通过通用 get/set 操作安全读写字段。

## 目标

1. 新增 `RuntimeFieldValueKind`，描述字段值在 runtime 操作层的处理分类。
2. 新增通用 runtime field 查询和读写 API，减少 Editor/Serialization/Command 层重复解释字段。
3. 支持简单 C++ enum / enum class 的 runtime metadata。
4. enum 字段在 JSON 中以字符串名作为主序列化格式。
5. Editor Inspector 对 enum 字段使用 combo 显示和编辑。
6. unsupported、readonly、复杂对象、资源引用字段有明确的 runtime 语义，不再由 Editor 自己猜。
7. 保持当前 `RuntimeTypeDescriptor` / `RuntimeFieldDescriptor` 主结构兼容，避免一次性重写 provider registry。

## 非目标

- 不做 C# 绑定或 C# 代码生成。
- 不重写完整 provider registry 架构。
- 不实现完整 UE-style property system。
- 不一次性支持所有 C++ 类型。
- 不把复杂资源字段和嵌套对象字段做成完整可编辑 UI。
- 不改变组件 JSON 外层结构。
- 不删除静态反射库；静态反射仍可作为底层 provider/adapter 能力保留。

## 命名说明

本设计使用 `RuntimeFieldValueKind`，而不是 `RuntimeFieldValueType`。

原因是它表达的不是严格 C++ 类型，而是 runtime 操作层的“处理分类”：

- `int`、`int32_t`、`long` 在 C++ 中可能是不同 type，但对 runtime field 操作可以统一归类为 `SignedInteger`。
- `glm::vec3` 的真实 type 仍是 `field.Type == "glm::vec3"`，但 Inspector 和通用操作关心的是它属于 `Float3`。
- enum 字段的真实 type 由 `RuntimeEnumDescriptor` 描述，`RuntimeFieldValueKind::Enum` 只是说明这个字段应按 enum 语义处理。

因此命名边界是：

```cpp
RuntimeTypeDescriptor        // 反射类型描述
RuntimeEnumDescriptor        // enum 类型描述
RuntimeFieldDescriptor       // 字段描述
RuntimeFieldValueKind        // 字段值处理分类
```

## Runtime 字段分类

新增：

```cpp
namespace HE::Refl {
    enum class RuntimeFieldValueKind {
        Unsupported,
        Bool,
        SignedInteger,
        UnsignedInteger,
        Float,
        Double,
        String,
        Float2,
        Float3,
        Float4,
        Enum,
        Object,
        AssetRef,
    };
}
```

分类规则：

- `bool` -> `Bool`
- `int` / `int8_t` / `int16_t` / `int32_t` / `int64_t` / signed aliases -> `SignedInteger`
- `unsigned int` / `uint8_t` / `uint16_t` / `uint32_t` / `uint64_t` / unsigned aliases -> `UnsignedInteger`
- `float` -> `Float`
- `double` -> `Double`
- `std::string` -> `String`
- `glm::vec2` -> `Float2`
- `glm::vec3` -> `Float3`
- `glm::vec4` -> `Float4`
- 字段 type 匹配已注册 enum descriptor -> `Enum`
- `Ref<...>` -> `AssetRef`
- 已反射 struct/class 但本阶段无嵌套编辑能力 -> `Object`
- 其他 -> `Unsupported`

`RuntimeFieldValueKind` 是 runtime 行为分类，不替代 `field.Type` 的 C++ 类型字符串。

## Runtime Enum Metadata

新增：

```cpp
namespace HE::Refl {
    struct RuntimeEnumValueDescriptor {
        std::string_view Name;
        int64_t Value;
        std::string_view DisplayName;
    };

    struct RuntimeEnumDescriptor {
        std::string_view Name;
        std::string_view QualifiedName;
        std::string_view UnderlyingType;
        std::span<const RuntimeEnumValueDescriptor> Values;
    };
}
```

新增查询 API：

```cpp
namespace HE::Refl {
    std::span<const RuntimeEnumDescriptor> GetRuntimeEnums();
    const RuntimeEnumDescriptor* FindRuntimeEnum(std::string_view qualifiedName);
    const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByName(
        const RuntimeEnumDescriptor& enumType,
        std::string_view name);
    const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByValue(
        const RuntimeEnumDescriptor& enumType,
        int64_t value);
}
```

`RuntimeFieldDescriptor` 增加可选 enum 引用：

```cpp
const RuntimeEnumDescriptor* EnumType;
```

对于非 enum 字段，`EnumType == nullptr`。

## Marker 和工具扫描

新增空 marker：

```cpp
#define HE_REFLECT_ENUM(...)
```

使用方式：

```cpp
namespace HE::Rendering {
    HE_REFLECT_ENUM()
    enum class BlendMode {
        Opaque,
        Masked,
        Transparent
    };
}
```

允许 metadata：

```cpp
HE_REFLECT_ENUM(DisplayName="Blend Mode")
enum class BlendMode {
    Opaque,
    Masked,
    Transparent
};
```

本阶段 enum value display name 不做单独 marker。`DisplayName` 预留在 descriptor 中，初期生成为空字符串，Editor 回退使用 value `Name`。

### 支持范围

`reflection_tool.py` 支持：

- `enum`
- `enum class`
- 隐式递增值
- 十进制整数值
- 十六进制整数值
- 负整数值

示例：

```cpp
HE_REFLECT_ENUM()
enum class Example {
    A,
    B = 4,
    C,
    D = -1,
    E = 0x10
};
```

复杂表达式暂不支持：

```cpp
enum class Flags {
    A = 1 << 0
};
```

遇到复杂表达式时，工具必须输出明确 diagnostic，例如 `enum.value_unsupported_expression`，并拒绝生成，不能生成错误或不完整 metadata。

## Runtime Field API

新增 API：

```cpp
namespace HE::Refl {
    RuntimeFieldValueKind GetRuntimeFieldValueKind(const RuntimeFieldDescriptor& field);
    bool IsRuntimeFieldSerializable(const RuntimeFieldDescriptor& field);
    bool IsRuntimeFieldEditable(const RuntimeFieldDescriptor& field);
    bool IsRuntimeFieldReadOnly(const RuntimeFieldDescriptor& field);

    const void* GetRuntimeFieldConst(
        const RuntimeFieldDescriptor& field,
        const void* object);

    void* GetRuntimeFieldMutable(
        const RuntimeFieldDescriptor& field,
        void* object);

    bool CopyRuntimeFieldValue(
        const RuntimeFieldDescriptor& field,
        const void* sourceObject,
        void* targetObject);

    bool GetRuntimeEnumFieldValue(
        const RuntimeFieldDescriptor& field,
        const void* object,
        int64_t& outValue);

    bool SetRuntimeEnumFieldValue(
        const RuntimeFieldDescriptor& field,
        void* object,
        int64_t value);

    bool SetRuntimeEnumFieldValueByName(
        const RuntimeFieldDescriptor& field,
        void* object,
        std::string_view valueName);

    template<typename T>
    bool GetRuntimeFieldValue(
        const RuntimeFieldDescriptor& field,
        const void* object,
        T& outValue);

    template<typename T>
    bool SetRuntimeFieldValue(
        const RuntimeFieldDescriptor& field,
        void* object,
        const T& value);
}
```

语义要求：

- object 为空返回 false/nullptr。
- accessor 为空返回 false/nullptr。
- `SetRuntimeFieldValue<T>` 必须检查 size 和 editable/read-only 状态。
- `GetRuntimeFieldValue<T>` 必须检查 size，避免错误类型读写。
- enum get/set 使用 `int64_t` 作为 runtime 交换值。
- enum 写入时只允许 descriptor 中存在的 value。
- 失败时不写入目标对象。

## Flags 和可编辑性

现有：

```cpp
enum class RuntimeFieldFlags : uint32_t {
    None = 0,
    Serializable = 1u << 0,
    Editable = 1u << 1,
    ReadOnly = 1u << 2,
    ComponentField = 1u << 3,
};
```

本阶段约定：

- `Serializable` 控制 generic serialization 是否访问字段。
- `Editable` 表示该字段允许 Editor/命令系统通过 generic runtime API 修改。
- `ReadOnly` 表示字段可显示、可读取，但不能通过 generic set 修改。
- 如果生成器没有显式 metadata，组件字段默认：
  - `Serializable`
  - `ComponentField`
  - 对基础可编辑类型额外标 `Editable`
  - 对 `Ref<...>`、object、unsupported 不标 `Editable`

`IsRuntimeFieldEditable(field)` 必须同时考虑：

1. `Editable` flag。
2. 非 `ReadOnly`。
3. 有 mutable accessor。
4. `RuntimeFieldValueKind` 不是 `Unsupported` / `Object` / `AssetRef`。

## Enum 序列化

enum 主序列化格式使用字符串名：

```json
{
  "Mode": "Transparent"
}
```

序列化规则：

1. 字段为 enum 时，读取当前整数值。
2. 在 `RuntimeEnumDescriptor::Values` 中查找对应 value。
3. 找到则写 value `Name` 字符串。
4. 找不到则该字段序列化失败策略为跳过字段并记录诊断的后续扩展；本阶段 `SerializeRuntimeObject` 无诊断通道，先不写该字段。

反序列化规则：

1. 字段存在且为字符串。
2. 按字符串在 enum descriptor 中查找 value。
3. 找到后写回 enum 字段。
4. 找不到则该字段失败，不写回，`DeserializeRuntimeObject` 返回 false。
5. 字段缺失保持默认值并返回成功。

整数兼容读取不是主格式。本阶段可以不实现；如果实现，只能作为兼容 fallback，不能改变保存格式。

## Editor Inspector 集成

`Editor/src/Panels/RuntimeInspector.*` 应从本地 `RuntimeFieldEditKind` 迁移到核心 `RuntimeFieldValueKind`。

处理规则：

- `Bool` -> checkbox
- `SignedInteger` / `UnsignedInteger` -> scalar input
- `Float` / `Double` -> scalar input
- `String` -> resize-safe `InputText`
- `Float2/Float3/Float4` -> vector drag
- `Enum` -> combo
- `Object` / `AssetRef` / `Unsupported` -> disabled text
- `ReadOnly` -> disabled control or text display

enum combo：

- 当前值通过 `GetRuntimeEnumFieldValue` 获取。
- 当前显示名优先 value `DisplayName`，否则 value `Name`。
- 用户选择后调用 `SetRuntimeEnumFieldValue`。
- 设置失败时不改变对象，不标记 dirty。

## 生成器输出调整

`reflection_tool.py` 需要：

1. 扫描 `HE_REFLECT_ENUM`。
2. 生成 `RuntimeEnumValueDescriptor[]`。
3. 生成 `RuntimeEnumDescriptor[]`。
4. 在 `RuntimeFieldDescriptor` 中为 enum 字段填充 `EnumType` 指针。
5. 为 enum 字段生成字段级 serialize/deserialize 时使用字符串名。
6. 对复杂 enum value 表达式报错。

生成文件仍保持：

- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`

不恢复 per-source `.generated.h`。

## 序列化调用链

组件保存：

```text
SceneSerializer
  -> ComponentRegistry metadata
  -> metadata.RuntimeType
  -> Refl::SerializeRuntimeObject(type, backend, name, component)
  -> RuntimeFieldDescriptor.Serialize
  -> enum field writes string name
```

组件加载：

```text
SceneSerializer
  -> metadata.ConstructDefault
  -> Refl::DeserializeRuntimeObject(type, backend, name, component)
  -> RuntimeFieldDescriptor.Deserialize
  -> enum string name resolves to integer value
  -> metadata.AddCopyToWorld on success
```

保持既有 policy：

- 缺失字段保持默认值。
- 未知字段忽略。
- 已知字段解析失败返回 false。
- 失败字段不半写。

## 测试策略

新增或扩展 smoke：

1. `ReflectionGeneratedSmoke`
   - 验证 `RuntimeFieldValueKind` 分类。
   - 验证 `GetRuntimeFieldValue` / `SetRuntimeFieldValue` 可读写 `TransformComponent.Position`、`CameraComponent.Primary`、`MeshComponent.MeshAssetName`。
   - 验证 enum descriptor 可查找。
   - 验证 enum field 可按整数值和字符串名写入。
   - 验证 unsupported/readonly 字段不会被 generic set 错误写入。

2. `ReflectionToolSmoke`
   - 增加 enum 正向 fixture。
   - 增加复杂 enum expression 负向 fixture。
   - 验证 generated files 无 drift。

3. `EditorInspectorRuntimeSmoke`
   - 验证 Editor 使用核心 `RuntimeFieldValueKind`。
   - 验证 enum 字段被识别为 `Enum`。

4. `SerializationPolicySmoke`
   - 增加 enum 字符串序列化 round-trip。
   - 验证未知 enum 字符串反序列化失败且不半写。

5. 继续保持以下目标通过：
   - `ReflectionSmoke`
   - `ReflectionGeneratedSmoke`
   - `ReflectionToolSmoke`
   - `SerializationPolicySmoke`
   - `EditorInspectorRuntimeSmoke`
   - `Editor`

## 风险

- `field.Type` 字符串仍是分类输入，短期可接受；后续可演进为更完整 runtime type id。
- enum underlying type 多样，runtime 操作统一用 `int64_t` 交换值，需要生成器保证写回转换正确。
- C++ enum 复杂表达式解析如果做过头会变成编译器前端；本阶段必须限制范围并明确报错。
- `Editable` 默认策略可能影响 Editor 可编辑字段范围，需要 smoke 覆盖当前核心组件行为。
- enum 序列化改为字符串会影响新增 enum 字段的 schema；当前已有组件无 enum 字段，因此不会破坏现有 scene。

## 验收标准

- `RuntimeFieldValueKind` 和通用 field API 在 `HE::Refl` 中可用。
- Editor Inspector 的字段分类不再维护独立 enum，而是消费 `RuntimeFieldValueKind`。
- `HE_REFLECT_ENUM` 可扫描、生成、查询。
- enum 字段 JSON 保存为字符串名。
- enum 字段读取未知字符串返回失败且不半写。
- unsupported/object/asset ref 字段有稳定 disabled 行为。
- `reflection_tool.py validate --root .` 无 diagnostics。
- 生成文件无 drift。
- 相关 smoke 和 `Editor` 构建通过。
