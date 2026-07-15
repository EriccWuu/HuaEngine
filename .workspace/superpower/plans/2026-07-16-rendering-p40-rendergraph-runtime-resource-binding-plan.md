# P40 RenderGraph Runtime Resource Binding 计划

状态：执行中
日期：2026-07-16
基线：`69e687f feat(rendering): record command buffers`

## 目标

让 `RenderGraphResourceDesc` 能绑定真实 runtime texture resource，并让 transient texture desc 在 graph execute 时分配真实 `TextureResource`。

## 本阶段范围

- `RenderGraphResourceDesc` 支持 imported `Ref<TextureResource>`。
- `RenderGraphResourceAllocator` 维护 runtime resource allocation table。
- `PassGraph::Execute()` 在 pass callback 前准备 runtime resources。
- imported texture resource 能从 allocation table 查询到。
- transient texture resource 能通过 `RenderDevice::CreateTexture()` 创建。

## 非目标

- 不绑定 `RenderTarget` attachment。
- 不处理 buffer transient allocation。
- 不做 aliasing / release / reuse。
- 不把 barrier hook 直接转换成 `CommandList::ResourceBarrier()`。

## RED 测试

在 `Tests/RHIResourceCreationSmoke.cpp` 中：

- 创建真实 `TextureResource`。
- 创建 graph imported texture，传入 runtime texture。
- 创建 graph transient texture。
- graph execute 后查询 runtime resource table。
- 验证 imported allocation 指向原 texture。
- 验证 transient allocation 创建出 texture，且 desc 尺寸/格式匹配。

## 验收标准

- imported texture graph resource 能查询到真实 runtime texture。
- transient texture graph resource 能在 execute 时创建真实 texture。
- 旧 `RenderPassGraphSmoke` 不需要 GL context，继续通过。
- 四个 smoke 通过。
- P40 单独提交。

## 执行结果

- `RenderGraphResourceDesc` 已支持 `RuntimeTexture`。
- `RenderGraphResourceAllocator` 已新增 runtime resource allocation table。
- `RenderGraphResourceAllocator::PrepareRuntimeResources()` 可保留 imported texture binding，并为 transient texture 创建 `TextureResource`。
- `RenderPassContext` 已新增可选 `RenderDevice* Device`，显式启用 runtime resource preparation。
- `PassGraph::Execute()` 在 context 提供 device 时准备 runtime resources。
- `RHIResourceCreationSmoke` 已覆盖 imported runtime texture preservation 与 transient runtime texture allocation。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
