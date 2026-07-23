# Rendering 现代 RHI 核心缺口复盘与下一阶段 Spec

状态：草案
日期：2026-07-16
基线提交：`9426da5 perf(rendering): cache resolved pipeline state`
范围：P33-P38 完成后，重新审视 HuaEngine Rendering / RHI 离现代 RHI runtime 的核心差距，并拆出下一组可执行阶段。

## 1. 背景

P33-P38 已经把 HuaEngine 从“OpenGL wrapper + 兼容路径”推进到“现代 RHI 主路径骨架”：

- Forward 主 draw path 已消费 `PipelineState + BindGroup + explicit Vertex/Index binding + DrawIndexed`。
- `CommandList` 已移除 `Camera` 依赖和 frame/object 硬编码 draw gating。
- `PipelineStateDesc` 已具备 color target、depth/stencil、raster state contract。
- `TextureDesc` 已具备 width、height、format、usage、mip、sample 等资源描述字段。
- `PassGraph` 已能生成 barrier plan，并在 pass 前触发 barrier execution hook。
- `RenderResourceResolver` 已缓存 frame/object bind group layout 与 pipeline state。

当前可判断为：

```text
现代 RHI 的主路径骨架已经建立，但还不是完整现代 RHI runtime。
```

核心问题已经从“API 形状不现代”转为“生命周期、资源绑定、同步和 backend 应用还不真实”。

## 2. 代码勘探摘要

### 2.1 Command / Queue

现状：

- `CommandBuffer` 只有 `CommandBufferDesc` 和 `GetDesc()`。
- `RenderQueue::Submit()` 只接收 `CommandBuffer&`，没有 fence、timeline、submit batch、wait/signal。
- OpenGL backend 的 `Submit()` 只做 usage 校验，没有真实 command list replay。

结论：

`CommandBuffer` 目前是资源对象占位，而不是可录制、可关闭、可提交的命令容器。

### 2.2 CommandList

现状：

- `CommandList` 已包含 render pass、resource barrier、pipeline、vertex/index、bind group、draw。
- `BeginFrame()` 仍存在，但已经退化为无参 compatibility scope。
- OpenGL `DrawIndexed()` 已按 pipeline-declared bind group slots 检查绑定完整性。

结论：

Immediate command path 可以继续服务 smoke，但还缺 recorded command path。下一步不应继续扩 `CommandList` 表面 API，而应建立 recording backend。

### 2.3 Resource / Barrier

现状：

- `ResourceBarrier` API 存在。
- `OpenGLCommandList::ResourceBarrier()` 基本是 no-op，只检查 null texture。
- `PassGraph` 能生成 `PassGraphResourceBarrier`，也能执行 barrier hook。
- Graph resource name 尚未映射到真实 `TextureResource` / `RenderTarget` runtime object。

结论：

barrier 现在是计划与 hook，不是资源状态系统。缺少 resource state tracker、runtime resource binding、真实 barrier emission。

### 2.4 Texture / RenderTarget

现状：

- `TextureDesc` 已支持非文件纹理描述。
- OpenGL `CreateTexture()` 已支持 file-backed 与 non-file-backed storage。
- `TextureViewDesc` / `SamplerDesc` 只是数据结构，还没有 RHI create API。
- `RenderTarget` 仍同时承担 framebuffer、attachment、resize、readback、clear attachment view。

结论：

Texture resource model 已启动，但 view/sampler/framebuffer attachment 分层尚未完成。

### 2.5 Pipeline / Descriptor

现状：

- `PipelineStateDesc` 已声明 render state contract。
- OpenGL backend 主要做创建期校验，尚未完整应用 blend/depth/stencil/raster state。
- `BindGroup` 有 layout contract，但缺 shader stage visibility、buffer range、sampler、texture view、dynamic offset、descriptor allocator。
- Material bind group layout 仍按 material parameters 构建，缺稳定 schema 或 shader reflection。

结论：

pipeline layout 与 bind group layout 的结构已经出现，但 descriptor/runtime binding 生命周期仍偏简化。

### 2.6 RenderGraph

现状：

- `PassGraph::Compile()` 生成 resource lifetime 和 barrier plan。
- `PassGraph::Execute()` 能在 pass 前执行 barrier executor。
- 暂无 transient resource allocation。
- 暂无 imported runtime resource binding。
- 暂无 aliasing / release / reuse。

结论：

RenderGraph 目前是 dependency graph + barrier schedule，还不是 frame resource owner。

## 3. 与现代 RHI 的差距

参考 Vulkan / D3D12 / WebGPU 的共同模型，现代 RHI 的关键能力包括：

- command buffer/list 有明确 recording、closed/executable、submitted/pending、reset 生命周期。
- queue submit 产生同步对象或 timeline，可表达 wait/signal。
- resource state transition 由应用或 RHI 层显式管理。
- texture/buffer/resource 与 view/sampler/descriptor 分层。
- pipeline state 是不可变 contract，并和 render pass attachment、pipeline layout 兼容。
- bind group/descriptor set 与 pipeline layout 按 slot/schema 匹配。
- render graph 负责 transient resource allocation、state transition、lifetime 和 pass execution ordering。

HuaEngine 当前已覆盖其中的 API 轮廓，但真实 runtime 能力还缺：

| 能力 | 当前状态 | 缺口 |
| --- | --- | --- |
| Command recording | `CommandBuffer` 只有 desc | 无 begin/end/reset/record/replay |
| Queue submit | `Submit(CommandBuffer&)` 占位 | 无 batch/fence/timeline |
| Resource state | `ResourceBarrier` + graph plan | 无 state tracker/runtime resource mapping |
| Texture resource | `TextureDesc` 已扩展 | 无 view/sampler API，无 attachment 分层 |
| Pipeline state | contract 已声明 | backend 未完整 apply，缺 format compatibility |
| Descriptor/bind group | layout contract 已有 | 缺 visibility/range/sampler/view/allocator |
| RenderGraph | dependency + barrier hook | 无 transient allocation/imported runtime binding |
| Material schema | parameter-driven layout | 缺稳定 schema/reflection/cache key 规范 |

## 4. 下一阶段总目标

下一阶段目标：

```text
把现代 RHI 从“主路径 API 骨架”推进到“具备真实生命周期的 runtime 雏形”。
```

完成后应达到：

- `CommandBuffer` 能记录最小 draw command stream，并通过 queue submit replay。
- RenderGraph resource 能绑定真实 runtime texture/render target。
- Graph barrier hook 能落到 `CommandList::ResourceBarrier()`，并由 state tracker 管理 before/after。
- Texture view / sampler 成为一等 RHI 对象，并进入 bind group。
- Pipeline render state 至少在 OpenGL backend 中真实应用。
- Material / bind group layout schema 更稳定，减少每 item layout 生成。

## 5. 非目标

本阶段不做：

- 不实现 Vulkan/D3D12/Metal backend。
- 不做完整多线程 command recording。
- 不做 GPU memory allocator。
- 不做完整 bindless descriptor。
- 不做 shader reflection 全量方案。
- 不重写整个 Forward renderer。
- 不删除 OpenGL immediate path。
- 不做完整 async compute/copy queue 调度。

## 6. 阶段拆分

## P39：真实 CommandBuffer Recording Skeleton

### 6.1 目标

让 `CommandBuffer` 从 desc-only 对象变成可录制的最小命令容器。

### 6.2 建议改动

- 新增 command recording API：
  - `Begin()`
  - `End()`
  - `Reset()`
  - `IsRecording()`
  - `IsExecutable()`
- 新增最小 recorded command 类型：
  - begin/end render pass
  - resource barrier
  - set pipeline state
  - set vertex/index buffer
  - set bind group
  - draw indexed
- OpenGL backend 可先 replay 到 immediate command list。
- 保留 immediate command list，不强制 Forward 立即切换。

### 6.3 验收

- smoke 能创建 command buffer，录制一个最小 triangle draw，submit 后 render target 有非 clear pixel。
- command buffer 未 `End()` 时 submit 失败。
- `Reset()` 后可重新录制。

## P40：RenderGraph Runtime Resource Binding

### 6.4 目标

让 `RenderGraphResourceDesc` 能绑定真实 runtime resource，为 barrier execution 和 transient allocation 打基础。

### 6.5 建议改动

- imported resource 支持携带：
  - `Ref<TextureResource>`
  - 或 `Ref<RenderTarget>` / color attachment view 过渡结构
- transient texture desc 能通过 `RenderDevice::CreateTexture()` 分配真实 texture。
- `RenderGraphResourceAllocator` 保存 runtime allocation table。
- PassGraph execute 前完成 transient allocation。

### 6.6 验收

- imported texture graph execute 时能找到 runtime texture。
- transient texture graph compile/execute 后能创建 texture resource。
- 未绑定 runtime resource 的 barrier 产生 diagnostic，而不是静默 no-op。

## P41：Resource State Tracker 与 Barrier Emission

### 6.7 目标

把 graph barrier hook 转为真实 `CommandList::ResourceBarrier()` 调用，并记录 resource current state。

### 6.8 建议改动

- 新增 `ResourceStateTracker`。
- 以 runtime resource identity 为 key 记录 current state。
- `PassGraphResourceBarrier` 通过 runtime resource binding 转换为 `ResourceBarrier`。
- OpenGL backend 仍可 no-op，但 RHI 层应能记录状态变化。

### 6.9 验收

- graph execute 会对 imported/transient texture 调用 barrier executor。
- state tracker 能从 `Undefined -> RenderTarget -> ShaderRead`。
- 重复相同 state 不产生冗余 barrier。

## P42：TextureView / Sampler RHI API

### 6.10 目标

让 texture view 和 sampler 成为一等 RHI 对象，解决 bind group 中直接绑定 texture resource 的粗粒度问题。

### 6.11 建议改动

- 新增：
  - `class TextureView`
  - `class Sampler`
  - `RenderDevice::CreateTextureView`
  - `RenderDevice::CreateSampler`
- `BindGroupEntry` 支持 texture view + sampler。
- 保留旧 `Ref<TextureResource>` binding 作为短期兼容路径。

### 6.12 验收

- 可创建 sampled texture view。
- 可创建 sampler。
- material texture binding 能使用 texture view + sampler。
- 旧 texture binding smoke 继续通过。

## P43：Pipeline Render State Backend Apply

### 6.13 目标

让 P35 的 pipeline render state contract 真正在 OpenGL backend 生效。

### 6.14 建议改动

- `SetPipelineState()` 应用：
  - cull mode
  - front face
  - fill mode
  - depth test/write/compare
  - blend enable/write mask
- draw 前校验当前 render target format 与 pipeline color/depth target contract。

### 6.15 验收

- smoke 覆盖 depth disabled / cull none / blend enabled 至少一个可观察行为。
- pipeline color format 与 render target attachment format mismatch 会拒绝 draw 或产生 diagnostic。

## P44：Material / BindGroup Layout Schema 稳定化

### 6.16 目标

降低 material parameter 临时拼 layout 的不稳定性，为 descriptor cache 和 pipeline cache 做稳定 key。

### 6.17 建议改动

- 为 `Material` 建立 `MaterialBindingSchema`。
- schema 包含 parameter name/type/binding/texture slot/stage visibility。
- `CreateMaterialBindGroup()` 基于 schema 创建 bind group。
- pipeline cache key 改用 schema id/signature。

### 6.18 验收

- 相同 base material 的多个 instance 共享 material layout schema。
- material override 不改变 layout 时 pipeline cache hit 稳定。
- 新增 smoke 验证同 material 多 item 不重复创建 material layout。

## P45：Queue Fence / Timeline Skeleton

### 6.19 目标

让 queue submit 具备最小同步返回值。

### 6.20 建议改动

- 新增：
  - `Fence`
  - `QueueSubmitDesc`
  - `QueueSubmitResult`
- `RenderQueue::Submit()` 从 void 改为返回 submit result 或 signal value。
- OpenGL backend 可用 CPU-side monotonically increasing fence value 模拟。

### 6.21 验收

- submit 返回递增 fence value。
- 可 query completed value。
- invalid command buffer submit 返回失败结果。

## P46：RenderTarget 退化为 Attachment Aggregation

### 6.22 目标

开始拆分 `RenderTarget` 的 monolithic 语义，让它逐步成为 texture attachments + framebuffer wrapper。

### 6.23 建议改动

- `RenderTarget` 内部 attachment 关联 `TextureResource` / `TextureView`。
- `GetColorAttachmentView()` 逐步退化为 view accessor。
- render pass desc 使用 attachment view 而不是只依赖 render target + index。
- 保留 readback helper 用于 smoke。

### 6.24 验收

- render target color attachment 能暴露为 texture/view。
- graph imported render target 能映射到 color texture resource。
- 现有 render target readback smoke 继续通过。

## 7. 推荐执行顺序

推荐顺序：

```text
P39 CommandBuffer recording skeleton
P40 RenderGraph runtime resource binding
P41 Resource state tracker and barrier emission
P42 TextureView / Sampler RHI API
P43 Pipeline render state backend apply
P44 Material / BindGroup layout schema
P45 Queue fence / timeline skeleton
P46 RenderTarget attachment aggregation
```

理由：

1. P39 先补 command lifetime，否则 queue/fence 都没有可提交对象。
2. P40/P41 让 RenderGraph barrier 从 hook 进入真实 resource state。
3. P42 补 descriptor 资源粒度，否则 material/schema 会继续绑定粗粒度 texture。
4. P43 让 pipeline contract 从声明变为 backend 行为。
5. P44 在 view/sampler/pipeline 语义稳定后做 schema/cache。
6. P45 补 queue 同步，适合在 command buffer 和 barrier 语义稳定后做。
7. P46 拆 RenderTarget，风险较高，放在 texture/view/graph 资源路径之后。

## 8. 风险

- 过早把 Forward 主路径切到 recorded command buffer，可能扩大回归面。
- RenderGraph transient allocation 如果没有 resource state tracker，会形成“创建了资源但状态不可解释”的半成品。
- TextureView/Sampler API 如果和 bind group schema 同时大改，容易导致 material path 震荡。
- Pipeline backend apply 可能暴露当前 smoke 对深度/裁剪状态的隐含依赖。
- Queue fence/timeline 在 OpenGL 上只能模拟，不能过度承诺跨 backend 语义。
- RenderTarget 拆分会影响 editor viewport/readback/clear path，需要保留 compatibility helper。

## 9. 完成定义

下一阶段完成时，应满足：

- Forward 主路径仍通过现有 smoke。
- 至少一个 smoke 通过 recorded command buffer submit 完成 draw。
- RenderGraph 能分配或绑定真实 texture resource。
- Resource state tracker 能证明 barrier before/after 变化。
- TextureView/Sampler 能进入 bind group。
- Pipeline render state 至少部分可观察地影响 backend。
- 每完成一个 P，单独提交。

## 10. 参考资料

- Vulkan Command Buffers：https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html
- Microsoft D3D12 Command Lists：https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles
- Microsoft D3D12 Resource Barriers：https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
- WebGPU Specification：https://www.w3.org/TR/webgpu/

## 11. 执行记录

### P39 实现结果

- `CommandBuffer` 已从 desc-only 对象扩展为具备 begin/end/reset/recording/executable 状态的最小命令容器。
- `CommandBuffer` 已支持录制 begin/end render pass、set pipeline、set vertex/index buffer、set bind group、draw indexed。
- `RenderQueue::Submit()` 已改为返回 `bool`，用于反馈 submit 是否被接受。
- OpenGL backend 已实现 recorded command replay 到 immediate command list。
- 未 `End()` 的 command buffer submit 会返回 false；已 `End()` 的 command buffer submit 能执行 recorded draw。
- `RHICommandListBindingSmoke` 已覆盖 recorded command buffer draw path。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P40 实现结果

- `RenderGraphResourceDesc` 已支持 imported `RuntimeTexture`。
- `RenderGraphResourceAllocator` 已维护 runtime resource allocation table，并提供 `GetRuntimeResource()` 查询。
- `PassGraph::Execute()` 在 `RenderPassContext::Device` 存在时准备 runtime resources。
- imported texture resource 会保留传入的真实 `TextureResource`。
- transient texture resource 会通过 `RenderDevice::CreateTexture()` 创建真实 texture。
- 纯图 `RenderPassGraphSmoke` 不提供 device，因此保持无 GL context 兼容。
- `RHIResourceCreationSmoke` 已覆盖 imported runtime texture binding 与 transient texture allocation。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P41 实现结果

- 新增 `ResourceStateTracker`，按 texture runtime identity 记录当前 resource state。
- `ResourceStateTracker::Transition()` 会在状态变化时生成 `ResourceBarrier`，同状态重复 transition 不发 barrier。
- `RenderPassContext` 已新增 `ResourceStateTracker* ResourceStates`。
- `PassGraph::Execute()` 已能从 barrier plan 和 runtime resource table 解析真实 texture，并调用 `CommandList::ResourceBarrier()`。
- OpenGL backend 的 `ResourceBarrier()` 仍是 no-op 语义，但调用路径和 state tracking 已可被 smoke 覆盖。
- `RHIResourceCreationSmoke` 已覆盖 graph execute 发出 `Undefined -> ShaderRead` barrier，以及重复 execute 不发重复 barrier。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P42 实现结果

- 新增 `TextureView` 与 `Sampler` RHI 对象，`RenderDevice` 已暴露 `CreateTextureView()` 与 `CreateSampler()`。
- OpenGL backend 已新增 `OpenGLTextureView` 与 `OpenGLSampler`，sampler filter/address state 映射到 GL sampler object。
- `BindingValueType` 与 `BindingValue` 已支持 `TextureView` 与 `Sampler`，并保留旧 `TextureResource` binding 作为兼容路径。
- OpenGL `SetBindGroup()` 已支持绑定 texture view 与 sampler；texture view 当前复用底层 texture storage，不引入 `glTextureView`。
- `RHIResourceCreationSmoke` 已覆盖 texture view 创建、sampler 创建、texture view + sampler bind group，以及空 texture view desc 拒绝路径。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P43 实现结果

- `OpenGLCommandList::SetPipelineState()` 已应用 raster/depth/blend/color write mask backend state。
- `OpenGLCommandList::DrawIndexed()` 已在 draw 前校验当前 render target color/depth-stencil format 与 pipeline target contract。
- render pass clear 与 `ClearColor()` 已显式恢复 color/depth write mask，避免 pipeline state 泄漏影响 clear 行为。
- Forward `BindTargetPass` 已绑定 depth/stencil attachment 并清 depth，使默认 depth pipeline state 在主路径真实生效。
- `RHICommandListBindingSmoke` 已覆盖 GL state query，以及 color target format mismatch 不写入 render target。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P44 实现结果

- 新增 `MaterialBindingSchema` 与 `MaterialBindingSchemaEntry`，作为 material bind group layout 的稳定描述。
- `Material::GetBindingSchema()` 已按参数名稳定排序并生成 schema signature；material instance override value 不改变 base material schema signature。
- `RenderBindGroupBuilder` 已支持基于 schema 创建 material bind group layout，并支持使用 cached layout 创建 bind group。
- `RenderResourceResolver` 已新增 material bind group layout cache，cache key 使用 schema signature。
- pipeline cache entry 已改用 material schema signature，不再依赖临时 layout entries。
- `RHIResourceCreationSmoke` 已覆盖 schema 排序、binding 编号、signature 与 override 稳定性。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P45 实现结果

- 新增 `Fence`、`QueueSubmitDesc` 与 `QueueSubmitResult`。
- `RenderQueue::Submit(CommandBuffer&)` 已返回 `QueueSubmitResult`，并通过隐式 `operator bool()` 保持旧 success/failure 判断兼容。
- `RenderQueue` 已提供 `Submit(const QueueSubmitDesc&)` convenience path 和 `GetTimelineFence()`。
- OpenGL graphics queue 已使用 CPU-side timeline fence 模拟同步；成功 submit 会递增 signal value 并更新 completed value。
- invalid/recording/non-executable submit 会返回失败结果，signal value 为 0。
- `RHICommandListBindingSmoke` 已覆盖失败 submit、成功 submit、连续 submit signal 递增，以及 fence completed value。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P46 实现结果

- `RenderTargetColorAttachmentView` 已扩展 native handle、format、width、height、samples、attachment index metadata。
- `RenderTarget` 已新增 `GetDepthStencilAttachmentView()`。
- OpenGL render target storage 已暴露 depth attachment native handle。
- OpenGL render target color/depth view accessor 已返回 attachment metadata。
- `RHIResourceCreationSmoke` 已覆盖 color/depth attachment view metadata 与 native handle。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P47：Attachment-backed TextureResource

#### 目标

将 `RenderTarget` 的 color/depth-stencil attachment 从仅含 native handle 的元数据，提升为可被 RHI、RenderGraph 与 bind group 消费的真实 `TextureResource` 和默认 `TextureView`。

#### 约束

- OpenGL framebuffer storage 仍是 attachment GL texture 的唯一所有者，资源包装不得重复释放 GL texture。
- attachment texture 在 render target resize 后必须继续解析到新创建的底层 GL texture。
- color attachment 必须带有 `TextureUsageColorAttachment | TextureUsageSampled`；depth/stencil attachment 必须带有 `TextureUsageDepthStencilAttachment | TextureUsageSampled`。
- 保留 P46 的 metadata accessor 与现有 readback API，避免 editor/smoke 路径回归。

#### 验收

- `GetColorAttachmentTexture()` 与 `GetDepthStencilAttachmentTexture()` 返回有效资源，尺寸、格式、sample count 与 render target specification 一致。
- `GetColorAttachmentTextureView()` 与 `GetDepthStencilAttachmentTextureView()` 返回以对应 attachment texture 为源的默认 view。
- attachment texture 可用于创建 bind group 的 texture view binding。
- resize 后既有 attachment resource/view 仍可解析并绑定新的 GL attachment，且 metadata 与 texture desc 更新。

### P48：RenderPass Attachment View Contract

#### 目标

使 `RenderPassDesc` 直接使用 `TextureView` 描述 color/depth-stencil attachment，并让 OpenGL backend 从 attachment-backed view 解析 framebuffer storage。

#### 约束

- `RenderPassColorAttachment::View` 和 `RenderPassDepthStencilAttachment::View` 是主契约。
- 既有 `Target` 与 `AttachmentIndex` 仅作为过渡兼容路径。
- OpenGL 仅接受来自同一 attachment-backed render target storage 的 color/depth view；普通 sampled texture view 不能直接作为 render pass output。
- pipeline target format 校验必须使用当前 attachment view 的 format，而不是依赖 legacy target。

#### 验收

- 不提供 `Target` 的 view-only render pass 可以完成 draw 并写入 render target。
- Forward 主路径以 color/depth attachment view 创建 render pass。
- legacy target-only render pass smoke 继续通过。

#### 实现结果

- `RenderPassColorAttachment` 与 `RenderPassDepthStencilAttachment` 已新增 `View`，并保留 `Target`/`AttachmentIndex` 作为兼容回退。
- OpenGL command list 已从 attachment-backed texture view 解析 framebuffer storage、color/depth format，并拒绝将普通 sampled texture view 作为 render pass output。
- pipeline target contract 现在基于已解析的 attachment view format 校验。
- Forward `BindTargetPass` 已使用 color/depth attachment view 构建 render pass。
- `RHICommandListBindingSmoke` 已覆盖不携带 legacy target 的 view-only render pass draw；legacy render pass smoke 保持通过。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 已通过。

### P49：跨 Pass Attachment 读写与状态迁移

#### 目标

验证 render target attachment 可以作为 graph imported texture：先被一个 pass 写入，再被后续 pass 以 sampled texture 消费，并产生 `RenderTarget -> ShaderRead` 状态迁移。

#### 约束

- graph pass callback 必须能解析当前 execute 的 runtime texture，但不得取得 allocator 的可变所有权。
- imported attachment 的 runtime identity 必须是 P47 暴露的 `TextureResource`。
- 第二个 pass 创建的 sampled view 必须以同一 attachment texture 为源。

#### 验收

- `RenderPassContext` 能查询 runtime graph resource。
- 两 pass graph 的 barrier 顺序为 `Undefined -> RenderTarget`、`RenderTarget -> ShaderRead`，且均指向同一个 attachment texture。
- writer pass 可通过 attachment view 建立 render pass；reader pass 可从解析出的 runtime texture 创建 sampled view。

#### 实现结果

- `RenderPassContext` 已提供只读 `GraphResources`，`PassGraph::Execute()` 仅在执行期间注入 allocator，结束时恢复原 context。
- `RHIResourceCreationSmoke` 已将 RenderTarget color attachment 作为 imported runtime texture。
- writer pass 使用 attachment view 建立 render pass；reader pass 通过 `GraphResources` 解析同一 texture 并创建 sampled view。
- smoke 已验证 `Undefined -> RenderTarget -> ShaderRead` barrier plan、最终 state 和 attachment texture identity。
