# P50：Graph Pass Attachment Sampling Draw 实现计划

## 目标

通过真实 OpenGL draw 验证 graph reader pass 可采样前一 pass 写入的 RenderTarget attachment。

## 实现步骤

1. 在 command list smoke 中创建 source 与 destination RGBA8 render target。
2. 建立只含 sampler2D 的 sampling shader、texture view/sampler bind group layout 和 pipeline。
3. 建图：writer 将 source attachment 作为 output 清除；reader 将其作为 input，从 `GraphResources` 创建 view/bind group，并 draw 到 destination attachment。
4. readback destination center pixel，验证它接近 source clear color，并检查 source 最终为 `ShaderRead`。

## 验收命令

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
& .\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
git diff --check
```
