# P48：RenderPass Attachment View Contract 实现计划

## 目标

将 render pass 的 attachment 表达切换为 `TextureView`，让 RHI 资源身份而非 `RenderTarget + index` 成为主契约，同时保持旧调用点可渐进迁移。

## 实现步骤

1. 扩展 `RenderPass` attachment 描述，新增 color/depth `View` 字段，保留 legacy target 字段。
2. 为 OpenGL attachment texture 暴露其 framebuffer storage 的非拥有访问，并在 command list 中从 view 解析 storage、format 与兼容性。
3. 将 OpenGL render pass begin/end 和 pipeline target validation 改为基于已解析的 attachment state。
4. 将 Forward 主路径迁移到 attachment view；扩展 command list smoke 覆盖没有 legacy target 的 view-only pass。

## 验收命令

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
& .\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
git diff --check
```
