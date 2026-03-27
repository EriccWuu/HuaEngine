# Extension And Integration

## 1. 什么类型适合直接走默认反射模板

通常满足这些条件时，可以先尝试默认 `Serializer<T>`：

- 纯数据结构
- 字段都是基础类型、`std::string`、`std::vector<T>`、GLM 类型或已有特化的类型
- 不依赖运行时外部资源
- 不需要额外元数据
- 不需要兼容特殊磁盘格式

典型例子：

- `TransformComponent`
- 纯 mesh 数据结构
- 简单项目描述对象

前提仍然是：

- 类型写了 `srefl_class(...)`
- 字段本身都能被 `SerializeValue(...) / DeserializeValue(...)` 处理

## 2. 什么类型更适合手工 Serializer<T>

出现这些情况时，优先考虑手工特化：

- 资源对象需要写路径、名字、slot，而不是直接写内存结构
- 类型内部有运行时缓存、句柄、注册表引用
- 需要兼容既有文件结构
- 需要额外元数据
- 单靠字段平铺无法表达真实协议

当前仓库中的典型例子：

- `Scene`
- `Material`
- `MaterialInstance`
- `Mesh`

## 3. Scene / Project / Rendering 的接入深度并不一样

### Project

`ProjectDescriptor` 更接近纯数据对象：

- 主要依赖反射默认模板
- 由 `ProjectService` 管理它的文件位置和目录结构

### Scene

`Scene` 不能只靠默认模板：

- 有实体数组
- 有组件登记表
- 有 `entity_id` 和组件名/类型映射

### Rendering Resources

`Material`、`MaterialInstance`、`Mesh` 这类对象通常还要叠加：

- shader path
- texture path
- 资源名字
- 参数类型语义

因此它们更偏“专用序列化协议”，不是简单字段平铺。

## 4. 给新类型补持久化时的最小检查单

建议按这个顺序判断：

1. 这是纯数据对象，还是资源/上下文敏感对象
2. 如果是纯数据对象，是否已经写了 `srefl_class(...)`
3. 字段是否都落在已支持类型集合内
4. 如果包含资源句柄或运行时状态，是否应该手工 `Serializer<T>`
5. 如果它还要进入 Scene、Editor 或其他上层系统，是否还要补登记表或宿主消费逻辑

## 5. 当前实现的几个真实边界

### 格式支持边界

- 代码里有多格式枚举
- 运行时真正注册的只有 JSON

### README 边界

- README 可以当概览
- 不能当精确事实源
- 精确行为以当前头文件和实现为准

### 签名边界

某些特化的 `Deserialize(...)` 返回值风格并不完全统一。

所以改通用模板前，要先核对：

- 现有调用点
- 特化返回值语义
- 上层是否依赖当前差异

### JSON backend 能力边界

当前实现更适合：

- 受控结构
- 引擎内部协议
- 可预测的读写链路

不应默认把它当成：

- 强鲁棒通用 JSON 工具库
- 自动容错复杂外部 JSON 的入口

## 6. 推荐排查顺序

### 反序列化失败

1. 先看 backend 是否已初始化
2. 再看 `Serializer<T>` 是否真的可达
3. 再看字段类型是否都支持
4. 最后看 JSON 节点路径和对象/数组上下文是否匹配

### 类型扩展后 Scene 或资源仍保存失败

1. 看是否只有反射，没有专用序列化
2. 看是否只有专用序列化，没有进入上层登记表
3. 看资源字段是否还缺路径或名字转换
4. 看是否已有 smoke 可以直接复用验证

## Related Skills

- 扩展的是 Scene 组件或 Editor 会看到的对象：
  - 转到 `huaengine-ecs-scene`
- 扩展的是材质、mesh、texture 或渲染资源：
  - 转到 `huaengine-rendering`
- 先不确定改动落在哪个宿主或目标：
  - 转到 `huaengine-architecture`
