# P52：Forward Typed Viewport Attachment 实现计划

## 目标

把 Forward renderer 的 viewport color attachment 从字符串中间节点迁移为每帧 imported `RenderGraphResourceHandle`，让主路径接入现代 RHI resource state 语义。

## 实现步骤

1. 支持同一 typed resource 的顺序多 writer，遗留字符串资源保持重复 writer diagnostic。
2. `ForwardRenderPipeline::BuildGraph()` 接收当前 view，导入 color attachment texture 并用 handle 构建 Bind/Clear/Opaque pass。
3. 每次 render 重建图并把 device/resource state tracker 写入 pass context。
4. 更新 rendering smoke 的 graph stats，并验证 source 中不再声明旧 viewport color 字符串节点。

## 验收命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RenderPassGraphSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
git diff --check
```
