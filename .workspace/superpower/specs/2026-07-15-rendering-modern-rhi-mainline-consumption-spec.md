# 渲染现代 RHI 主路径消费与后续演进 Spec

状态：草案  
日期：2026-07-15  
基线提交：`c515015 refactor(rendering): extend modern rhi skeleton`  
范围：P30-P32 之后，推动 rendering 主路径真正消费现代 RHI，并继续收敛底层 RHI 语义边界。

## 1. 背景

当前 RHI 已经不再只是 OpenGL wrapper。到 `c515015` 为止，系统已经具备现代 RHI 的关键骨架：

- `RenderDevice` 统一资源创建入口。
- `CommandList` 具备 render pass、bind group、resource barrier、显式 vertex/index binding。
- `PipelineState` 能声明 bind group layout contract。
- `CommandBuffer` / `RenderQueue` 已有最小 API 形状。
- `PassGraph` 能表达 pass 输入输出、资源 lifetime，并生成最小 barrier plan。
- OpenGL backend 已作为第一后端承载这些接口。

但当前 rendering 主路径仍有明显过渡层：

- Forward pass 仍使用 `GetImmediateCommandList()`。
- Forward draw 仍通过 `SetVertexBufferView()` 提交 mesh。
- `CommandList::BeginFrame(Camera&)` 仍泄漏 renderer/frame 高层语义。
- `CommandList::DrawIndexed()` 仍硬编码要求 frame/object bind group 已提交。
- `PassGraph` 生成 barrier plan，但不执行 barrier，也不创建 transient resources。
- `TextureResource`、`RenderTarget`、`PipelineState`、`BindGroup` 仍是简化模型。

因此下一阶段重点不是继续“加宽”RHI API，而是让当前 rendering 主路径真正吃掉已建立的现代抽象，并逐步把高层语义从 RHI 下沉边界中移出去。

## 2. 当前形态判断

当前可以称为：

```text
现代 RHI 的可演进骨架，而不是完整现代 RHI 运行时。
```

强项：

- draw path 已基本收敛到 `PipelineState + BindGroup + Vertex/Index binding + Draw`。
- backend 创建路径已集中到 `RenderDevice`。
- binding contract 已能在 pipeline 与 command submit 时校验。
- RenderGraph/PassGraph 已经有资源访问和 barrier plan 雏形。

短板：

- command buffer 还没有真实录制和 submit 生命周期。
- resource barrier 还是 OpenGL no-op，且 graph 没有执行 barrier plan。
- resource model 还缺 texture usage/view/sampler/mip/sample count。
- pipeline state 缺 render target formats、blend、depth/stencil、raster state。
- bind group 缺 shader stage visibility、buffer range、sampler、descriptor cache/pool。
- rendering layer 仍在每 item 创建 pipeline/bind group layout，缺缓存与批处理。

## 3. 总目标

下一阶段目标：

```text
让 Forward 主路径从 compatibility path 迁移到现代 RHI path，并清理 RHI 中的高层 renderer 语义，为后续 RenderGraph 驱动资源生命周期打基础。
```

阶段完成后应达到：

- Forward draw 不再依赖 `VertexBufferView` 作为唯一提交方式。
- `CommandList` 不再理解 Camera、frame/object/material 这类 renderer 概念。
- Draw 前置条件由 pipeline/binding 状态通用校验表达，而不是硬编码 frame/object bind group。
- RenderGraph barrier plan 能被执行或至少能通过明确 hook 接入 command list。
- Resource / Pipeline / BindGroup 后续扩展方向明确，不再继续临时补字段。

## 4. 非目标

本阶段不做：

- 不实现 Vulkan/D3D12/Metal backend。
- 不删除 OpenGL backend。
- 不一次性删除 `VertexBufferView`。
- 不引入完整 shader reflection。
- 不实现完整 descriptor allocator / descriptor heap。
- 不实现 GPU memory allocator。
- 不重写整个 RenderGraph。
- 不做多线程 command recording。
- 不做完整材质系统重构。

## 5. 分阶段设计

## P33：Forward 主路径迁移到 explicit vertex/index binding

### 5.1 目标

Forward pass 当前仍使用：

```cpp
SetVertexBufferView(*resolvedItem.VertexBufferViewRef);
DrawIndexed(resolvedItem.VertexBufferViewRef->GetDesc().IndexCount);
```

P33 目标是让 mesh/resource resolver 输出显式 vertex/index binding，使 Forward pass 使用：

```cpp
SetVertexBuffer(0, resolvedItem.VertexBufferBinding);
SetIndexBuffer(resolvedItem.IndexBufferBinding);
DrawIndexed(resolvedItem.IndexBufferBinding.IndexCount);
```

`VertexBufferView` 暂时保留，作为资源创建与兼容容器存在，但 Forward 主路径不再直接消费它。

### 5.2 建议改动

- `ResolvedRenderItem` 增加：
  - `VertexBufferBinding VertexBinding`
  - `IndexBufferBinding IndexBinding`
- `RenderResourceResolver::Resolve()` 从 mesh 的 `VertexBufferView` 中拆出 vertex/index binding。
- `ForwardOpaquePass::Execute()` 改用 `SetVertexBuffer` / `SetIndexBuffer`。
- 保留 `VertexBufferViewRef`，用于短期提供 layout / compatibility 信息。

### 5.3 验收

- `ForwardRenderPipeline.cpp` 主 draw path 不再调用 `SetVertexBufferView`。
- `RenderingOperationsSmoke` 通过。
- `RHICommandListBindingSmoke` 继续覆盖 explicit path 与 compatibility path。

## P34：清理 CommandList 高层 renderer 语义

### 5.4 目标

当前 `CommandList` 仍有：

```cpp
BeginFrame(Camera&)
EndFrame()
DrawIndexed()
```

其中 `BeginFrame(Camera&)` 是明显高层语义；`DrawIndexed()` 在 OpenGL backend 中还硬编码检查 frame/object bind group。P34 目标是把这些概念从 RHI 层移除或退化为 compatibility helper。

### 5.5 建议改动

第一步：

- 新增更底层的 `BeginRenderingScope()` / `EndRenderingScope()` 或直接让 render pass 内 draw 不再需要 frame begin/end。
- Forward pass 暂时可以继续调用 `BeginFrame/EndFrame`，但实现内部不再持有 `Camera*`。

第二步：

- 将 `DrawIndexed` 校验改为通用状态：
  - pipeline state 是否已绑定。
  - vertex/index binding 是否已绑定。
  - pipeline 声明的 required bind group slots 是否都已绑定且 layout 匹配。
- 删除 OpenGL command list 内部 `m_HasFrameBindGroup` / `m_HasObjectBindGroup` 这类 scope 特化状态。

第三步：

- Forward pass 负责决定 slot 0/1/2 的语义。
- RHI 只知道 slot 是否满足 pipeline contract。

### 5.6 验收

- `CommandList` 不再依赖 `Camera` 类型。
- OpenGL `DrawIndexed` 不再硬编码 frame/object bind group。
- 错误 binding 仍会被 contract validation 拒绝。
- 现有 RHI smoke 和 rendering smoke 通过。

## P35：PipelineState 补齐 render state contract

### 5.7 目标

当前 `PipelineStateDesc` 只有 shader、vertex layout、topology、bind group layout。现代 RHI 需要 pipeline 对 render target 与固定功能状态有明确 contract。

### 5.8 建议新增

- `ColorTargetState`
  - format
  - blend enable / blend factors
  - write mask
- `DepthStencilState`
  - depth test/write
  - compare op
  - stencil state
- `RasterState`
  - cull mode
  - front face
  - fill mode
- `PipelineStateDesc::ColorTargets`
- `PipelineStateDesc::DepthStencil`
- `PipelineStateDesc::Raster`

短期 OpenGL backend 可只支持当前默认值，但 pipeline 创建时应校验 render pass attachment format 与 pipeline color target format 是否兼容。

### 5.9 验收

- pipeline state 能声明 color attachment format。
- render pass begin 或 draw 前能发现 pipeline/target format mismatch。
- smoke 覆盖正确 format 与错误 format。

## P36：Texture / RenderTarget 资源模型收敛

### 5.10 目标

当前 `TextureDesc` 只有 `SourcePath`，`RenderTarget` 同时承担 framebuffer、attachment、resize、readback、clear。P36 目标是拆出更现代的资源描述。

### 5.11 建议方向

新增或扩展：

- `TextureUsage`
  - Sampled
  - ColorAttachment
  - DepthStencilAttachment
  - CopySrc
  - CopyDst
- `TextureDesc`
  - width / height
  - format
  - usage flags
  - mip levels
  - sample count
  - optional source path
- `TextureView`
- `Sampler`
- `RenderTarget` 逐步退化为 framebuffer/render pass attachment 聚合，而不是唯一 color attachment resource。

### 5.12 验收

- 可以通过 `RenderDevice::CreateTexture` 创建非 file-backed texture。
- RenderGraph transient texture desc 能映射到真实 texture resource。
- 旧 `CreateTexture({ SourcePath })` 继续可用。

## P37：RenderGraph 执行 barrier plan

### 5.13 目标

当前 `PassGraph::Compile()` 会生成 barrier plan，但 `Execute()` 没有使用。P37 目标是让 graph execution 能在 pass 前执行对应 barrier hook。

### 5.14 建议改动

- `PassGraphResourceBarrier` 从 name-based 计划逐步关联到 typed resource handle。
- `RenderPassContext` 增加 barrier execution 入口，或直接使用 `context.Commands->ResourceBarrier(...)`。
- 对尚未真实映射到 texture resource 的逻辑资源，先产生 diagnostic，不执行 GPU barrier。
- imported resources 支持携带真实 `TextureResource` / `RenderTarget` 引用。

### 5.15 验收

- graph execute 会按 pass index 执行 barrier plan。
- OpenGL backend 仍 no-op，但调用路径可被 smoke 覆盖。
- 未绑定真实 runtime resource 的 barrier 有明确 diagnostic。

## P38：缓存与批处理

### 5.16 目标

当前 `RenderResourceResolver` 每 item 创建 material bind group、frame/object layout、pipeline state。这对 smoke 足够，但不适合真实 renderer。

### 5.17 建议方向

- `PipelineStateCache`
  - key: shader + vertex layout + render state + bind group layouts
- `BindGroupLayoutCache`
  - frame/object 标准 layout 单例化
  - material layout 基于 base material contract
- `MaterialBindGroupCache`
  - instance override 变化时更新或重建
- Forward pass 按 pipeline/material 排序，减少 state switch。

### 5.18 验收

- 相同 material/mesh 多 item 不重复创建 pipeline state。
- frame/object standard layout 不重复创建。
- smoke 或计数测试能证明 cache hit。

## 6. 推荐执行顺序

推荐顺序：

```text
P33 Forward explicit binding migration
P34 CommandList high-level semantic cleanup
P35 PipelineState render state contract
P36 Texture / RenderTarget resource model
P37 RenderGraph barrier execution
P38 Pipeline / BindGroup cache and batching
```

原因：

1. P33 先让主路径消费 P32 的新能力，避免 compatibility path 长期存在。
2. P34 清理 RHI 语义边界，否则后续 backend 会继承错误抽象。
3. P35/P36 补齐资源与 pipeline contract，为 graph 真正执行做准备。
4. P37 再让 RenderGraph 接管 barrier/resource execution，避免 graph 先依赖不完整资源模型。
5. P38 处理性能和重复创建问题，适合在语义稳定后做。

## 7. 风险

- 过早删除 compatibility path 会扩大回归风险。
- 在资源模型不完整前强推 RenderGraph transient resource，容易产生临时抽象。
- 将 shader reflection、material contract、descriptor cache 同时推进会耦合过重。
- OpenGL backend 的 immediate 特性可能掩盖 command buffer/resource state 设计缺陷。
- cache key 如果过早设计过宽，会拖慢后续 pipeline state 扩展。

## 8. 下一步建议

下一步建议单独为 P33 写 implementation plan 并执行。

P33 的最小交付边界：

- `ResolvedRenderItem` 增加 explicit vertex/index binding。
- `RenderResourceResolver` 从 `VertexBufferView` 拆出 binding。
- `ForwardOpaquePass` 使用 `SetVertexBuffer` / `SetIndexBuffer`。
- `SetVertexBufferView` 保留用于 smoke compatibility path。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke` 通过。

P33 不应该同时做：

- 不删除 `VertexBufferView`。
- 不移除 `BeginFrame/EndFrame`。
- 不做 pipeline cache。
- 不改 material/bind group 生命周期。

### P33 实现结果

- `ResolvedRenderItem` 已新增 explicit `VertexBinding` 与 `IndexBinding`。
- `RenderResourceResolver` 已从 mesh `VertexBufferView` 拆出 vertex/index binding。
- `ForwardOpaquePass` 已改用 `SetVertexBuffer` / `SetIndexBuffer`。
- `VertexBufferView` 保留为 compatibility/layout 来源。
- `RenderingOperationsSmoke` 已覆盖 Forward 主路径不再调用 `SetVertexBufferView`。
- `RHICommandListBindingSmoke` 继续覆盖 `SetVertexBufferView` compatibility path。
### P34 实现结果

- `CommandList` 已移除 `Camera` 类型依赖，`BeginFrame(Camera&)` 已收敛为 `BeginFrame()`。
- `OpenGLCommandList` 已删除 `m_CurrentCamera`、`m_HasFrameBindGroup`、`m_HasObjectBindGroup`。
- OpenGL draw 前置校验已从 frame/object scope 特化检查改为按 `PipelineStateDesc::BindGroupLayouts` 检查 required slot 是否完成绑定。
- Forward pass 继续负责 slot 0/1/2 的业务语义，RHI 只校验 pipeline contract。
- `RHICommandListBindingSmoke` 增加结构约束，覆盖 CommandList 无 Camera 依赖与 OpenGL 无 Frame/Object 特化 gating。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P35 实现结果

- `PipelineStateDesc` 已新增 `ColorTargets`、`DepthStencil`、`Raster` 三类 render state contract。
- 默认 pipeline contract 声明一个 `RGBA8` color target、`DEPTH24_STENCIL8` depth/stencil target 和兼容 raster 默认值。
- OpenGL `CreatePipelineState` 已校验 color target 列表不能为空、color target format 不能为 `None` 或 depth format、depth/stencil format 不能为 color format。
- `RHIResourceCreationSmoke` 已覆盖默认 contract、显式 contract round-trip 和非法 format 拒绝。
- 本阶段只完成 contract 建模和创建期校验，暂不做 render pass active target 与 pipeline format 的 draw-time mismatch 检查。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P36 实现结果

- `TextureDesc` 已从单一 `SourcePath` 扩展为包含 `Width`、`Height`、`Format`、`Usage`、`MipLevels`、`Samples` 的 texture resource 描述。
- 新增 `TextureUsageFlags`，覆盖 sampled、color attachment、depth/stencil attachment、copy src、copy dst 等基础用途。
- 新增轻量 `TextureViewDesc` 与 `SamplerDesc`，作为后续 view/sampler 创建 API 的类型基础。
- OpenGL `CreateTexture` 已支持无文件来源的 GPU texture storage 创建。
- 旧 file-backed texture 路径继续可用，并会在创建后补齐 desc 中的尺寸、格式和 usage 信息。
- `RHIResourceCreationSmoke` 已覆盖 file-backed texture、non-file-backed texture、空 desc、无 format、无 usage 等路径。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P37 实现结果

- `PassGraph` 已新增 `PassGraphBarrierExecutor` 与 `SetBarrierExecutor()`，作为 barrier plan 执行 hook。
- `PassGraph::Execute()` 已按 pass index 在 pass callback 前执行匹配的 `PassGraphResourceBarrier`。
- 当前阶段仍不伪造真实 `TextureResource` barrier；graph resource name 到真实 runtime resource 的映射留给后续资源生命周期阶段。
- barrier executor 为空时保持 no-op 兼容，现有 graph 执行路径不受影响。
- `RenderPassGraphSmoke` 已覆盖 barrier execution order。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。
