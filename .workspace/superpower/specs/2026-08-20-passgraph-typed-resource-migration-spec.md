# PassGraph 纯 Typed Resource Migration Spec

状态：已完成
日期：2026-08-20

## 目标

移除 `PassGraphPassDesc` 的字符串 `Inputs`/`Outputs` 及过渡型 typed 输入/输出列表，使图中的 GPU 资源依赖只由 `RenderGraphResourceHandle + ResourceAccess + ResourceState` 表达。

## 设计

1. `PassGraphResourceUsage` 新增显式 `Read`/`Write` access；resource state 仅描述使用状态，不再隐式推断 producer/consumer。
2. `PassGraph::AddPass()` 返回 `PassGraphPassHandle`；Pass 可通过 `Dependencies` 表达没有 GPU resource 的执行顺序。
3. imported resource 是 typed 的图外输入；删除 `AddExternalInput()`、`m_ExternalInputs`、`ExternalInputCount` 和相关 diagnostics。
4. render-pass attachment 在编译时自动转换为 typed write usage：color 为 `RenderTarget`，depth/stencil 为 `DepthStencilWrite`。
5. `ForwardRenderPipeline` 的 `BeginFrame/EndFrame` 移到 graph execute 前后；Forward/PostProcess 只通过 SceneColor、SceneDepth、ViewportColor 的 typed usage 建立依赖。
6. compiler 以 access 建 producer/consumer 边、以 state 建 barrier；保留 cycle、duplicate writer、invalid handle、invalid usage、culling、queue batch 和 lifetime 语义。

## 非目标

- 不实现 DX12 后端。
- 不扩展实际 compute/copy 命令回放。
- 不改变 RHI 的 `ResourceBarrier` 后端映射。

## 验收

- 生产代码与 smoke 中不再使用 `Inputs`、`Outputs`、`InputResources`、`OutputResources`、`AddExternalInput` 或 `GetExternalInputs`。
- `RenderPassGraphSmoke` 覆盖 typed future producer、cycle、queue batch、culling、barrier 和 explicit dependency。
- `RHIResourceCreationSmoke`、`RHICommandListBindingSmoke`、`RenderingOperationsSmoke` 通过。

## 实现结果

- `PassGraphResourceUsage` 现在显式携带 `Read`/`Write` access 与 `ResourceState`，不再根据 state 猜测 producer/consumer。
- `AddPass()` 返回 `PassGraphPassHandle`；无 GPU resource 的顺序约束通过 `Dependencies` 表达。
- imported resource 成为唯一的图外资源来源；移除了 external input API、统计数据和 operation payload。
- Forward 图只声明真实的 color/depth/post-process resource usage；帧开始/结束移至图执行外层。
- legacy 字符串资源接口和已隔离的旧编译路径已物理删除；四个 RHI/rendering smoke 全部通过。
