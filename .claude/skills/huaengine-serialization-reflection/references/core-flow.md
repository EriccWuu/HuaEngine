# Core Flow

## 1. 启动与初始化

序列化系统的运行时接入点很简单：

- `Application` 构造时调用 `HE::Serialization::InitializeSerialization()`
- `InitializeSerialization()` 当前只向 `SerializationManager` 注册 `SerializationFormat::JSON`
- backend 工厂返回 `JsonSerializationBackend`

这意味着：

- 大多数运行时序列化功能默认都假设 JSON backend 已经完成初始化
- 单元测试或独立工具代码如果绕开 `Application`，要自己先调 `InitializeSerialization()`

## 2. Reflection 提供了什么

关键入口是 `Reflection.h`：

- `srefl_class(type, ...)` 为类型生成 `type_info<T>` 特化
- `fields(...)` 收集字段描述
- `field(name)` 记录字段指针、名字和偏移
- `reflect<T>()` 返回 `reflect_info<T>`

`reflect_info<T>` 的核心能力：

- `visit_fields(...)`
- `visit_member_variables(...)`
- `visit_member_functions(...)`
- `name()`
- `has_fields()/has_bases()/has_ctors()`

当前序列化最常用的是 `visit_fields(...)` 和字段偏移读写。

## 3. 默认 Serializer<T> 如何工作

`SerializationCore.h` 里的泛型 `Serializer<T>` 逻辑分两类：

### 基础类型

- 直接走 backend 的标量 `Serialize/Deserialize`

### 复杂对象

- `Serialize(...)` 时先 `BeginObject(name)`
- 通过 `Refl::reflect<T>().visit_fields(...)` 遍历字段
- 用字段名直接调用 `SerializeValue(...)`
- `Deserialize(...)` 时按相同字段名读取，并通过字段偏移直接写回对象

重要事实：

- 默认对象序列化不写类型标签
- 字段类型来自编译期反射，不来自文件里的运行时类型描述
- 缺字段会触发 warning，并把 `success` 置为 false

## 4. SerializationBackend 抽象层

`SerializationBackend` 统一暴露：

- `BeginObject/EndObject`
- `BeginArray/EndArray`
- 标量 `Serialize/Deserialize`
- `HasField/GetArraySize/GetFieldType`
- `GetObjectKeys/ForEachField`
- `LoadFromString/LoadFromFile/SaveToString/SaveToFile`

上层模板和专门序列化器都依赖这套接口，所以扩新 backend 时的首要工作不是改业务代码，而是先完整实现这套协议。

## 5. JsonSerializationBackend 的真实模型

当前 JSON backend 不是 DOM 第三方库，而是自定义：

- `JsonNode` 用 `Object / Array / Value` 三类节点表示树结构
- `JsonValue` 用 `std::variant` 表示标量值
- `m_NodeStack` 维护当前对象/数组上下文
- `m_ArrayIndices` 维护当前数组元素索引

读写要点：

- 写模式下，`BeginObject/BeginArray` 会创建新节点并挂到当前节点下
- 读模式下，`BeginObject/BeginArray` 会导航到已有节点
- `ForEachField(...)` 会临时把 value node 压栈，再调用回调

这也是 Scene/Material 这类手工序列化能在对象字段间遍历切换的基础。

## 6. 常见特化层

### GLM

`GLMSerializer.h` 为以下类型补了专门特化：

- `glm::vec2`
- `glm::vec3`
- `glm::vec4`
- `glm::mat3`
- `glm::mat4`

向量走对象 `{x,y,z,w}` 形式，矩阵走数组形式。

### std::vector<T>

通用数组序列化走 `SerializeArray/DeserializeArray`。

### Ref<T>

`Serializer<Ref<T>>` 会：

- 空指针时写一个带 `is_null` 的对象
- 非空时转而序列化其内部对象
- 反序列化时可能先 `CreateRef<T>()` 再填充内容

这意味着 `T` 本身必须仍然能被正常反序列化。

## 7. 测试和验证入口

当前仓库里最直接的验证入口：

- `HuaEngine/src/HuaEngine/Test/SerializationTest.h`
- `HuaEngine/src/HuaEngine/Test/SceneSerializationTest.h`
- `HuaEngine/src/HuaEngine/Test/TestReflection.cpp`

这些文件分别覆盖：

- `ToJson/FromJson`
- 场景保存/加载
- 基于 `reflect<T>()` 的字段遍历与字段写回

## Related Skills

- Scene 如何在上层登记组件并写入实体数组：转到 `huaengine-ecs-scene/references/serialization-and-integration.md`
- 材质和 mesh 为什么要叠加手工序列化，而不是只靠默认反射：转到 `huaengine-rendering/references/assets-and-materials.md`
- backend 在引擎启动链路中的位置：转到 `huaengine-architecture/references/architecture.md`