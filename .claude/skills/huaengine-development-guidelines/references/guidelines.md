# 开发规范正文

这份 reference 固化 [development-guidelines.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/development-guidelines.md) 的当前正式约束。

## 1. 目的

本文档用于约束 HuaEngine 后续开发方式。后续所有新功能、重构、Bug 修复、工具接入，都应优先遵循本文档，而不是按单点需求临时扩散实现。

相关架构参考：

- [refactored-engine-architecture.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/refactored-engine-architecture.md)
- [editor-project-workbench.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/editor-project-workbench.md)
- [huaengine-headless-cli.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/huaengine-headless-cli.md)

## 2. 宿主与控制面约束

- 宿主要薄：
  - `ProjectHub.exe` 负责无项目入口、项目选择、项目创建、拉起 `Editor.exe`
  - `Editor.exe` 负责项目绑定的 GUI 工作台
  - `HuaEngineHeadless.exe` 负责 CLI / 自动化 / Agent 调用
- 正式能力必须有唯一权威入口：
  - `Application`
  - `ApplicationOperations`
  - `OperationRegistry`
  - `ResultEnvelope`
- 所有共享写操作必须统一走正式操作面，不能在不同宿主各写一套

## 3. 读取与写入边界

- 共享写操作：
  - 项目创建 / 打开 / 状态检查
  - 场景创建 / 加载 / 保存 / 校验
  - 实体创建 / 删除
  - 组件添加 / 删除
  - 资产登记 / 校验
  - 脚本状态 / 初始化 / 更新 / 关闭
  - 聚合验证
- 读取能力分两类：
  - 共享查询能力：后续逐步沉淀为正式 query facade
  - `Editor` 实时面板读取：允许在仅只读的 GUI 渲染路径内直接读运行时 `Scene` / `EntityManager` / `entt::registry`

## 4. 分层约束

- Host Layer：
  - 接收输入
  - 管理窗口、命令行、菜单、快捷键、面板等宿主交互
  - 调用正式操作面
  - 消费返回结果并展示
- Control Layer：
  - 统一宿主调用入口
  - 统一操作命名
  - 统一结果协议
  - 编排对领域服务的调用
- Domain Layer：
  - `ProjectService`
  - `SceneService`
  - `AssetService`
  - `ScriptService`
  - `ValidationService`
- Engine Core Layer：
  - ECS
  - Scene
  - Serialization
  - Reflection
  - Rendering
  - OpenGL backend
  - Window / Event / ImGui 集成底座

## 5. Editor 工作台约束

- 当前现阶段参考状态模型：
  - `ProjectSession`
  - `SceneDocument`
  - `EditorWorkbenchState`
- 当前现阶段参考交互接入点：
  - `EditorInteractionHost`
  - `EditorCommandStack`
  - `ContextMenuRegistry`
  - `ShortcutRegistry`
  - `DragDropIntentRegistry`
- 要求：
  - `Undo / Redo` 的写操作必须命令化
  - 右键菜单必须通过注册面接入
  - 快捷键必须通过注册面接入
  - 拖拽意图必须通过注册面接入

## 6. 场景、实体、组件约束

- 后续所有实体和组件写操作都必须优先进入 `ApplicationOperations`
- `Editor` 命令层只做调用与撤销编排，不拥有另一套写语义
- `Headless` 只能调用正式操作面，不允许自己补一套场景改写逻辑
- 默认规则：一个实体上，同类型组件默认只允许一份
- 组件归属规则：
  - 基础组件放核心层
  - 模块专属组件放各自模块
  - 序列化、Inspector、验证通过注册表或显式接线聚合

## 7. 序列化与资源约束

- 序列化风格应接近现代引擎：
  - 对象级别保留必要类型信息
  - 字段级别仅在动态类型场景下保留类型标签
  - 静态字段类型由代码、反射、schema 决定，不冗余落盘
- 当前正式 schema 已收紧：
  - 项目元数据使用 `schema_version`
  - 材质根级类型使用 `material_type`
  - 场景组件使用更直接的对象结构
  - 旧格式已不再作为正式兼容目标
- 引擎资源根：
  - 开发态：`Resources/`
  - 构建产物：`${output_root}/Resources`

## 8. 测试、文档与同步

- 新增正式能力必须补 smoke 或回归验证
- 每次重要改动至少覆盖：
  - 编译通过
  - 对应 smoke / 回归通过
- 影响架构、宿主职责、正式操作面、工作流入口的改动，必须同步更新：
  - `docs/refactored-engine-architecture.md`
  - `docs/editor-project-workbench.md`
  - `docs/huaengine-headless-cli.md`
  - 对应模块 Skill

## 9. 禁止事项

- 在 `Editor` 和 `Headless` 中各自实现一套相同写逻辑
- 绕过 `ApplicationOperations` 直接实现共享写能力
- 在面板类中偷偷直接落盘文件
- 为兼容临时需求重新扩散旧 schema
- 在宿主里写硬编码资源根路径
- 在没有 smoke 的情况下引入新的正式操作面
- 用 GUI 私有状态替代正式工作台状态模型

## 10. 功能接入检查表

新增功能前，按下面顺序检查：

1. 这是宿主交互，还是共享业务能力？
2. 如果是共享业务能力，是否应进入 `ApplicationOperations`？
3. 这是写操作，还是读取操作？
4. 如果是写操作，是否已经只有一套权威入口？
5. 如果是读取操作，是否未来需要被 Headless / 自动化 / Agent 复用？
6. 是否破坏了当前 `ProjectSession / SceneDocument / EditorWorkbenchState` 模型？
7. 是否需要新增或更新 smoke？
8. 是否需要同步文档和模块 Skill？
