# P43 Pipeline Render State Backend Apply 计划

状态：已完成
日期：2026-07-16
基线：`0d7b5f7 feat(rendering): add texture view sampler api`

## 目标

让 `PipelineStateDesc` 中已经声明的 raster/depth/blend render state 真正在 OpenGL backend 生效，并在 draw 前校验当前 render target 与 pipeline target contract。

## 本阶段范围

- `OpenGLCommandList::SetPipelineState()` 应用 raster state：
  - `CullMode`
  - `FrontFace`
  - `FillMode`
- `OpenGLCommandList::SetPipelineState()` 应用 depth/stencil state 的 depth 部分：
  - depth test enable
  - depth write mask
  - depth compare
- `OpenGLCommandList::SetPipelineState()` 应用 color target state：
  - blend enable
  - blend func/op
  - color write mask
- `OpenGLCommandList::DrawIndexed()` 在提交 draw 前校验：
  - 当前 render target color attachment format 与 pipeline color target format 一致
  - 当前 render target depth/stencil attachment 与 pipeline depth/stencil format 兼容
- smoke 覆盖至少一个可观察 backend 状态，以及 format mismatch 不产生 draw。

## 非目标

- 不实现多 color attachment 独立 blend state。
- 不实现 stencil state。
- 不扩展 `PrimitiveTopology`。
- 不改变 Forward 主路径的 pipeline cache key。

## RED 测试

在 `Tests/RHICommandListBindingSmoke.cpp` 中：

- 创建显式 render state pipeline，调用 `SetPipelineState()` 后用 `glGet*` 验证：
  - cull disabled
  - depth test disabled
  - depth write disabled
  - blend enabled
  - color write mask 仅 red/alpha enabled
- 创建 color target format 与 render target 不一致的 pipeline，绑定后 draw，验证 render target 仍保持 clear color。

## 验收标准

- `SetPipelineState()` 后 OpenGL backend state 与 pipeline desc 一致。
- format mismatch pipeline draw 被拒绝，不污染当前 render target。
- 既有 bind group、recorded command buffer、resource creation、render graph smoke 继续通过。
- P43 单独提交。

## 执行结果

- `OpenGLCommandList::SetPipelineState()` 已应用 cull/front face/fill、depth test/write/compare、blend func/op 与 color write mask。
- `OpenGLCommandList::DrawIndexed()` 已在 draw 前校验当前 render target color/depth-stencil format 与 pipeline contract。
- render pass clear 与 `ClearColor()` 已显式恢复 color/depth write mask，避免 pipeline color/depth mask 泄漏影响 clear。
- Forward `BindTargetPass` 已绑定 depth/stencil attachment 并按 clear policy 清 depth，保证默认 depth pipeline state 生效后主路径仍能绘制。
- `RHICommandListBindingSmoke` 已覆盖 backend GL state query 和 color target format mismatch skip draw。

## 验证结果

- `RenderingOperationsSmoke` passed
- `RHICommandListBindingSmoke` passed
- `RHIResourceCreationSmoke` passed
- `RenderPassGraphSmoke` passed
- `git diff --check` passed
