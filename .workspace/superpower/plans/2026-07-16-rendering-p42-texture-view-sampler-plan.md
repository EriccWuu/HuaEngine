# P42 TextureView / Sampler RHI API 计划

状态：已完成
日期：2026-07-16
基线：`034e3d4 feat(rendering): track graph resource states`

## 目标

让 texture view 和 sampler 成为一等 RHI 对象，并允许 bind group 绑定 texture view + sampler。

## 本阶段范围

- 新增 `TextureView` 和 `Sampler` RHI class。
- `RenderDevice` 新增 `CreateTextureView()` 和 `CreateSampler()`。
- OpenGL backend 新增 `OpenGLTextureView` 和 `OpenGLSampler`。
- `BindingValueType` 新增 `TextureView` 与 `Sampler`。
- `BindingValue` 支持 `Ref<TextureView>` 与 `Ref<Sampler>`。
- OpenGL `SetBindGroup()` 支持绑定 texture view 与 sampler。
- 保留旧 `Ref<TextureResource>` texture binding。

## 非目标

- 不实现 OpenGL `glTextureView` 分离 storage view。
- 不改 material serializer。
- 不迁移现有 material texture path。
- 不做 shader reflection。

## RED 测试

在 `Tests/RHIResourceCreationSmoke.cpp` 中：

- 创建 texture view，验证 desc round-trip。
- 创建 sampler，验证 desc round-trip。
- 创建含 texture view + sampler 的 bind group layout 和 bind group。
- 验证空 texture view / sampler desc 被拒绝。

## 验收标准

- `CreateTextureView()` 成功创建 sampled texture view。
- `CreateSampler()` 成功创建 sampler。
- bind group 能携带 texture view + sampler entries。
- 旧 texture resource binding 继续通过。
- 四个 smoke 通过。
- P42 单独提交。

## 执行结果

- 新增 `TextureView` 与 `Sampler` RHI 对象，并在 `RenderDevice` 暴露 `CreateTextureView()` 与 `CreateSampler()`。
- OpenGL backend 新增 `OpenGLTextureView` 与 `OpenGLSampler`，sampler state 映射到 GL sampler object。
- `BindingValueType` 与 `BindingValue` 已支持 texture view 与 sampler，同时保留旧 `TextureResource` binding。
- OpenGL `SetBindGroup()` 已支持绑定 texture view 与 sampler；texture view 当前复用底层 texture storage，不引入 `glTextureView`。
- `RHIResourceCreationSmoke` 已覆盖 texture view 创建、sampler 创建、texture view + sampler bind group，以及空 texture view desc 拒绝路径。

## 验证结果

- `RenderingOperationsSmoke` passed
- `RHICommandListBindingSmoke` passed
- `RHIResourceCreationSmoke` passed
- `RenderPassGraphSmoke` passed
- `git diff --check` passed
