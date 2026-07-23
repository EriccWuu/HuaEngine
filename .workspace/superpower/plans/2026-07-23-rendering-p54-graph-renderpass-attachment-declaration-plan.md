# P54：Graph RenderPass Attachment Declaration 实现计划

## 目标

以 graph 层 attachment 声明表达 render pass 输出，自动生成 resource usage，减少 Forward 手写状态。

## 实现步骤

1. 在 `PassGraphPassDesc` 增加 color/depth-stencil attachment handle 声明。
2. 编译时将 attachment 声明合并为 `RenderTarget` explicit usage。
3. Forward 导入 viewport color/depth texture，并将 Bind/Clear/Opaque 迁移到 attachment 声明。
4. 更新 graph/rendering smoke，验证自动 barrier 和 Forward 图统计。

## 验收命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RenderPassGraphSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
git diff --check
```
