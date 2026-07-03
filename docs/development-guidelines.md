# HuaEngine 开发规范

## 1. 目的

本文档用于约束 HuaEngine 后续开发方式。后续所有新功能、重构、Bug 修复、工具接入，都应优先遵循本文档，而不是按单点需求临时扩散实现。

本文档基于以下现有架构设计整理：

- [refactored-engine-architecture.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/refactored-engine-architecture.md)
- [editor-project-workbench.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/editor-project-workbench.md)
- [huaengine-cli.md](/D:/Workspace/VS%20Workspace/HuaEngine/docs/huaengine-cli.md)

## 2. 适用范围

本规范适用于：

- `HuaEngine/` 核心引擎库
- `Editor/` 项目工作台
- `ProjectHub/` 项目启动器
- `CLI/` CLI 与自动化宿主
- `Tests/` smoke 与回归验证
- `Resources/` 引擎开发态共享资源

## 3. 核心原则

### 3.1 宿主要薄

宿主只负责自己的交互形式，不负责发明业务能力。

- `ProjectHub.exe` 负责无项目入口、项目选择、项目创建、拉起 `Editor.exe`
- `Editor.exe` 负责项目绑定的 GUI 工作台
- `HuaEngineCLI.exe` 负责 CLI / 自动化 / Agent 调用

禁止在宿主里直接复制一套项目、场景、资产、脚本、验证逻辑。

### 3.2 正式能力必须有唯一权威入口

所有共享业务能力都必须只有一套正式入口，当前权威控制面为：

- `Application`
- `ApplicationOperations`
- `OperationRegistry`
- `ResultEnvelope`

凡是可能被 `Editor`、`CLI`、后续 Agent 宿主共同消费的能力，都必须优先进入正式操作面，再由不同宿主调用。

### 3.3 写操作必须统一

所有会改变持久化状态、运行时场景状态、项目状态的操作，都必须统一走正式操作面，不能在不同宿主各写一套。

当前这一原则已经适用于：

- 项目创建 / 打开 / 状态检查
- 场景创建 / 加载 / 保存 / 校验
- 实体创建 / 删除
- 组件添加 / 删除
- 资产登记 / 校验
- 脚本状态 / 初始化 / 更新 / 关闭
- 聚合验证

后续新增写操作时，也必须遵循同一原则。

### 3.4 读取型能力分层处理

读取型能力不要求一刀切全部命令化，但必须区分两类：

- `共享查询能力`
  - 会被 GUI、CLI、自动化、Agent 共同消费
  - 应逐步沉淀为正式 query facade
- `Editor 实时面板读取`
  - 仅用于 GUI 每帧渲染
  - 可以直接读取当前运行时 `Scene` / `EntityManager` / `entt::registry`

当前阶段的明确约束是：

- `Hierarchy`、`Inspector` 这类每帧 UI 渲染，允许在仅只读的 GUI 渲染路径内直接读运行时场景
- 但不能把共享写能力继续藏在这些面板内部

### 3.5 GUI 与 CLI 必须对齐结果语义

共享操作必须统一返回：

- `ResultEnvelope`
- `Success / Failure / ManualInterventionRequired`
- 稳定的 `operation / target / summary / payload / details`

禁止 GUI 自己定义一套成功失败语义，CLI 再定义另一套。

## 4. 分层约束

### 4.1 Host Layer

宿主层只做以下事情：

- 接收输入
- 管理窗口、命令行、菜单、快捷键、面板等宿主交互
- 调用正式操作面
- 消费返回结果并展示

宿主层禁止：

- 直接绕过 `ApplicationOperations` 操作项目、场景、资产、脚本
- 在宿主内维护平行业务状态源
- 在宿主层复制领域校验逻辑

### 4.2 Control Layer

控制层是共享业务控制面，当前由以下部分组成：

- `Application`
- `ApplicationOperations`
- `OperationRegistry`
- `ResultEnvelope`

控制层职责：

- 统一宿主调用入口
- 统一操作命名
- 统一结果协议
- 编排对领域服务的调用

控制层禁止：

- 承担底层渲染实现细节
- 直接承载面板 UI 逻辑
- 随意持有宿主私有状态

### 4.3 Domain Layer

领域层当前包括：

- `ProjectService`
- `SceneService`
- `AssetService`
- `ScriptService`
- `ValidationService`

领域层职责：

- 提供稳定的业务规则与生命周期
- 提供清晰的输入输出契约
- 不感知具体宿主是 GUI 还是 CLI

领域层禁止：

- 依赖 ImGui
- 依赖具体面板类
- 假设调用方一定来自 `Editor`

### 4.4 Engine Core Layer

引擎核心层包括：

- ECS
- Scene
- Serialization
- Reflection
- Rendering
- OpenGL backend
- Window / Event / ImGui 集成底座

核心层职责是提供底层实现，不直接承担产品工作流编排。

## 5. 宿主职责规范

### 5.1 ProjectHub

`ProjectHub` 是独立启动器，不是 `Editor` 的一个面板模式。

要求：

- 无项目入口优先进入 `ProjectHub`
- `ProjectHub` 负责最近项目、创建项目、打开项目、启动 `Editor`
- `ProjectHub` 与 `Editor` 通过命令行契约衔接，而不是直接共享 UI 状态

### 5.2 Editor

`Editor` 是项目绑定工作台。

要求：

- 启动后应尽快落到明确的 `ProjectSession + SceneDocument` 状态
- 所有项目级、场景级写操作必须走正式操作面
- 面板层负责交互，不直接扮演领域服务

### 5.3 CLI

`CLI` 是正式自动化入口。

要求：

- CLI 命令必须只消费正式操作面
- stdout 应保持机器可读 JSON
- exit code 必须与 `ResultEnvelope` 语义一致

## 6. 编辑器工作台规范

### 6.1 状态模型

`Editor` 当前工作台的现阶段参考状态模型是：

- `ProjectSession`
- `SceneDocument`
- `EditorWorkbenchState`

后续新增面板、工具、交互功能时，应优先对接这套状态模型，而不是再创建新的全局状态副本。

这里的“参考模型”表示当前正式约定的主线，而不是冻结未来重构空间。后续如果确有明确收益，可以重构具体结构名和组织方式，但必须同时满足以下条件：

- 不破坏宿主薄化原则
- 不重新引入平行状态源
- 不削弱正式操作面的唯一权威入口
- 同步更新相关文档、Skill 与 smoke

### 6.2 面板职责

当前基础面板职责如下：

- `Project`
  - 显示项目摘要、场景摘要、目录摘要、轻量导航
- `Hierarchy`
  - 显示实体树与选择关系
- `Inspector`
  - 显示并编辑当前选择
- `Scene`
  - 场景视口
- `Console`
  - 运行日志与诊断

要求：

- 面板只负责展示和交互
- 需要改状态时，调用正式操作或编辑器交互核心
- 不在面板里偷偷改项目/场景底层状态

### 6.3 交互核心

编辑器交互当前的现阶段参考接入点为：

- `EditorInteractionHost`
- `EditorCommandStack`
- `ContextMenuRegistry`
- `ShortcutRegistry`
- `DragDropIntentRegistry`

要求：

- `Undo / Redo` 的写操作必须命令化
- 右键菜单必须通过注册面接入
- 快捷键必须通过注册面接入
- 拖拽意图必须通过注册面接入

禁止：

- 直接在面板渲染逻辑里散写一堆互不对齐的快捷键判断
- 为单个面板私自绕开命令栈做不可撤销写操作

## 7. 场景、实体、组件规范

### 7.1 实体与组件写操作

后续所有实体和组件写操作都必须遵循：

- 能进入 `ApplicationOperations` 的，必须先进入 `ApplicationOperations`
- `Editor` 命令层只做调用与撤销编排，不拥有另一套写语义
- `CLI` 只能调用正式操作面，不允许自己补一套场景改写逻辑

### 7.2 组件设计

默认规则：

- 一个实体上，同类型组件默认只允许一份

优先方案：

- 多个同类数据，优先用 `SetComponent`
- 多个同类对象，优先拆成子实体

不建议当前阶段引入“一个实体多个同类型组件”的通用机制，除非需求明确且收益显著。

### 7.3 组件归属

组件定义不应继续无限堆到单一 `Components.h`。

规则：

- 基础组件放核心层
- 模块专属组件放各自模块
- 序列化、Inspector、验证通过注册表或显式接线聚合

## 8. 序列化与 Schema 规范

### 8.1 风格要求

序列化风格应接近现代引擎，而不是无意义地为静态字段重复写 `type`。

要求：

- 对象级别保留必要类型信息
- 字段级别仅在动态类型场景下保留类型标签
- 静态字段类型由代码、反射、schema 决定，不冗余落盘

### 8.2 当前正式约束

当前正式 schema 已经收紧，新增逻辑必须与现有约束一致：

- 项目元数据使用 `schema_version`
- 材质根级类型使用 `material_type`
- 场景组件使用更直接的对象结构
- 旧格式已不再作为正式兼容目标

禁止：

- 为了省事重新引入旧字段兼容分支
- 在新格式里重新塞回历史遗留包装结构

### 8.3 删除必须真实删除

序列化输出必须只反映 live runtime state。

要求：

- 删除实体后保存，文件中不得残留空壳实体
- 不得把 tombstone / 已无效句柄重新写回场景文件

## 9. 资源与路径规范

### 9.1 引擎资源

开发态共享资源放在：

- `Resources/`

构建产物资源放在：

- `${output_root}/Resources`

禁止继续以历史 `assets/` 作为新的正式资源根。

### 9.2 路径解析

运行时路径解析统一通过：

- `ResourcePaths`

要求：

- shader、texture、默认材质、默认 mesh 等引擎共享资源都走统一资源解析
- 不在单个模块里写硬编码相对路径

### 9.3 项目资源与引擎资源分离

必须区分：

- 引擎资源
- 项目资源

原则：

- 引擎必须可提供自己的默认资源
- 项目只保存项目自己的资源和显式依赖
- 不要把“引擎必须存在的默认资源”只放在某个具体工程目录里

## 10. 查询面与读取边界规范

当前阶段的正式决定如下：

- 写操作统一
- 共享查询逐步统一
- Editor 实时读取暂不强制命令化

因此：

- `Hierarchy`、`Inspector` 当前直接读 runtime scene 是允许的
- 但如果一个读取能力未来需要被 `CLI`、自动化、Agent 复用，应优先补成正式 query facade

判断标准：

- 只服务 GUI 每帧渲染：可直接读 runtime
- 要对外复用、要做自动化、要做 CLI：应补正式 query 接口

## 11. 测试与验证规范

### 11.1 新增正式能力必须补 smoke

凡是新增正式操作面、正式宿主行为、正式 schema、正式工作流，都必须补 smoke 或回归验证。

优先覆盖：

- `ApplicationOperations` 级别 smoke
- `CLI` 命令工作流 smoke
- `Editor` 交互核心 smoke

### 11.2 验证层次

每次重要改动至少覆盖两层：

- 编译通过
- 对应 smoke / 回归通过

如果是 GUI 行为变更，建议额外做一次人工运行验证。

### 11.3 文档同步

凡是影响架构、宿主职责、正式操作面、工作流入口的改动，都必须同步更新相关文档。

至少检查：

- `docs/refactored-engine-architecture.md`
- `docs/editor-project-workbench.md`
- `docs/huaengine-cli.md`
- 对应模块 Skill

## 12. 禁止事项

后续开发中，以下行为默认禁止：

- 在 `Editor` 和 `CLI` 中各自实现一套相同写逻辑
- 绕过 `ApplicationOperations` 直接实现共享写能力
- 在面板类中偷偷直接落盘文件
- 为兼容临时需求重新扩散旧 schema
- 在宿主里写硬编码资源根路径
- 在没有 smoke 的情况下引入新的正式操作面
- 用 GUI 私有状态替代正式工作台状态模型

## 13. 新功能接入检查表

新增功能前，先按下面顺序检查：

1. 这是宿主交互，还是共享业务能力？
2. 如果是共享业务能力，是否应进入 `ApplicationOperations`？
3. 这是写操作，还是读取操作？
4. 如果是写操作，是否已经只有一套权威入口？
5. 如果是读取操作，是否未来需要被 CLI / 自动化 / Agent 复用？
6. 是否破坏了当前 `ProjectSession / SceneDocument / EditorWorkbenchState` 模型？
7. 是否需要新增或更新 smoke？
8. 是否需要同步文档和模块 Skill？

## 14. 当前阶段的开发结论

后续开发应优先沿这条主线推进：

- 保持宿主薄化
- 继续强化正式操作面
- 不让 GUI 和 CLI 再次分叉
- 保持工作台状态模型统一
- 保持序列化 schema 收紧
- 保持资源根统一
- 保持测试与文档同步

这份规范的目标不是限制开发速度，而是避免引擎在继续扩展时重新长回“多个入口、多个状态源、多个半正式实现”的旧形态。
