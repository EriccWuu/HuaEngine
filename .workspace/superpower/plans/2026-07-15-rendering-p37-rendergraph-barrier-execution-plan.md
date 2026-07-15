# P37 RenderGraph Barrier Plan Execution 计划

状态：执行中
日期：2026-07-15

## 目标

让 `PassGraph::Execute()` 不再只执行 pass callback，而是在每个 pass 前执行 `Compile()` 生成的 barrier plan hook。

## 本阶段范围

- 在 `PassGraph` 增加 barrier executor callback。
- `Execute()` 按 pass index 执行对应 barrier，再执行 pass。
- barrier executor 接收 `PassGraphResourceBarrier` 和 `RenderPassContext&`。
- 默认 executor 为空时保持兼容 no-op。
- `RenderPassGraphSmoke` 覆盖 barrier 执行顺序：barrier 必须发生在 pass callback 之前。

## 非目标

- 不把 graph resource name 直接伪造为真实 `TextureResource`。
- 不实现 transient texture 真实分配。
- 不修改 `CommandList::ResourceBarrier` 签名。
- 不实现 Vulkan/D3D12 级资源状态转换。

## 验收标准

- `PassGraph::SetBarrierExecutor()` 可设置 barrier hook。
- `Execute()` 对同一 pass 的 barriers 在 pass callback 之前触发。
- 没有 barrier executor 时旧 smoke 仍可执行。
- `RenderPassGraphSmoke` 覆盖 barrier execution order。
- 四个 smoke 通过，提交为单独 P37 commit。

## 执行结果

- `PassGraph` 已新增 `PassGraphBarrierExecutor` 和 `SetBarrierExecutor()`。
- `PassGraph::Execute()` 已改为按 pass index 执行 barrier executor，再执行对应 pass callback。
- barrier executor 为空时保持 no-op 兼容行为。
- `RenderPassGraphSmoke` 已覆盖 barrier 先于 pass callback 执行。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
