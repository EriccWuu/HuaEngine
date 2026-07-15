# P39 CommandBuffer Recording Skeleton 计划

状态：执行中
日期：2026-07-16
基线：`9426da5 perf(rendering): cache resolved pipeline state`

## 目标

让 `CommandBuffer` 从 desc-only 对象变成可录制、可关闭、可提交 replay 的最小命令容器。

## 本阶段范围

- `CommandBuffer` 新增生命周期 API：
  - `Begin()`
  - `End()`
  - `Reset()`
  - `IsRecording()`
  - `IsExecutable()`
- `CommandBuffer` 新增最小 recorded command API：
  - `RecordBeginRenderPass()`
  - `RecordEndRenderPass()`
  - `RecordSetPipelineState()`
  - `RecordSetVertexBuffer()`
  - `RecordSetIndexBuffer()`
  - `RecordSetBindGroup()`
  - `RecordDrawIndexed()`
- `RenderQueue::Submit()` 从 `void` 改为 `bool`，用于反馈 submit 是否接受。
- OpenGL backend 先将 recorded commands replay 到 immediate command list。
- 保留 immediate command list 现有路径不变。

## 非目标

- 不做 fence/timeline。
- 不做 secondary command buffer。
- 不做多线程 recording。
- 不做 command allocator/reset pool。
- 不迁移 Forward 主路径到 recorded command buffer。

## RED 测试

在 `Tests/RHICommandListBindingSmoke.cpp` 中增加：

- recording 状态下的 command buffer submit 必须失败。
- `End()` 后的 command buffer submit 必须成功。
- submit 后 render target 中心像素应变成 fragment shader 颜色。
- `Reset()` 后 command buffer 回到非 recording、非 executable 状态。

## 实现步骤

1. 修改 `CommandSubmission.h`，增加 `CommandBuffer` 生命周期与录制接口。
2. 修改 `RenderQueue::Submit()` 返回 bool。
3. 修改 `OpenGLCommandBuffer`，保存 recorded command vector 和 recording/executable 状态。
4. 修改 `OpenGLRenderQueue::Submit()`，校验 usage/executable，并 replay recorded command。
5. 更新 smoke 中旧 `Submit()` 调用。
6. 跑四个 smoke 和 `git diff --check`。

## 验收标准

- `RHICommandListBindingSmoke` 覆盖 recorded command buffer draw。
- 未 `End()` 的 command buffer submit 返回 false。
- 已 `End()` 的 command buffer submit 返回 true 且有像素输出。
- `Reset()` 清空 recorded command 并清除 executable 状态。
- 四个 smoke 通过。
- P39 单独提交。

## 执行结果

- `CommandBuffer` 已新增 `Begin()`、`End()`、`Reset()`、`IsRecording()`、`IsExecutable()` 生命周期 API。
- `CommandBuffer` 已新增最小 recorded render command API，覆盖 render pass、pipeline、vertex/index、bind group、draw。
- `RenderQueue::Submit()` 已改为返回 `bool`。
- `OpenGLCommandBuffer` 已保存 recorded command stream，并能 replay 到 immediate command list。
- `OpenGLRenderQueue` 已拒绝 recording / non-executable / incompatible command buffer。
- `RHICommandListBindingSmoke` 已覆盖 unfinished submit 失败、ended submit 成功、recorded draw 像素输出、reset 清状态。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
