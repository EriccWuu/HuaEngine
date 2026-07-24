# P55：Graph Runtime RenderPassDesc 实现计划

## 目标

将 PassGraph 的 attachment handle 声明在 execute 期映射为实际 `TextureView` 与 `RenderPassDesc`，让 pass callback 使用 graph 生成的 RHI pass 契约。

## 实现步骤

1. 扩展 graph attachment 声明的 load/store/clear 参数，并在 `RenderPassContext` 中新增短生命周期 `GraphRenderPass`。
2. `PassGraph::Execute()` 解析 runtime texture、创建 view、构造 color/depth attachment render pass desc，并在 callback 前后设置/恢复 context。
3. Forward BindTargetPass 直接消费 `GraphRenderPass`；BuildGraph 将 view clear 设置写入 attachment 声明。
4. P49 resource smoke 的 writer pass 改用生成 desc，验证 view source 与 context 恢复。

## 验收命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RHIResourceCreationSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
git diff --check
```
