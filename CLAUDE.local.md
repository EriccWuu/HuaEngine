# CLAUDE.local.md

全程使用中文交流。

## 项目概述

**HuaEngine** - 一个基于 ECS 架构的 C++20 游戏引擎，使用 CMake 构建，基于 OpenGL/GLFW/GLAD 渲染，并提供 ProjectHub、Editor 与 Headless 应用。技术栈：C++20 / CMake / OpenGL / EnTT / ImGui。
- `HuaEngine/src/HuaEngine/` - 引擎核心模块，包含 Core、ECS、Rendering、Scene、Serialization、Events、GUI 等子系统
- `Editor/` - 编辑器应用，负责基于 ImGui 的工具界面与引擎集成

## Skill 驱动开发（强制）

**每次开发/修复必须遵循此流程：**

```
1. 定位 Skill → 2. 阅读上下文 → 3. 执行任务 → 4. 智能 Sync → 5. 执行反馈
```

### Skill 目录

路径：`.claude/skills/`

### 开发流程

| 步骤 | 操作 | 说明 |
|------|------|------|
| 定位 | 在 `.claude/skills/` 查找 | 按系统名称匹配 |
| 阅读 | 打开 `SKILL.md` | 了解架构、API、常见陷阱 |
| 执行 | 按 Skill 指导完成任务 | - |
| Sync | 智能判断是否更新 | 见下方规则 |
| 反馈 | 基于执行结果进化 Skill | 见下方进化规则 |

### 智能 Sync 规则

代码修改后，基于变更类型判断是否更新 Skill：

| 变更类型 | Sync | 说明 |
|----------|------|------|
| API 签名变化（方法名/参数/返回值） | 必须 | 影响使用方式 |
| 新增公开参数/属性 | 必须 | 新增可配置项 |
| 删除/重命名功能 | 必须 | 影响使用方式 |
| 默认值修改 | 必须 | 影响默认行为 |
| Shader/配置关键字变化 | 必须 | 影响配置方式 |
| Bug 修复（内部逻辑） | 跳过 | 不影响外部接口 |
| 性能优化（无 API 变化） | 跳过 | 不影响使用方式 |
| 注释/格式调整 | 跳过 | 无功能变化 |
| 不确定 | 询问 | 列出变更摘要请用户决策 |

更新 Skill 时：修改 `SKILL.md` + `.evolution/changelog.md`
跳过 Sync 时：如用户明确拒绝同步建议，在 `.evolution/changelog.md` 记录跳过原因：`| {日期} | skip | {跳过原因} |`

### 执行反馈（自动进化）

任务完成后，基于执行效果判断是否进化 Skill：

| 执行结果 | AI 行为 |
|----------|---------|
| 成功，Skill 指导正确 | 无需操作 |
| 成功，但 Skill 缺少关键信息 | 将补充的知识回写到 Skill 对应章节 |
| 走了弯路/Skill 有误导 | 修正 Skill 错误内容，记录到 `.evolution/changelog.md` |
| 发现未覆盖的常见场景 | 补充到 Common Issues 或 Best Practices |
| 用户纠正了匹配/理解 | 更新 description 触发词或 Core Rules |

### Sync 冲突处理

- **冲突检测**：依赖 Git 原生合并冲突机制
- **冲突提示**：提示用户先解决 Git 冲突
- **合并策略**：以最新代码变更为准；矛盾时人工裁决

### 废弃 Skill 检测

当加载标记 `deprecated: true` 的 Skill 时：
1. 提示"此 Skill 已废弃"
2. 读取 `superseded-by` 字段，指向替代 Skill
3. 自动加载替代 Skill 继续任务

### 无对应 Skill 时

- **可独立成 Skill**：调用 `module-skill-creator` 创建新模块 Skill
- **通用/跨系统任务**：归入框架 Skill（如 `huaengine-framework`）
- **Skill 内容过时**：调用 `module-skill-creator` 更新对应模块 Skill

## 全局约束

- 代码规范：遵循 C++20 与仓库既有风格；核心命名空间保持 `HE::`；优先使用 `Ref<T>` / `Scope<T>` 表达所有权；涉及可序列化组件时保持反射声明与序列化链路一致
- 构建约束：默认通过 CMake + Visual Studio 2022 x64 构建；修改核心引擎、ProjectHub 或 Editor 后，优先验证受影响目标可正常编译
- Git 提交格式：`feat/fix/refactor/chore: <summary>`
- 证据驱动：技术方案须有理论依据，不确定时明确说明

## Skill 索引

<!-- SKILL-INDEX-START -->
| Skill | 功能域 | 关联 Skill | 状态 |
|-------|--------|-----------|------|
| huaengine-architecture | 整体架构 / 主工程 | - | active |
| huaengine-core-runtime | Core / Runtime | huaengine-architecture | active |
| huaengine-editor-workbench | Editor / Workbench | huaengine-architecture | active |
| huaengine-rendering | 渲染模块 | huaengine-architecture | active |
| huaengine-ecs-scene | ECS / Scene | huaengine-architecture | active |
| huaengine-serialization-reflection | 序列化 / 反射 | huaengine-architecture | active |
<!-- SKILL-INDEX-END -->
