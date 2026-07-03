# HuaEngine 功能路线优先级

## 当前判断

CLI 已经有一部分基础，不再按“从零建设”的大功能看待。当前代码里已经有 `HuaEngineCLI`、`HE::CLI::CommandRunner`、JSON 输出、命令行 smoke 测试，并且命令覆盖了 `project`、`scene`、`asset`、`script`、`validation` 等基础工作流。

因此 CLI 仍然排第一，但目标应调整为“补齐和固化自动化入口”，优先把它做成稳定的操作引擎，再让后续工具链都能通过 CLI 被调用、验证和回归测试。

## 排序原则

1. 先收口 CLI，让引擎具备稳定的自动化入口。
2. 再做 Reflection Tool，优先解决后续序列化、编辑器、脚本绑定和资产工具都会依赖的类型信息问题。
3. 资产管理和序列化格式属于数据底座，应在大量内容和工具继续堆叠前稳定下来。
4. RenderGraph + RHI 属于渲染架构主线，放在数据和工具链基础之后集中推进。
5. 工程打包、Lua、Gizmos 等属于扩展能力或内容生产力，整体后置。

## P0：自动化入口收口

### 1. CLI 操作引擎补齐

CLI 已有基础，优先级仍然最高，但工作量应收敛为补齐缺口和固化契约。

建议目标：

- 梳理命令注册、帮助输出和命令命名规范。
- 固化 JSON 输出、错误码和诊断信息格式。
- 让 operation registry 和 CLI command 的映射更清晰。
- 扩展常用工程、场景、资产、验证命令的 smoke 覆盖。
- 为后续 Reflection Tool、资产工具、序列化迁移、打包流程预留统一入口。

## P1：反射与数据底座

### 2. Reflection Tool

反射代码自动生成工具排在 CLI 之后。它会影响组件注册、序列化、Editor Inspector、脚本绑定和资产工具，是后续很多能力的共同前置。

建议分两步：

- Python MVP：快速验证扫描标记、生成规则、构建接入和最小可用产物。
- C# 正式版：在规则稳定后再做更强解析、增量生成、IDE/构建集成和长期维护版本。

### 3. 资产管理

资产管理排在序列化后端修改之前。先明确资产身份和工程内组织方式，再改底层持久化格式，返工风险更低。

建议目标：

- 明确 Asset ID、路径、registry 和资源引用规则。
- 建立资产验证、缺失引用诊断和基础导入/注册流程。
- 接入 CLI，使资产扫描、验证和修复可以自动化执行。
- 为工程打包和内容生产流程提供稳定输入。

### 4. 序列化后端改为 YAML

序列化格式会影响场景、材质、工程配置、资产索引和后续脚本绑定，适合在资产规则明确后推进。

建议目标：

- 明确 Scene / Material / Project / Asset registry 的 YAML schema。
- 保留 JSON 到 YAML 的迁移工具或一次性转换命令。
- 通过 CLI 提供格式验证和迁移入口。
- 加 smoke 测试覆盖读写往返、旧格式迁移和错误诊断。

## P2：渲染架构主线

### 5. RenderGraph + RHI

RenderGraph 和 RHI 建议作为连续阶段推进。先用当前 OpenGL 后端承载 RenderGraph 或轻量 pass graph，再逐步抽出 RHI，可以减少一次性替换底层渲染对象的风险。

建议顺序：

- RenderGraph / PassGraph：RenderPass 描述、资源依赖、attachment、diagnostics、stats。
- OpenGL 后端适配：保持现有功能可运行，避免同时改太多底层对象。
- RHI：RenderDevice、CommandList、Buffer、Texture、ShaderProgram、PipelineState、RenderTarget、Swapchain。

## P3：扩展能力和内容生产力

这些能力整体后置。它们有价值，但更依赖前面的 CLI、反射、资产、序列化和渲染边界稳定。

### 6. 工程打包

工程打包依赖 CLI、资产管理、资源路径、序列化格式和构建配置。建议等资产和 YAML 稳定后再做。

建议目标：

- CLI 一键 package。
- 资源收集、路径重写和缺失资源诊断。
- Debug / Release / Dist 配置区分。
- 最小可运行包验证。

### 7. Lua 接入

Lua 属于运行时扩展能力，应在反射、组件注册、场景格式和资产规则稳定后推进。

建议目标：

- Lua VM 生命周期。
- Scene / Entity / Component 基础绑定。
- 脚本组件进入 scene serialization。
- CLI smoke 验证脚本初始化、更新和关闭。

### 8. Gizmos

Gizmos 属于编辑器和内容生产力能力，放到最后一组。等 Transform、Selection、Editor Camera、渲染 overlay 和 undo/redo 边界更清楚后推进更稳。

建议目标：

- Transform translate gizmo。
- selection 绑定。
- undo/redo 预留。
- 后续再扩展 rotate / scale。

## 总体顺序

1. CLI 操作引擎补齐
2. Reflection Tool
3. 资产管理
4. 序列化后端改为 YAML
5. RenderGraph + RHI
6. 工程打包
7. Lua 接入
8. Gizmos
