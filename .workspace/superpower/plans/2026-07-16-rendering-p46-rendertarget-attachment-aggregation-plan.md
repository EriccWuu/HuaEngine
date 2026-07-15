# P46 RenderTarget Attachment Aggregation 计划

状态：已完成
日期：2026-07-16
基线：`93ea5a4 feat(rendering): add queue timeline fence`

## 目标

开始拆分 `RenderTarget` 的 monolithic 语义，让 color/depth attachment 能以稳定 view metadata 暴露，而不是只有 framebuffer wrapper 行为。

## 本阶段范围

- 扩展 attachment view metadata：
  - native handle
  - format
  - width/height
  - samples
  - attachment index
- `RenderTarget` 新增 depth/stencil attachment view accessor。
- OpenGL storage 暴露 depth attachment native handle。
- smoke 覆盖 color/depth attachment view metadata。

## 非目标

- 不把 framebuffer attachment 直接包装成拥有型 `TextureResource`。
- 不实现 OpenGL external texture ownership wrapper。
- 不改 render pass desc 到 attachment view 输入。
- 不迁移 editor viewport/readback 路径。

## RED 测试

在 `RHIResourceCreationSmoke` 中：

- `GetColorAttachmentView(0)` 返回非零 native handle、RGBA8 format、target size 与 samples。
- `GetDepthStencilAttachmentView()` 返回非零 native handle、DEPTH24_STENCIL8 format、target size 与 samples。

## 验收标准

- render target color attachment 能暴露稳定 view metadata。
- render target depth/stencil attachment 能暴露稳定 view metadata。
- readback smoke 继续通过。
- 既有四个 smoke 通过。
- P46 单独提交。

## 执行结果

- `RenderTargetColorAttachmentView` 已扩展 native handle、format、width、height、samples、attachment index metadata。
- `RenderTarget` 已新增 `GetDepthStencilAttachmentView()`。
- OpenGL render target storage 已暴露 depth attachment native handle。
- OpenGL render target color/depth view accessor 已返回 attachment metadata。
- `RHIResourceCreationSmoke` 已覆盖 color/depth attachment view metadata 与 native handle。

## 验证结果

- `RenderingOperationsSmoke` passed
- `RHICommandListBindingSmoke` passed
- `RHIResourceCreationSmoke` passed
- `RenderPassGraphSmoke` passed
- `git diff --check` passed
