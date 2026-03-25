# Extension And Integration

## 1. 什么时候只靠反射就够了

如果类型满足以下条件，通常可以直接走默认 `Serializer<T>`：

- 字段都是基础类型、`std::string`、`std::vector<T>` 或已有特化的类型
- 字段没有运行时外部资源依赖
- 不需要写额外元数据
- 不需要特殊格式布局

典型例子：

- `TransformComponent`
- `MeshData`
- 其他纯数据结构体

前提仍然是：

- 类型写了 `srefl_class(...)`
- 字段本身的类型都能被 `SerializeValue/DeserializeValue` 处理

## 2. 什么时候必须自定义 Serializer<T>

以下情况通常要手工特化：

- 资源对象需要路径、名字或 slot，而不是直接转储内存
- 类型内部用 `std::variant`、句柄、注册表引用、运行时缓存
- 文件结构要兼容既有格式
- 需要对象外层附加额外元信息

当前仓库里的典型例子：

- `Scene`：实体数组和组件注册表驱动，不能只靠反射
- `Material` / `MaterialInstance`：要处理 shader path、参数类型字符串、纹理路径、override
- `Mesh`：要保存 mesh name 与 mesh data，而不是运行时 VAO

## 3. Scene / Material / Mesh 的接入方式

### Scene

- `SceneSerializer.cpp` 维护 `ComponentSerializers`
- 场景文件里写实体数组和组件数组
- 组件是否进入场景文件，不只取决于 `Serializer<T>`，还取决于组件是否在注册表里登记

### Material

- `MaterialSerializer` 负责 `MaterialParameter`、`Material`、`MaterialInstance`
- 参数类型会被显式写成字符串，如 `Float`、`Vec3`、`Texture2D`
- 纹理以路径字符串读写，运行时再恢复成 `Texture2D`

### Mesh

- `Mesh` 自己在 `MeshCore.h` 里提供特化
- 保存 `mesh_name` 和 `mesh_data`
- `mesh_data` 本身再通过反射型默认序列化处理

## 4. 扩新类型的最小检查单

给新类型补序列化时，建议按这个顺序检查：

1. 这个类型是否真的是“纯数据类型”
2. 若是纯数据，是否已经写了 `srefl_class(...)`
3. 字段里是否包含 GLM、vector、Ref 或其他已有支持类型
4. 若包含资源/variant/上下文对象，是否应该改成手工 `Serializer<T>`
5. 若这个类型还被 Scene 之类上层系统管理，是否需要额外登记到上层注册表

## 5. 当前实现的几个真实边界

### 格式支持边界

- 代码里有 `JSON/YAML/Binary` 三种格式枚举
- 但真正接入 `SerializationManager` 的只有 JSON

### 文档边界

- `README.md` 中提到的一些文件，如 `ReflectionSerializer.h`、`SerializationExamples.h`，当前仓库里并未看到
- README 适合作快速概览，不适合作精确入口索引

### 签名与约定边界

- 某些特化的 `Deserialize` 返回 `bool`
- 某些则只返回 `void`
- 修改通用模板或统一接口前，要先看具体调用方是否依赖现有签名差异

### JSON backend 能力边界

- 当前实现偏轻量、手写解析
- 更适合受控结构和引擎内部数据
- 对复杂 JSON 兼容性、报错质量、性能的要求不能按成熟库预期去假设

## 6. 排查建议

### 反序列化失败

1. 先看 backend 是否已初始化
2. 再看 `Serializer<T>` 是否真的存在可用路径
3. 再看字段类型是否都有可达的 `DeserializeValue(...)`
4. 最后再看 JSON 节点路径、对象/数组上下文是否匹配

### 类型扩展后 Scene/Material 仍保存失败

1. 看是否只有反射，没有自定义序列化
2. 看是否只有序列化，没有上层注册表登记
3. 看资源类字段是否需要额外路径/名字转换
4. 看测试入口是否已有可复用样例

## Related Skills

- 如果你扩的是 Scene 组件或编辑器里会显示的对象：转到 `huaengine-ecs-scene`
- 如果你扩的是材质、mesh、渲染资源路径或 GPU 相关对象：转到 `huaengine-rendering`
- 如果你还不确定改动应该落在哪个工程或目标：转到 `huaengine-architecture`