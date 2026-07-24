# P56：Graph 托管 RenderPass 生命周期

## 目标

由 `PassGraph::Execute()` 负责带附件 pass 的 render-pass 开始与结束，Forward 只保留实际的帧和绘制阶段。

## 实施步骤

1. 在 graph 执行期完成 runtime `RenderPassDesc` 构造后，由命令列表自动包围 callback。
2. 将附件图冒烟测试改为记录并验证自动 begin/end 以及生成的 clear 参数。
3. 删除 Forward 的 BindTarget、ClearTarget、UnbindTarget pass，将 color/depth attachment 收敛至 ForwardOpaque。
4. 更新 Forward 图统计断言，编译并执行完整 RHI/Rendering 冒烟集。

## 验证命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RHIResourceCreationSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
```
