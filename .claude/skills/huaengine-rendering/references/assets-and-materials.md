# Assets And Materials

## 1. 材质系统结构

关键文件：

- `Material/MaterialCore.h`
- `Material/MaterialCore.cpp`
- `Material/MaterialTypes.h`
- `Material/MaterialLibrary.h`
- `Material/MaterialSerializer.h`
- `Material/MaterialSerializer.cpp`

当前模型：

- `Material` 持有 shader、参数定义、默认值、texture slot 映射
- `MaterialInstance` 持有对 `BaseMaterial` 的参数覆盖
- 提交绘制时，最终还是由 `BaseMaterial` 的 shader 负责 uniform / texture 绑定

## 2. 参数绑定规则

`MaterialParameterValue` 当前基于 `std::variant`，常见类型包括：

- `int`
- `float`
- `glm::vec2/vec3/vec4`
- `glm::mat3/mat4`
- `Ref<Texture2D>`
- `std::vector<int>`
- `std::vector<float>`

`Material::ApplyParameter(...)` 的行为：

- 标量和向量直接写 shader uniform
- `Texture2D` 会先通过 texture slot 绑定，再把 slot 写回 uniform
- `FloatArray` 目前只打 warning，没有完整 shader 支持

这意味着：

- 新增材质参数类型时，不只改 enum；还要补 variant、序列化和 `ApplyParameter(...)`
- 纹理类参数若没设置 slot，会走默认 slot 逻辑，先检查 `AddParameter()` 和 `SetTextureSlot()`

## 3. MaterialInstance 行为

`MaterialInstance::ApplyParameters()` 顺序是：

1. 先应用 base material 的默认参数
2. 再应用 instance 的 override

所以排查“实例参数没生效”时，先看：

- 这个参数是否存在于 base material
- override 是否真的写进 `m_ParameterOverrides`
- 最终 shader uniform 名称是否和参数名一致

## 4. Mesh 与 MeshManager

关键文件：

- `Mesh/MeshCore.h`
- `Mesh/MeshCore.cpp`
- `Mesh/MeshData.h`
- `Mesh/MeshManager.h`
- `Mesh/MeshManager.cpp`

当前工作方式：

- `Mesh` 同时保存 CPU 侧 `MeshData` 和运行时 `VertexArray`
- `GetVertexArray()` 是惰性上传，只有第一次访问才会 `LoadToGPU()`
- `MeshManager` 管理按名字注册的 mesh，并支持默认几何体 `Quad/Cube/Sphere`
- `MeshComponent` 可以只序列化 `MeshAssetName`，运行时再从 `MeshManager` 回取实际网格

这意味着：

- 场景反序列化后如果只恢复了 `MeshAssetName`，还需要确保 `MeshManager` 中已经有对应 mesh
- CPU 侧 mesh 数据变了但没重新加载时，要考虑 `ReloadToGPU()` 或清空缓存

## 5. 与序列化的连接点

### Mesh

- `Mesh` 在 `MeshCore.h` 里直接提供 `Serializer<Rendering::Mesh>` 特化
- `SaveMesh` / `LoadMesh` 通过通用序列化接口落文件
- `MeshData` 和 `SerializableBufferLayout` 通过反射注册

### Material

- `MaterialSerializer.h/.cpp` 提供 `MaterialParameter`、`Material`、`MaterialInstance` 的序列化特化
- `Material` 存 `shader_path`、参数表和 texture slot
- `MaterialInstance` 主要存 base material name 和参数覆盖

排查“读档后渲染不对”时，优先检查：

1. 材质文件里的 `shader_path` 是否有效
2. 参数类型字符串是否能正确映射回 `MaterialParameterType`
3. 纹理路径是否可被 `Texture2D::Create(...)` 找到
4. mesh 名称是否已在 `MeshManager` 中注册

## 6. 典型问题定位顺序

### 物体不显示

1. `MeshComponent` 是否拿到有效 VAO
2. `MaterialInstance` 是否存在且能拿到 shader
3. Camera/Transform 矩阵是否正确
4. FrameBuffer 是否绑定并有有效尺寸
5. OpenGL draw call 是否真的触发

### 材质参数不生效

1. 参数名是否和 shader uniform 名一致
2. 参数是否先定义在 `Material`，而不是只写在 `MaterialInstance`
3. 参数类型是否和 `ApplyParameter(...)` 支持分支一致
4. 纹理 slot 和 sampler uniform 是否匹配

### 序列化后资源丢失

1. shader 路径、纹理路径是否还是相对当前工作目录可解析
2. mesh 是否在运行时提前注册
3. 读取后是否只恢复了名字，没恢复资源注册表

## Related Skills

- 如果资源问题实际是场景侧没有正确提供 `MeshComponent` / `MaterialComponent`：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 如果资源读写问题涉及 `Serializer<T>`、GLM、`Ref<T>` 或 backend 行为：转到 `huaengine-serialization-reflection/references/core-flow.md`