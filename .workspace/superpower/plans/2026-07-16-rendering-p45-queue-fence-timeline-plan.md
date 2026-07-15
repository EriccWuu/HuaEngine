# P45 Queue Fence / Timeline Skeleton 计划

状态：已完成
日期：2026-07-16
基线：`4a53e3e feat(rendering): stabilize material binding schema`

## 目标

让 graphics queue submit 返回最小同步结果，并提供可查询 completed value 的 fence/timeline 骨架。

## 本阶段范围

- 新增 `Fence` RHI interface。
- 新增 `QueueSubmitDesc` 与 `QueueSubmitResult`。
- `RenderQueue::Submit(CommandBuffer&)` 返回 `QueueSubmitResult`。
- `QueueSubmitResult` 提供 `operator bool()`，兼容既有 submit success/failure 判断。
- OpenGL backend 使用 CPU-side monotonically increasing signal value 模拟 timeline。
- invalid command buffer submit 返回失败结果与 signal value 0。

## 非目标

- 不引入 GPU fence object。
- 不实现 wait-before-submit。
- 不实现多 queue。
- 不改变 immediate command list 路径。

## RED 测试

在 `RHICommandListBindingSmoke` 中：

- invalid/recording command buffer submit 返回失败结果。
- executable command buffer submit 返回成功结果。
- 连续 submit 返回递增 signal value。
- queue fence completed value 等于最新 signal value。

## 验收标准

- submit result 能表达 success/failure。
- submit success signal value 单调递增。
- completed value 可查询。
- 既有四个 smoke 通过。
- P45 单独提交。

## 执行结果

- 新增 `Fence`、`QueueSubmitDesc` 与 `QueueSubmitResult`。
- `RenderQueue::Submit(CommandBuffer&)` 已返回 `QueueSubmitResult`，并通过隐式 `operator bool()` 保持旧 success/failure 判断兼容。
- `RenderQueue` 已提供 `Submit(const QueueSubmitDesc&)` convenience path 和 `GetTimelineFence()`。
- OpenGL graphics queue 已维护 CPU-side timeline fence，成功 submit 递增 signal value 并更新 completed value。
- invalid/recording/non-executable submit 返回失败结果，signal value 为 0。
- `RHICommandListBindingSmoke` 已覆盖失败 submit、成功 submit、连续 submit signal 递增，以及 fence completed value。

## 验证结果

- `RenderingOperationsSmoke` passed
- `RHICommandListBindingSmoke` passed
- `RHIResourceCreationSmoke` passed
- `RenderPassGraphSmoke` passed
- `git diff --check` passed
