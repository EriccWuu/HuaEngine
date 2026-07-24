# P57：Graph CommandBuffer 录制与队列提交

## 目标

把 Forward 图从 immediate command list 迁移为 graphics command buffer 的录制和 queue 提交，并对外暴露 timeline 完成信息。

## 实施步骤

1. 扩展 `CommandBuffer` 录制接口，使其覆盖 resource barrier、BeginFrame、EndFrame，并让 OpenGL backend 回放这些命令。
2. 添加 `CommandBuffer` 到 `CommandList` 的录制适配器，记录失败状态并拒绝未支持的遗留命令。
3. 在 Forward 渲染时创建、开始、结束并提交 graphics command buffer；保留该帧 pipeline/bind group 的引用直至提交。
4. 将 queue signal/completed value 写入渲染统计和 operation payload，补充冒烟断言。

## 验证命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RHICommandListBindingSmoke RHIResourceCreationSmoke RenderPassGraphSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
```
