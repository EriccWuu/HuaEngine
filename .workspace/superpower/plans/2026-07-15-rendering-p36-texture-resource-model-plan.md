# P36 Texture / RenderTarget Resource Model 计划

状态：执行中
日期：2026-07-15

## 目标

把 `TextureResource` 从单一 file-backed 贴图扩展为可描述 GPU texture resource 的最小模型，为后续 RenderGraph transient texture 和 render target attachment 拆分做准备。

## 本阶段范围

- 扩展 `TextureDesc`：
  - `Width`
  - `Height`
  - `Format`
  - `Usage`
  - `MipLevels`
  - `Samples`
  - 保留 `SourcePath`
- 新增 `TextureUsage` flags。
- 新增轻量 `TextureViewDesc` / `SamplerDesc` 数据结构，但本阶段不增加 device create API。
- OpenGL `CreateTexture` 支持两条路径：
  - `SourcePath` 非空：沿用图片加载，补齐 desc 中的 width/height/format/usage。
  - `SourcePath` 为空：按 desc 创建空 GPU texture storage。
- `RHIResourceCreationSmoke` 覆盖：
  - 旧 file-backed texture 继续可用。
  - 新 non-file-backed texture 可创建并 round-trip desc。
  - 非法 empty texture desc 被拒绝。

## 非目标

- 不拆 `RenderTarget` 为 TextureView/framebuffer 聚合。
- 不引入 sampler 对象创建 API。
- 不实现 texture view 对象创建 API。
- 不实现 mip generation。
- 不实现 GPU copy/upload command。

## 验收标准

- `CreateTexture({ .Width = 32, .Height = 16, .Format = RGBA8, .Usage = Sampled | CopyDst })` 成功。
- 返回的 `TextureResource::GetDesc()` 包含 width/height/format/usage/mip/sample 信息。
- `CreateTexture({})` 和缺少尺寸/format/usage 的 empty texture desc 失败。
- 旧 `CreateTexture({ .SourcePath = ... })` smoke 继续通过。
- 四个 smoke 通过，提交为单独 P36 commit。

## 执行结果

- `TextureDesc` 已新增 `Width`、`Height`、`Format`、`Usage`、`MipLevels`、`Samples`，并保留 `SourcePath`。
- 新增 `TextureUsageFlags` 和 `TextureUsageSampled` / `ColorAttachment` / `DepthStencilAttachment` / `CopySrc` / `CopyDst` flags。
- 新增轻量 `TextureViewDesc` 与 `SamplerDesc` 数据结构，为后续 view/sampler API 做类型铺垫。
- OpenGL `CreateTexture` 已支持 file-backed 与 non-file-backed 两条路径。
- file-backed texture 会补齐 desc 中的 width/height/format/usage/mip/sample 信息。
- non-file-backed texture 会按 desc 分配 OpenGL texture storage。
- `RHIResourceCreationSmoke` 已覆盖旧路径、新路径和非法 desc 拒绝。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
