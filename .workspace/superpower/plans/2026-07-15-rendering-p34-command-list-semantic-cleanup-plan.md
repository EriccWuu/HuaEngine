# P34 CommandList 高层语义清理计划

状态：执行中
日期：2026-07-15

## 目标

清理 RHI `CommandList` 中的高层 renderer 语义：

- `CommandList` 不再依赖 `Camera` 类型。
- `BeginFrame` 不再接收 renderer/camera 上下文。
- OpenGL `DrawIndexed` 不再硬编码 frame/object bind group。
- Draw 前置条件改为检查当前 `PipelineState` 声明的 bind group slot 是否已绑定。
- Forward 主路径继续负责 slot 0/1/2 的语义分配。

## 验收标准

- `CommandList.h` 不包含 `Camera` 依赖。
- OpenGL command list 不包含 `m_HasFrameBindGroup` / `m_HasObjectBindGroup`。
- OpenGL draw gating 只依赖 pipeline slot contract，而不是 `BindGroupScope::Frame/Object`。
- `RHICommandListBindingSmoke` 覆盖结构约束和绑定缺失场景。
- `RenderingOperationsSmoke`、`RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderPassGraphSmoke` 通过。

## 执行步骤

1. 增加 RED 测试，读取核心源文件并断言 RHI 边界不再包含高层 Camera/Frame/Object 特化状态。
2. 修改 `CommandList` 接口：`BeginFrame(Camera&)` -> `BeginFrame()`，移除 `Camera` forward declaration。
3. 修改 OpenGL command list：删除 camera 指针和 frame/object bool，改为记录已绑定 slot。
4. 修改 `DrawIndexed`：遍历当前 pipeline 声明的 bind group slots，缺失任一 slot 时跳过 draw。
5. 更新 Forward 和 smoke 调用点。
6. 运行 smoke 与 diff whitespace 校验。

## 执行结果

- `CommandList` 已移除 `Camera` forward declaration，`BeginFrame` 改为无参接口。
- `OpenGLCommandList` 已删除 `m_CurrentCamera`、`m_HasFrameBindGroup`、`m_HasObjectBindGroup`。
- `OpenGLCommandList::SetBindGroup` 现在记录已成功绑定的 pipeline slot。
- `OpenGLCommandList::DrawIndexed` 现在按当前 `PipelineStateDesc::BindGroupLayouts` 检查 required slot 是否已绑定。
- `ForwardRenderPipeline` 已更新为无参 `BeginFrame()` 调用。
- `RHICommandListBindingSmoke` 增加结构约束，防止 `CommandList` 重新引入 camera 和 OpenGL frame/object 特化 gating。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
