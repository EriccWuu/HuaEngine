# P49：跨 Pass Attachment 读写与状态迁移实现计划

## 目标

让 graph pass 回调可以查询当前 runtime resource，并用 render target color attachment 验证从输出写入到 sampled 输入读取的资源身份与状态迁移。

## 实现步骤

1. 在 `RenderPassContext` 中暴露只读 `RenderGraphResourceAllocator` 指针。
2. `PassGraph::Execute()` 在 pass 执行期间注入 allocator，并在结束后恢复调用方 context。
3. 在 `RHIResourceCreationSmoke` 建立 imported attachment graph：writer 输出 attachment，reader 输入 attachment。
4. 验证 writer 使用 attachment view、reader 从 runtime texture 创建 sampled view，以及 barrier 序列和状态追踪结果。

## 验收命令

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
& .\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
git diff --check
```
