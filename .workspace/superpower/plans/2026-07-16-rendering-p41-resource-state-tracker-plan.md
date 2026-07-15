# P41 Resource State Tracker 与 Barrier Emission 计划

状态：执行中
日期：2026-07-16
基线：`45e97f4 feat(rendering): bind render graph runtime resources`

## 目标

把 `PassGraph` 的 barrier plan 从 hook/diagnostic 推进为真实 `CommandList::ResourceBarrier()` emission，并通过 resource state tracker 避免重复 barrier。

## 本阶段范围

- 新增 `ResourceStateTracker`。
- `RenderPassContext` 可携带 `ResourceStateTracker*`。
- `PassGraph::Execute()` 在 runtime texture 可解析时：
  - 根据 desired state 请求 tracker transition。
  - tracker 生成 `ResourceBarrier`。
  - 调用 `context.Commands->ResourceBarrier()`。
- 同状态重复 transition 不产生重复 barrier。
- 保留自定义 `PassGraphBarrierExecutor` hook。

## 非目标

- 不实现 OpenGL 真实 GPU memory barrier。
- 不做跨 queue ownership transfer。
- 不处理 buffer resource state。
- 不实现 RenderTarget attachment resource mapping。

## RED 测试

在 `Tests/RHIResourceCreationSmoke.cpp` 中：

- 用真实 imported texture 创建 graph。
- 提供 fake `CommandList` 捕获 `ResourceBarrier()` 调用。
- 提供 `ResourceStateTracker`。
- execute 后验证第一次 barrier 为 `Undefined -> ShaderRead`。
- 第二次 execute 同样 graph 时不产生重复 barrier。

## 验收标准

- graph execute 能对 runtime texture 发出 `CommandList::ResourceBarrier()`。
- tracker 记录状态，重复同状态不会重复发 barrier。
- 四个 smoke 通过。
- P41 单独提交。

## 执行结果

- 新增 `ResourceStateTracker`，按 `TextureResource*` 记录当前 `ResourceState`。
- `ResourceStateTracker::Transition()` 会输出 `ResourceBarrier`，同状态重复 transition 返回 false。
- `RenderPassContext` 已新增 `ResourceStateTracker* ResourceStates`。
- `PassGraph::Execute()` 在 runtime texture、commands、tracker 都存在时调用 `CommandList::ResourceBarrier()`。
- 自定义 `PassGraphBarrierExecutor` hook 保留，仍会收到 barrier plan callback。
- `RHIResourceCreationSmoke` 已覆盖首次 graph execute 发出 `Undefined -> ShaderRead` barrier，重复 execute 不重复发 barrier。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
