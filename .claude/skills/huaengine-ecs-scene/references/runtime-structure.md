# 运行时结构

## 1. raw Scene 层与正式操作层是两层语义

当前场景行为分成两层：

- 原始运行时层
  - `EntityManager`
  - `Entity`
  - `Scene`
  - `System`
- 正式共享操作层
  - `SceneService`
  - `ScriptService`
  - `ApplicationOperations`

排查问题时，先判断它属于哪一层：

- 原始 registry / 运行时所有权问题
- 正式宿主可消费的修改 / 校验问题

如果一个能力将来要被 `Editor`、`Headless` 和自动化共同消费，优先从 `ApplicationOperations` 往下找，而不是从面板代码往里追。

## 2. EntityManager 与 Entity 仍是薄封装

关键文件：

- `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- `HuaEngine/src/HuaEngine/ECS/Entity.h`

当前设计依然很薄：

- `EntityManager` 拥有唯一 `entt::registry`
- `CreateEntity()` 直接调 `registry.create()`
- 创建后默认加 `TransformComponent`
- `DestroyEntity()` 直接销毁 raw entity handle
- `Entity` 本质仍是对 registry 的语法糖

不要把这一层和正式宿主场景编辑层混为一谈。

## 3. 共享写操作已经统一

凡是会改真实项目 / 场景状态的场景修改，现在都应该被看成共享操作。

当前典型例子：

- 创建实体
- 删除实体
- 添加组件
- 删除组件
- 保存场景

这里最重要的架构事实是：

- `Editor` 命令通过 `ApplicationOperations` 执行这些修改
- `Headless` CLI 走的是同一套底层正式操作

因此这些写逻辑不应该再各自散落在：

- `HierarchyPanel`
- `InspectorPanel`
- `HeadlessCommandRunner`

## 4. GUI 读取仍可直接读运行时状态

当前保留的例外是：

- `HierarchyPanel` 直接枚举运行时实体
- `InspectorPanel` 直接读取运行时组件

这对每帧 GUI 渲染和轻量编辑器观察是允许的。

但这不代表可复用共享查询也应该继续直接长在 panel 里。

判断规则：

- 只服务 GUI 每帧渲染：可以直接读运行时场景
- 要给 CLI / 自动化 / Agent 复用：后续应收敛到正式查询面

## 5. 脚本运行时消费

脚本定义仍然放在 ECS 面向的类型里：

- `NativeScriptComponent`
- `ScriptableEntity`

但正式生命周期控制现在放在：

- `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`

当前应这样理解：

- 脚本组件定义仍属于 ECS 状态
- 脚本生命周期推进属于 `ScriptService`
- `Headless` 的脚本命令和更上层宿主都不应绕过这个 service

## 6. 场景序列化期望

关键文件：

- `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- `HuaEngine/src/HuaEngine/Scene/SceneService.cpp`

当前正式期望：

- 保存结果只反映 live 场景状态
- 删除实体后不能留下空壳
- tombstone entity id 绝不能再写回场景文件
- 旧场景结构已经不是正式目标

如果删除后的实体仍出现在场景文件里，优先怀疑序列化枚举逻辑。

## 7. 当前 GUI 消费路径

最直接的 GUI 消费点：

- `Editor/src/Panels/HierarchyPanel.cpp`
- `Editor/src/Panels/InspectorPanel.cpp`

当前行为：

- `Hierarchy` 通过 `view<TransformComponent>()` 枚举实体
- `Inspector` 直接读取主选中实体和组件事实
- Editor 摘要和最近结果来自 `EditorWorkbenchState`
- 真正的共享场景写操作通过交互核心和正式操作层完成

这套拆分是刻意的：

- 直接读取，用于面板渲染
- 统一写入，用于修改场景状态

## 8. 当前 Headless 消费路径

当前 headless 场景侧命令包括：

- `scene create`
- `scene validate`
- `scene entity create`
- `scene entity delete`
- `scene component add`
- `scene component remove`
- `script status`
- `script initialize`
- `script update`
- `script shutdown`

它们都通过：

- `HeadlessCommandRunner`
- `ApplicationOperations`
- `SceneService / ScriptService`

如果一个新能力只加在 raw `Scene` helper 里，而没有暴露到正式层，headless 不会自动得到它。

## 相关 Skill

- 看 render-facing 组件是怎么进入渲染消费链的：转 `huaengine-rendering/references/runtime-flow.md`
- 看序列化和反射字段细节：转 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 看宿主 / 控制层边界：转 `huaengine-architecture/references/architecture.md`
