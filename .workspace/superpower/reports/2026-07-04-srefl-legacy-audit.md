# srefl legacy 使用审计

## P3 更新：runtime facade 与静态反射 provider 状态

- 组件主路径已经统一到 `HE::Refl` runtime facade：`ComponentRegistry` 只关联 `RuntimeTypeDescriptor`，场景序列化和 `Serializer<T>` 的组件分支都调用 `SerializeRuntimeObject` / `DeserializeRuntimeObject`。
- 普通组件的 generated type-level `Serialize_HE__*` / `Deserialize_HE__*` 函数已经删除；组件字段改为生成 `RuntimeFieldDescriptor` 的 offset/size/flags/accessor/field operation。
- `RuntimeFieldDescriptor` 已具备 offset、size、flags、const/mutable accessor、field serialize/deserialize callback，支持通用 runtime object traversal。
- `GetRuntimeTypes()` 已通过 provider 聚合缓存返回 generated runtime descriptors，不再在对外实现里直接裸返回 generated 数组。
- 静态反射未删除，而是作为内部 metadata 来源保留：`MakeStaticRuntimeTypeDescriptor<T>()` 可以把已有 `srefl_class` 字段适配成 runtime descriptor。
- `ProjectDescriptor` 已通过 `ReflectionRuntimeProviderSmoke` 覆盖静态反射到 runtime descriptor 的适配和 generic runtime serialize/deserialize；其现有手写 `Serializer<ProjectDescriptor>` 暂时保留，避免改变项目文件 snake_case schema。
- 剩余迁移候选仍包括 `MeshData` 系列、README 编码/内容清理，以及 `SerializationCore.h` 中非组件 `Refl::reflect<T>()` legacy fallback 的边界收敛。

## 结论

组件序列化路径已经统一到 `HE::Refl` runtime descriptor facade：组件注册、组件序列化和组件反序列化优先走运行时 descriptor，不再依赖 `srefl_class` 生成静态反射信息。当前 `srefl_class` 仍然存在，但它已经不是组件序列化路径的必要依赖。

现有静态反射库仍保留为非组件 fallback/legacy 能力。`SerializationCore.h` 中的通用 `Serializer<T>` 在没有命中特化 serializer、且类型不是已注册组件 runtime descriptor 时，仍会调用 `Refl::reflect<T>()` 遍历静态字段。因此彻底移除 `srefl_class` 前，需要先替换或下线这条非组件 fallback 路径。

## 当前命中

审计命令：

```powershell
rg -n "srefl_class|Refl::reflect|type_info<" HuaEngine/src Tests
```

### 核心库定义

- `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
  - `type_info<T>` 是静态反射类型信息入口。
  - `srefl_class` / `srefl_enum` 宏用于生成 `HE::Refl::type_info<...>` 特化。
  - `Refl::reflect<T>()` 返回静态 `reflect_info`，供 legacy 序列化 fallback 遍历字段。

### 序列化 fallback

- `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
  - 通用 `Serializer<T>::Serialize` 在未命中特化 serializer 时调用 `Refl::reflect<T>()` 并遍历字段写入 JSON。
  - 通用 `Serializer<T>::Deserialize` 同样通过 `Refl::reflect<T>()` 遍历字段读取 JSON。
  - 对组件类型已先尝试 `Refl::FindRuntimeType(ComponentTypeIdOf<T>())`，命中 runtime descriptor 时直接返回，不进入静态反射 fallback。

### 生产类型

- `HuaEngine/src/HuaEngine/Project/ProjectContext.h`
  - `HE::ProjectDescriptor` 仍声明 `srefl_class`。
  - 同文件已有 `HE::Serialization::Serializer<HE::ProjectDescriptor>` 手写特化，使用小写字段名和 schema 字段名。

- `HuaEngine/src/HuaEngine/Rendering/Mesh/MeshData.h`
  - `HE::Rendering::SerializableBufferElement`、`SerializableBufferLayout`、`MeshData` 仍声明 `srefl_class`。
  - 同文件已有三组手写 `Serializer` 特化，使用稳定的序列化字段名，例如 `shader_data_type`、`vertex_data`、`index_data`、`layout`。

### 测试

- `Tests/ReflectionToolSmoke.cpp`
  - 明确检查 `GeneratedReflection.h` 不包含 `srefl_class(`。
  - 该测试用于防止组件 runtime descriptor 生成物回退到 legacy 静态反射宏。

- `Tests/SerializationSmoke.cpp`
  - `HE::SmokePlayerComponent` 是测试用 fixture，仍使用 `srefl_class` 覆盖静态反射 fallback 的 round-trip 行为。

- `Tests/SerializationPolicySmoke.cpp`
  - `HE::PolicyRefPayload`、`PolicyRefHolder`、`PolicyReflectedComponent` 是测试用 fixture，仍使用 `srefl_class` 覆盖 policy 和 fallback 序列化行为。

### 文档

- `HuaEngine/src/HuaEngine/Serialization/README.md`
  - 仍包含旧式 `srefl_class(MyComponent, ...)` 示例。
  - 该 README 当前还存在编码显示异常，后续更新时应同时统一文档编码和序列化推荐路径。

## 保留项

- `Reflection.h` 静态反射实现短期应保留。它仍是 `srefl_class`、`type_info<T>`、`Refl::reflect<T>()` 的定义位置，也是非组件 fallback 的基础设施。
- `SerializationCore.h` 通用 fallback 短期应保留。当前测试和非组件数据类型仍可能依赖它处理未提供手写 serializer 的普通类型。
- 测试 fixture 中的 `srefl_class` 可短期保留。它们为 legacy fallback 提供覆盖，避免在下线前失去回归信号。
- `ProjectDescriptor` 与 `MeshData` 系列的 `srefl_class` 不建议无验证直接删除。虽然已有手写 `Serializer` 特化，但仍需确认所有调用路径都稳定命中特化，而不是隐式依赖静态 fallback。

## 迁移候选

- `ProjectDescriptor`
  - 优先确认项目文件读写路径全部命中 `Serializer<ProjectDescriptor>`。
  - 若确认无 fallback 依赖，可删除 `srefl_class(HE::ProjectDescriptor, ...)`。

- `MeshData` 系列
  - 优先确认 mesh 资产或场景序列化路径全部命中 `SerializableBufferElement`、`SerializableBufferLayout`、`MeshData` 的手写 serializer。
  - 若确认无 fallback 依赖，可删除三处 `srefl_class`。

- `Serialization README`
  - 将“组件自动序列化”示例迁移为 runtime descriptor / reflection tool 语义。
  - 将“普通类型”示例改为推荐手写 `Serializer<T>`，或明确标注 `srefl_class` 是 legacy fallback。

- 测试 fixture
  - 可以逐步拆分为两类测试：runtime descriptor 组件测试、legacy fallback 非组件测试。
  - 当 fallback 下线后，相关 fixture 应改为手写 serializer 或未来 runtime descriptor 方案。

## 删除前置条件

完全删除 `srefl_class` 前至少需要满足：

- 有明确的非组件反射路径替代方案，例如手写 serializer 规范、非组件 runtime descriptor，或确定不再支持自动反射 fallback。
- `HuaEngine/src/HuaEngine/Serialization/README.md` 更新为当前推荐路径，不再把 `srefl_class` 作为新代码默认方案。
- `ProjectDescriptor` 迁移完成，或确认其手写 `Serializer<ProjectDescriptor>` 覆盖所有序列化入口。
- `MeshData` 系列迁移完成，或确认其手写 serializer 覆盖所有序列化入口。
- `SerializationCore.h` 的 `Refl::reflect<T>()` fallback 下线、替换，或被明确限定在 legacy 编译开关内。
- `Tests/SerializationSmoke.cpp` 和 `Tests/SerializationPolicySmoke.cpp` 的静态反射 fixture 完成改造，或迁移为专门验证 legacy 开关的测试。
- `Tests/ReflectionToolSmoke.cpp` 继续保留“生成物不含 `srefl_class`”的断言，确保组件路径不会回退。

## 风险与 TODO

- `ProjectDescriptor` 与 `MeshData` 系列已经有手写 `Serializer` 时，对应 `srefl_class` 可能冗余；但删除前必须用调用路径和测试验证覆盖范围，避免破坏隐式 fallback。
- 静态 fallback policy 与 runtime descriptor policy 需要保持一致，尤其是字段缺失、失败返回、默认值保留、`Ref<T>` 处理和嵌套对象行为。
- 当前通用 fallback 使用 C++ 字段名，而部分手写 serializer 使用稳定 snake_case 字段名；迁移时需要避免同一类型出现两套 JSON schema。
- 低级 `LoadScene` 返回 `false` 后可能存在部分加载状态，这是 residual risk。此前 quality review 判断该问题不阻塞本阶段，但后续仍需要作为场景加载语义决策单独处理。
- README 存在编码显示异常，后续文档修订应避免继续传播乱码内容。
