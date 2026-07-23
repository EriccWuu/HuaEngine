# P51：PassGraph Typed Resource Handle Binding 实现计划

## 目标

为有 runtime RHI 资源的 pass 引入 `RenderGraphResourceHandle` input/output 声明，并保持字符串图节点兼容。

## 实现步骤

1. 扩展 `PassGraphPassDesc`，新增 `InputResources` 与 `OutputResources` handle 列表。
2. 在 graph compile 时解析 handle 为资源名称，复用现有依赖、barrier 与 lifetime 推导。
3. 为无效 handle 增加 `InvalidResourceHandle` diagnostic。
4. 将 P50 attachment sampling graph 迁移到 typed handle 声明和 callback 查询，并在 graph smoke 覆盖无效 handle。

## 验收命令

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RenderPassGraphSmoke
& .\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
git diff --check
```
