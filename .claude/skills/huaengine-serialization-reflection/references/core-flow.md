# Core Flow

## 1. 运行时初始化入口

序列化系统当前通过：

- `HuaEngine/src/HuaEngine/Serialization/Serialization.cpp`

里的 `InitializeSerialization()` 接入运行时。

当前事实很明确：

- 只注册 `SerializationFormat::JSON`
- backend 工厂返回 `JsonSerializationBackend`
- YAML 和 Binary 仍只是规划占位

如果你绕开 `Application::Start()` 单独写工具或测试，通常也要自己先调用初始化路径。

## 2. 反射层到底提供了什么

关键入口：

- `srefl_class(type, ...)`
- `fields(...)`
- `field(name)`
- `reflect<T>()`

这层提供的是：

- 字段名
- 字段偏移
- 访问器
- 访问遍历能力

最常用的是：

- `visit_fields(...)`

默认序列化模板就是靠它遍历字段。

## 3. 默认 Serializer<T> 的真实工作方式

关键文件：

- `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`

默认模板分两类：

- 标量和字符串
  - 直接调用 backend 的基础读写
- 复杂对象
  - `BeginObject(name)`
  - `visit_fields(...)`
  - 逐字段 `SerializeValue(...)`
  - 反序列化时按字段名读回

重要事实：

- 默认模板不会自动写类型标签
- 反序列化的类型来自编译期反射，不来自文件里的运行时类型描述
- 缺字段时会记 warning，并让整体 `success = false`

## 4. SerializationBackend 抽象层

`SerializationBackend` 统一暴露：

- `BeginObject / EndObject`
- `BeginArray / EndArray`
- 标量 `Serialize / Deserialize`
- `HasField / GetArraySize / GetFieldType`
- `GetObjectKeys / ForEachField`
- `LoadFromString / LoadFromFile / SaveToString / SaveToFile`

扩 backend 时，先保证这套契约完整，再谈上层对象是否兼容。

## 5. JsonSerializationBackend 的模型

当前 JSON backend 是自研树结构：

- `JsonNode`
  - `Object`
  - `Array`
  - `Value`
- `JsonValue`
  - `std::variant`
- `m_NodeStack`
  - 维护当前对象/数组上下文
- `m_ArrayIndices`
  - 维护当前数组索引

这解释了为什么上层对象可以在复杂对象、数组元素和字段遍历之间切换上下文。

## 6. 当前已经非常关键的两个现实边界

### 6.1 根对象空名路径是正式能力的一部分

当前 backend 在 `BeginObject("")` 时会正确处理 root object 场景。

这已经不是小细节，因为：

- `ProjectDescriptor` 这类根对象 JSON 读写依赖它
- 根对象空名路径出错，会直接影响 `project.json` 反序列化

### 6.2 JSON 是权威作者格式

在当前代码和规划里：

- JSON 是正式工作格式
- 不是“临时 demo 格式”

所以改 backend 或改通用模板时，要优先考虑：

- 项目元数据
- 场景
- 材质/mesh 资源

这些正式持久化链路是否会被影响。

## 7. 常见特化

### GLM

`GLMSerializer.h` 已为这些类型补了特化：

- `glm::vec2`
- `glm::vec3`
- `glm::vec4`
- `glm::mat3`
- `glm::mat4`

### std::vector<T>

走统一的数组序列化辅助：

- `SerializeArray(...)`
- `DeserializeArray(...)`

### Ref<T>

`Serializer<Ref<T>>` 的语义是：

- 空指针时写一个带 `is_null` 的对象
- 非空时转而读写其内部对象
- 反序列化时需要时会先 `CreateRef<T>()`

这意味着：

- `T` 本身依然必须可被正常序列化

## Related Skills

- 场景如何把组件真正登记进场景文件：
  - 看 `huaengine-ecs-scene/references/serialization-and-integration.md`
- 材质和 mesh 为什么不能只靠默认反射模板：
  - 看 `huaengine-rendering/references/assets-and-materials.md`
- backend 是在哪条宿主启动链上被注册：
  - 看 `huaengine-architecture/references/architecture.md`
