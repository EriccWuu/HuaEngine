# P53：PassGraph Explicit Resource Usage 实现计划

## 目标

以显式 typed resource state 声明驱动 PassGraph barrier 计划，消除 Forward 对多 writer 推断规则的依赖。

## 实现步骤

1. 增加 `PassGraphResourceUsage` 和 `ResourceUsages` pass 字段，包含 resource handle 与目标 state。
2. 在 compile 中校验 handle/state，并将 explicit usage 接入 availability、lifetime、barrier 与 output 统计。
3. 将 Forward viewport attachment 的三个写入 pass 迁移到 `RenderTarget` usage。
4. 在 graph smoke 验证 explicit write/read barrier 与无效 usage diagnostic。

## 验收命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RenderPassGraphSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
git diff --check
```
