# P47：RenderTarget Attachment-backed TextureResource 实现计划

## 目标

使 RenderTarget 的 color 与 depth/stencil attachment 成为具有稳定 RHI 身份的 `TextureResource` 和默认 `TextureView`，为后续 RenderPass view contract 与跨 pass 采样提供资源基础。

## 实现步骤

1. 扩展 `RenderTarget` 接口，公开 color/depth attachment 的 texture 与默认 texture view，同时保留 P46 metadata accessor。
2. 为 OpenGL texture resource 增加 attachment-storage 借用模式：storage 保留 GL texture 所有权，resource 每次绑定时从 storage 取得当前 handle。
3. `OpenGLRenderTarget` 根据 specification 创建 attachment texture resource 和 view；resize 后同步 texture desc 并保持资源对象身份稳定。
4. 扩展 `RHIResourceCreationSmoke`，覆盖 attachment texture/view 元数据、bind group 与 resize 后绑定。

## 验收命令

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
& .\build\bin\Debug-Windows-x86_64\RHIResourceCreationSmoke\RHIResourceCreationSmoke.exe
git diff --check
```
