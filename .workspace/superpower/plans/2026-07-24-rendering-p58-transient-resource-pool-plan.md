# P58：Transient Resource Pool 与生命周期复用

## 目标

让 render graph transient texture 的 lifetime 驱动物理纹理别名与跨帧复用，并以 graphics queue fence 确保复用安全。

## 实施步骤

1. 在 render graph allocator 中持久化 transient texture pool，按尺寸和格式匹配可用纹理。
2. 准备 runtime resources 时按 lifetime 顺序分配；不重叠的资源共享一个 pool entry。
3. 成功提交后登记 active pool entry 的 fence signal value；下一次准备仅选择已完成的 entry。
4. 在真实 OpenGL device 冒烟中验证同帧别名、未完成 fence 阻止复用、完成 fence 后复用。

## 验证命令

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke RenderingOperationsSmoke RHICommandListBindingSmoke RenderPassGraphSmoke
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
```
