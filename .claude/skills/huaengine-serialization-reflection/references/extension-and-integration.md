# 扩展与集成

## 1. 什么类型适合默认反射序列化

当类型满足这些条件时，优先尝试默认 `Serializer<T>`：

- 是数据型对象
- 字段由基础类型、`std::string`、`std::vector<T>`、GLM 类型或已支持类型组成
- 不依赖运行时外部资源
- 不依赖额外持久化元数据
- 不依赖特殊磁盘协议

典型例子：

- `TransformComponent`
- 纯 mesh 数据结构
- 简单项目描述对象

前提仍然是：

- 类型已经写了 `srefl_class(...)`
- 字段都能被 `SerializeValue(...) / DeserializeValue(...)` 处理

## 2. 什么类型更适合手工 Serializer 或更高层专用序列化

当类型出现这些情况时，优先考虑手工特化或专用集成：

- 它是资源对象，落盘时更应该写路径、名字、slot，而不是内存布局
- 内部带有运行时缓存、句柄、注册表引用或进程内状态
- 需要额外元数据才能正确恢复
- 依赖领域专用的磁盘 schema

当前仓库中的典型例子：

- `Scene`
- `Material`
- `MaterialInstance`
- `Mesh`

## 3. 上层接入深度并不一致

### Project

`ProjectDescriptor` 更接近普通数据对象：

- 主要依赖反射 / 默认 serializer
- 文件位置和目录结构由 `ProjectService` 管

### Scene

`Scene` 不能只靠默认 serializer：

- 它有实体集合
- 它有组件注册和恢复规则
- 它有场景级身份和所有权语义

### Rendering 资源

`Material`、`MaterialInstance`、`Mesh` 往往还要叠加这些集成逻辑：

- shader 路径
- texture 路径
- 资源命名
- 参数和值语义

它们更像“专用持久化协议”，而不是简单字段平铺。

## 4. 当前正式 schema 约束

当前正式 schema 期望：

- 项目元数据使用 `schema_version`
- 材质根级使用 `material_type`
- 静态字段依赖代码 / 反射 / schema 上下文，不重复写逐字段 `type`
- 动态材质参数和值载荷在必要时保留显式 value-type 标记

除非架构决策明确改变，否则不要重新开放旧字段名和旧包装结构。

## 5. 给新类型补持久化时的最小检查单

建议按这个顺序判断：

1. 这是纯数据对象，还是资源 / 上下文敏感对象？
2. 如果是纯数据对象，是否已经写了 `srefl_class(...)`？
3. 字段是否都落在已支持类型集合内？
4. 如果包含资源句柄或运行时状态，是否应改为手工 serializer？
5. 如果还要进入 `Scene`、`Editor` 或其他上层系统，是否还缺注册或消费链？

## 6. 推荐排查顺序

### 反序列化失败

1. 先确认 backend 是否初始化
2. 再确认 `Serializer<T>` 是否真实可达
3. 再确认字段类型是否都已支持
4. 最后看具体 JSON 节点路径和对象 / 数组上下文

### 类型已经加了但 Scene 或资源仍然不能持久化

1. 看是否只有反射，没有专用持久化逻辑
2. 看是否只有持久化逻辑，没有上层注册
3. 看资源字段是否还缺路径 / 名称转换
4. 能复用现有 smoke 就优先复用

## 相关 Skill

- 如果这个类型还会进入 Scene 或 Editor 消费：转 `huaengine-ecs-scene`
- 如果这个类型是材质、mesh、texture 这类渲染资源：转 `huaengine-rendering`
- 如果首先要决定它落在哪个宿主 / 控制层边界：转 `huaengine-architecture`
