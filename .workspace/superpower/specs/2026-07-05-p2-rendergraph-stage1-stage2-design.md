# P2 RenderGraph Stage 1+2 设计方案

## 背景

P2 渲染架构主线的目标是把当前直接调用式 forward 渲染链路逐步演进为 RenderGraph + RHI 架构。现有工作区已经具备 Stage 0 基线：`PassGraph` 可以按顺序执行 pass，`ForwardRenderPipeline` 已经把 `ForwardOpaque` 包进 graph 执行路径，`RenderPassGraphSmoke` 能验证空 graph、重复 pass、缺少回调和稳定执行顺序。

当前 `PassGraph` 已有 `Inputs` / `Outputs` 字段，但这些字段还只是描述数据，没有参与资源依赖校验、统计或 operation 输出。下一步应进入 Stage 1 和 Stage 2：让 graph 具备逻辑资源语义，并让 graph 生命周期成为渲染管线的一部分。

## 范围

本轮包含：

- Stage 1：逻辑资源声明、资源依赖校验、graph 资源统计。
- Stage 2：`ForwardRenderPipeline` 持有并编译 graph，整理 graph 执行上下文边界。
- 渲染 operation 输出 graph 诊断和资源统计，便于 CLI、测试和自动化观察。

本轮不包含：

- 不引入 RHI。
- 不创建或管理真实 GPU 资源。
- 不做 pass 自动排序。
- 不把清屏、framebuffer bind/unbind、overlay 等 frame lifecycle 操作迁入 graph。
- 不重命名 `PassGraph` 为 `RenderGraph`，避免扩大无关改动。

## 推荐方案

采用“逻辑资源 + pipeline 持有 graph + 诊断输出”的方案。

`PassGraph` 保留当前命名，作为 RenderGraph v0 的落地点。它负责维护 pass 列表、编译期校验逻辑资源关系、执行已编译的顺序 graph。`ForwardRenderPipeline` 从每帧临时构造 graph 改为持有固定 graph 描述，在首次渲染或 dirty 时重建并编译。

当前 OpenGL 执行路径保持不变。`Renderer::Begin`、`Renderer::Submit`、`Renderer::End` 仍按现有方式执行，避免在 Stage 1/2 过早触碰 RHI 和底层渲染对象。

## 架构设计

### PassGraph 职责

`PassGraph` 继续承担 RenderGraph v0 的核心职责：

- 接收 `RenderPassDesc`。
- 校验 pass 名称、执行回调和逻辑资源声明。
- 记录编译诊断。
- 生成 graph 资源统计。
- 按声明顺序执行 pass。

`RenderPassDesc` 现有 `Inputs` 和 `Outputs` 字段继续保留。编译时把 `Inputs` 视为资源读取，把 `Outputs` 视为资源写入。这样能减少调用点改动，同时让资源语义从 Stage 1 开始生效。

### 逻辑资源模型

本轮只表达逻辑资源，不映射到真实 GPU 对象。建议使用轻量资源标识：

- `CameraView`：当前相机视图。
- `SceneItems`：场景提取后的 render item 集合。
- `SceneColor`：forward pass 的颜色输出。

`ForwardOpaque` pass 的资源声明为：

- 读取：`CameraView`、`SceneItems`。
- 写入：`SceneColor`。

`CameraView` 和 `SceneItems` 作为 graph 外部输入声明，`SceneColor` 作为 graph 输出统计。

### 编译校验规则

`PassGraph::Compile()` 应覆盖以下规则：

- graph 至少包含一个 pass。
- pass 名称不能为空。
- pass 名称不能重复。
- pass 必须提供 execute callback。
- `Inputs` / `Outputs` 中的资源名不能为空。
- 同一个 pass 内不能重复声明同一资源。
- pass 读取的资源必须由之前 pass 写出，或由 graph 声明为外部输入。
- 同一个逻辑资源不能被多个 pass 写入。

本轮不做自动排序，所以“之前 pass 写出”按声明顺序判断。资源多写先视为错误，不支持 alias、overwrite 或 transient resource。

### 生命周期

`ForwardRenderPipeline` 增加 graph 成员，例如 `PassGraph m_Graph`，并增加初始化或 dirty 标记。建议实现 `EnsureGraphCompiled()`：

1. 如果 graph 已经编译且未 dirty，直接复用。
2. 如果需要重建，清空 graph。
3. 声明外部输入 `CameraView`、`SceneItems`。
4. 添加 `ForwardOpaque` pass。
5. 编译 graph。
6. 把编译诊断和资源统计写入本次 `RenderResult`。

编译失败时，`RenderResult.Succeeded = false`，不进入 `Renderer::Begin()`。这样 graph 结构错误不会触发底层渲染执行。

### 执行上下文

短期内保留 `RenderPassContext` 作为 pass callback 的参数，避免一次性改动所有 pass。它继续承载：

- `View`
- `RenderItems`
- `ResourceResolver`
- `Stats`
- `Diagnostics`

同时引入 graph 运行时上下文或 graph 统计结构，用于承载：

- graph 编译诊断。
- graph 资源数量。
- graph 资源边数量。
- graph 外部输入数量。
- graph 输出数量。

本轮不把 RHI command list 放入上下文，但上下文边界应避免继续把所有渲染状态塞进 `RenderPassContext`，为后续 RHI 接入保留位置。

## Operation 输出

`RenderResult` 增加 graph 相关结果字段：

- `GraphDiagnostics`
- `GraphResourceCount`
- `GraphEdgeCount`
- `GraphExternalInputCount`
- `GraphOutputCount`

`ApplicationOperations::RenderSceneViewport` 成功时新增 payload：

- `graph_resources`
- `graph_edges`
- `graph_external_inputs`
- `graph_outputs`
- `graph_diagnostics`

如果 graph 编译或执行失败，operation 仍通过 `rendering.render_scene_viewport.pipeline_failed` 失败，但应把 graph 诊断转成 `ResultEnvelope::Details`。诊断 code 建议使用 `rendering.graph.*` 前缀，例如：

- `rendering.graph.empty_resource`
- `rendering.graph.duplicate_resource_access`
- `rendering.graph.missing_resource_producer`
- `rendering.graph.duplicate_resource_writer`

payload 保持计数字段，不承载大量明细；明细继续走 `ResultEnvelope::Details`。

## 测试设计

### RenderPassGraphSmoke

扩展 smoke 覆盖：

- 外部输入可满足读取依赖。
- 读取未声明且无生产者的资源会编译失败。
- 多个 pass 写同一资源会编译失败。
- 空资源名会编译失败。
- 同一 pass 内重复资源访问会编译失败。
- 有效 graph 的资源统计符合预期。

### RenderingOperationsSmoke

扩展现有 operation smoke，保持已有统计口径不变：

- `pass_count` 仍为 `1`。
- `submitted_items`、`skipped_items`、`draw_calls`、`fallback_items` 语义不变。
- 新增 payload 字段存在。
- 当前单 pass forward graph 的预期统计为：
  - `graph_resources = 3`
  - `graph_edges = 3`
  - `graph_external_inputs = 2`
  - `graph_outputs = 1`
  - `graph_diagnostics = 0`

## 验收标准

- `PassGraph` 能表达并校验 pass 的逻辑输入、输出和外部输入。
- `ForwardRenderPipeline` 不再每帧临时构造 graph，而是持有并复用已编译 graph。
- graph 编译失败不会进入底层渲染执行。
- `rendering.render_scene_viewport` 输出 graph 资源统计和 graph 诊断计数。
- 现有渲染行为和统计口径保持稳定。
- `RenderPassGraphSmoke` 和 `RenderingOperationsSmoke` 通过。

## 风险与约束

- 不应把 Stage 3 的 frame lifecycle 提前混入本轮，否则清屏、目标绑定和 overlay 的统计口径会被提前改变。
- 不应把 `PassGraph` 与 RHI 绑定，否则 Stage 1/2 会被底层 API 抽象拖大。
- 不应把材质、mesh、shader、asset resolver 边界一起重构；本轮只整理 graph 层语义和 pipeline 生命周期。
- 文档主体使用中文；代码注释继续使用英文。

## 下一步

设计确认后，进入 implementation plan。计划应拆成小步：先补 `PassGraph` 资源模型和 smoke，再调整 `ForwardRenderPipeline` 生命周期，最后接入 `RenderResult` 和 `ApplicationOperations` 的可观测输出。
