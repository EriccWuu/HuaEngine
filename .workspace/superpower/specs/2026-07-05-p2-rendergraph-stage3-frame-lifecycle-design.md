# P2 RenderGraph Stage 3 Frame Lifecycle 设计方案

## 背景

Stage 1+2 已经让 `PassGraph` 具备逻辑资源依赖校验、资源统计、pipeline 持有 graph、operation 可观测输出。当前 `ForwardRenderPipeline::Render()` 中仍有一组 frame 级动作散落在 graph 外部：framebuffer bind/unbind、clear、`Renderer::Begin()`、`Renderer::End()`。

Stage 3 的目标是把这些 frame lifecycle 动作纳入 graph，使一帧的主要渲染步骤能从 pass 列表直接看出来，并为后续 overlay、gizmo、post process、debug draw 等 pass 提供稳定插入点。

## 范围

本轮包含：

- 将 target bind/unbind、clear、renderer begin/end 图化为 pass。
- `ForwardOpaque` 继续作为 graph 中的绘制 pass。
- clear、begin、end、bind、unbind 都计入 `RenderStats.PassCount`。
- 更新 graph 资源依赖和 operation 统计口径。
- 保持现有 OpenGL 执行路径和 framebuffer 渲染行为可用。

本轮不包含：

- 不引入 RHI。
- 不引入 command list。
- 不改变 `FrameBuffer`、`RenderCommand`、`Renderer` 底层 API。
- 不实现 overlay、gizmo、post process 或 debug draw。
- 不把 Editor ImGui viewport 绘制纳入 graph；Editor 仍只显示 framebuffer color attachment。
- 不改变资产、材质、mesh、shader 解析路径。

## 推荐方案

采用“完整 frame lifecycle 图化，但复用现有 callback 执行路径”的方案。

`ForwardRenderPipeline` 的 graph pass 顺序固定为：

1. `BindTarget`
2. `ClearTarget`
3. `BeginRenderer`
4. `ForwardOpaque`
5. `EndRenderer`
6. `UnbindTarget`

这些 pass 都使用现有 `RenderPassContext` 执行，不引入新的 command abstraction。这样能让 frame 主路径图化，同时避免 Stage 3 过早耦合 Stage 5 RHI。

## Pass 设计

### BindTarget

职责：绑定当前 `RenderView::Target`。

资源：

- 读取：`RenderTarget`
- 写入：`BoundRenderTarget`

执行：

- 检查 `context.View` 和 `context.View->Target`。
- 调用 `context.View->Target->Bind()`。
- 计入 `PassCount`。

### ClearTarget

职责：根据 `RenderView::ClearColorBuffer` 清理当前 target。

资源：

- 读取：`BoundRenderTarget`
- 写入：`ClearedSceneColor`

执行：

- 检查 `context.View`。
- 如果 `ClearColorBuffer == true`，调用 `RenderCommand::SetClearColor()` 和 `RenderCommand::Clear()`。
- 如果 `ClearColorBuffer == false`，pass 仍执行并计数，但不调用 clear。
- 计入 `PassCount`。

### BeginRenderer

职责：开启现有 `Renderer` frame。

资源：

- 读取：`CameraView`
- 写入：`RendererFrame`

执行：

- 检查 `context.View` 和 `context.View->CameraRef`。
- 调用 `Renderer::Begin(context.View->CameraRef)`。
- 计入 `PassCount`。

### ForwardOpaque

职责：执行现有 forward opaque render item 提交。

资源：

- 读取：`CameraView`
- 读取：`SceneItems`
- 读取：`RendererFrame`
- 读取：`ClearedSceneColor`
- 写入：`SceneColor`

执行：

- 保留当前 `ForwardOpaquePass::Execute()` 中的 resource resolve、fallback、submit、draw call 统计逻辑。
- 计入 `PassCount`。

### EndRenderer

职责：结束现有 `Renderer` frame。

资源：

- 读取：`RendererFrame`

执行：

- 调用 `Renderer::End()`。
- 计入 `PassCount`。

### UnbindTarget

职责：解绑当前 framebuffer target。

资源：

- 读取：`BoundRenderTarget`

执行：

- 检查 `context.View` 和 `context.View->Target`。
- 调用 `context.View->Target->Unbind()`。
- 计入 `PassCount`。

## 资源依赖

外部输入：

- `RenderTarget`
- `CameraView`
- `SceneItems`

内部资源：

- `BoundRenderTarget`
- `ClearedSceneColor`
- `RendererFrame`

图输出：

- `SceneColor`

依赖链路：

```text
RenderTarget -> BindTarget -> BoundRenderTarget -> ClearTarget -> ClearedSceneColor -> ForwardOpaque -> SceneColor
CameraView -> BeginRenderer -> RendererFrame -> ForwardOpaque
RendererFrame -> EndRenderer
BoundRenderTarget -> UnbindTarget
SceneItems -> ForwardOpaque
```

`ClearTarget` 不直接写 `SceneColor`，避免和 `ForwardOpaque` 形成 duplicate writer。它写 `ClearedSceneColor`，由 `ForwardOpaque` 消费并生成最终 `SceneColor`。

## 统计口径

成功渲染时，当前 forward graph 的预期统计为：

- `pass_count = 6`
- `graph_resources = 7`
- `graph_edges = 12`
- `graph_external_inputs = 3`
- `graph_outputs = 1`
- `graph_diagnostics = 0`

`pass_count` 从 Stage 1+2 的 `1` 调整为 `6` 是有意行为，因为 Stage 3 明确要求 frame lifecycle pass 进入统计口径。

`graph_edges` 继续沿用 Stage 1+2 的现有语义：input edges 加 terminal outputs。若后续需要展示完整读写边或调度图，可以在后续阶段新增更精确的统计字段，不在本轮重命名。

## 错误处理

保持当前渲染行为：

- 如果 `RenderView::CameraRef` 缺失，`Render()` 在执行 graph 前返回失败结果。
- 如果 `RenderView::Target` 缺失，`Render()` 在执行 graph 前返回失败结果。
- graph 编译失败时返回失败结果，并保留 graph diagnostics。
- pass 内部仍做空指针防御，但不新增运行时诊断，避免本轮扩大错误模型。

`BindTarget` 成功后，`UnbindTarget` 应作为 graph 最后一个 pass 执行。当前阶段不处理 pass 中途异常或 C++ exception 场景；引擎现有 pass callback 也没有异常恢复契约。

## 测试设计

### RenderingOperationsSmoke

更新成功渲染路径断言：

- `pass_count` 从 `1` 改为 `6`。
- `graph_resources` 从 `3` 改为 `7`。
- `graph_edges` 从 `3` 改为 `12`。
- `graph_external_inputs` 从 `2` 改为 `3`。
- `graph_outputs` 保持 `1`。
- `graph_diagnostics` 保持 `0`。
- `HasRenderedPixel(framebuffer)` 仍必须通过。

### RenderPassGraphSmoke

保留现有底层资源依赖校验测试。可选择增加 frame lifecycle graph 的 focused 用例，验证 6 pass 顺序和资源统计；如果不暴露 `ForwardRenderPipeline` 内部 graph，也可以仅通过 `RenderingOperationsSmoke` 验证对外统计。

## 验收标准

- `ForwardRenderPipeline` 的 graph pass 列表包含完整 frame lifecycle：bind、clear、begin、forward opaque、end、unbind。
- 成功渲染时 `pass_count = 6`。
- 成功渲染时 graph 统计为 `resources=7`、`edges=12`、`external_inputs=3`、`outputs=1`、`diagnostics=0`。
- 现有 framebuffer 渲染仍能写出非 clear pixel。
- `RenderPassGraphSmoke` 和 `RenderingOperationsSmoke` 通过。
- 不引入 RHI、command list 或新的底层渲染 API。

## 风险与约束

- `pass_count` 口径变化会影响依赖该字段的测试或工具链，本轮必须同步更新 smoke。
- `ClearTarget` 在 `ClearColorBuffer == false` 时仍计入 pass；这是为了保持 graph 结构稳定。
- 当前 pass 执行没有异常恢复机制，`BindTarget` 后若中途出现未捕获异常，`UnbindTarget` 不保证执行。这个问题属于后续执行器可靠性设计，不在本轮解决。
- 本轮只图化当前 forward frame；overlay、gizmo、post process 等只保留插入点，不提前实现。

## 下一步

设计确认后进入 implementation plan。计划应先更新 `ForwardRenderPipeline` 的 pass 构建和 pass 类，再更新 smoke 统计口径，最后运行两个 smoke 目标验证。
