# 渲染 RHI 兼容接口退场设计 Spec

状态：已实现  
日期：2026-07-14  
范围：P23-P26 完成后的 RHI 兼容接口清理  

## 1. 背景

`2026-07-08-rendering-modern-rhi-next-stage-design.md` 中的 P23-P26 已完成：

- P23：新增 `PipelineState`，Forward 主路径已使用 `CommandList::SetPipelineState`。
- P24：新增 `BindGroupLayout` / `BindGroup`，RHI smoke 已使用 `SetBindGroup` 覆盖 frame/material/object，Forward material 主路径已使用 material bind group。
- P25：新增 typed render graph resource、imported/transient resource、resource lifetime 与 invalid resource desc diagnostic。
- P26：新增 `RenderBackendType`、`RenderDeviceDesc`、capabilities、backend factory；公共 RHI 已移除 `TextureResource::GetRenderID` 与 `RenderTarget::GetRenderID/GetColorAttachment`。

当前剩余问题不是“缺现代 RHI 基础抽象”，而是兼容接口仍暴露在公共 RHI：

- `CommandList::SetShaderProgram` 仍存在，虽然 Forward 主路径已不使用。
- `CommandList::SetFrameBinding`、`SetMaterialBinding`、`SetObjectBinding` 仍存在，虽然新路径应使用 `SetBindGroup`。
- `ShaderProgram` 公共接口仍暴露 `SetInt/SetFloat/SetMat4...` 这类 OpenGL uniform setter。
- `FrameBinding` / `ObjectBinding` 仍通过 `FrameObjectBinding.h` 被 `CommandList.h` include。
- `MaterialBinding` 仍是 `CommandList` 的公共依赖，并且 `RenderResourceResolver` 先构建 `MaterialBinding` 再转换成 material `BindGroup`。

本阶段目标是把这些“迁移期兼容接口”从公共 RHI 中退场，让 draw 提交流程只依赖现代路径：

```text
PipelineState + VertexBufferView + BindGroup + Draw
```

## 2. 当前代码形状

### 2.1 CommandList

当前 `CommandList` 暴露：

- `SetShaderProgram(ShaderProgram&)`：兼容路径，期望由 `SetPipelineState` 替代。
- `SetPipelineState(PipelineState&)`：当前 draw 主路径。
- `SetVertexBufferView(VertexBufferView&)`：仍保留。
- `SetBindGroup(uint32_t slot, BindGroup&)`：当前现代资源绑定入口。
- `SetFrameBinding(const FrameBinding&)`：兼容路径，期望由 frame bind group 替代。
- `SetMaterialBinding(const MaterialBinding&)`：兼容路径，期望由 material bind group 替代。
- `SetObjectBinding(const ObjectBinding&)`：兼容路径，期望由 object bind group 替代。
- `DrawIndexed(uint32_t)`。

### 2.2 ShaderProgram

当前 `ShaderProgram` 公共接口仍包含 uniform setter：

- `SetInt`
- `SetIntArray`
- `SetFloat`
- `SetFloat2`
- `SetFloat3`
- `SetFloat4`
- `SetMat3`
- `SetMat4`

这些是 OpenGL 迁移期接口。现代 RHI 公共层不应允许上层直接按字符串写 shader uniform。

### 2.3 Forward 渲染路径

当前 `ForwardOpaquePass` 已使用：

- `SetPipelineState`
- material `SetBindGroup(1, ...)`

但仍使用：

- `SetFrameBinding`
- `SetObjectBinding`

因此 frame/object 仍未完成 bind group 化。

### 2.4 Material 绑定路径

当前 `RenderResourceResolver` 的 material 资源构建路径是：

```text
MaterialInstance -> MaterialBinding -> BindGroup
```

这能工作，但长期会让 `MaterialBinding` 继续作为中间旧模型存在。更干净的路径应是：

```text
MaterialInstance -> BindGroup
```

## 3. 目标

### 3.1 公共 CommandList 只保留现代提交接口

最终 `CommandList` 应保留：

- `BeginRenderTarget`
- `ClearColor`
- `BeginFrame`
- `SetPipelineState`
- `SetVertexBufferView`
- `SetBindGroup`
- `DrawIndexed`
- `EndFrame`
- `EndRenderTarget`

并移除：

- `SetShaderProgram`
- `SetFrameBinding`
- `SetMaterialBinding`
- `SetObjectBinding`

说明：

- `BeginRenderTarget` / `EndRenderTarget` 仍偏旧式 render pass 语义，但它属于后续 RenderPassDesc/RenderGraph pass 方向，不纳入本阶段。
- `SetVertexBufferView` 仍保留，因为 vertex buffer view 已是当前 RHI 的 buffer binding 抽象；后续可进一步拆成 vertex/index buffer view 与 pipeline input layout。

### 3.2 Frame/Object 迁移到 BindGroup

新增或复用 helper，将 frame/object 参数构造成 bind group：

- frame bind group：
  - scope：`BindGroupScope::Frame`
  - slot：`0`
  - entry：`u_ViewProjection` / `Mat4`
- material bind group：
  - scope：`BindGroupScope::Material`
  - slot：`1`
  - entries：来自 material scalar/texture 参数
- object bind group：
  - scope：`BindGroupScope::Object`
  - slot：`2`
  - entry：`u_Transform` / `Mat4`

Forward draw 目标调用顺序：

```cpp
commands.SetPipelineState(*pipeline);
commands.SetBindGroup(0, *frameBindGroup);
commands.SetVertexBufferView(*vertexBufferView);
commands.SetBindGroup(1, *materialBindGroup);
commands.SetBindGroup(2, *objectBindGroup);
commands.DrawIndexed(indexCount);
```

### 3.3 移除 ShaderProgram 公共 uniform setter

`ShaderProgram` 公共接口应只保留：

- destructor
- `GetDesc()`

OpenGL 后端仍需要设置 uniform，但应变为后端内部能力：

- `OpenGLShaderProgram` 可以继续保留 `SetInt/SetFloat/SetMat4...` 作为后端实现方法。
- 这些方法不再 override `ShaderProgram`，不再出现在公共 RHI 抽象中。
- `OpenGLCommandList::SetBindGroup` 可以通过 `OpenGLShaderProgram&` 调用后端内部 setter。

### 3.4 MaterialBinding 退场

优先级：

1. 从 `CommandList` 公共接口移除 `SetMaterialBinding`。
2. 从 OpenGL command list 移除 `SetMaterialBinding`。
3. 将 `RenderResourceResolver` 改为直接从 `MaterialInstance` 构造 material `BindGroup`。
4. 如果 `MaterialBinding.h` 已无调用方，则删除该文件；如果仍有上层调试或过渡用途，则保留但不得被 RHI command 依赖。

本阶段倾向于删除 `MaterialBinding.h`，除非代码勘探发现仍有合理上层用途。

### 3.5 保持运行时行为不变

本阶段是接口清理，不应改变以下行为：

- material scalar 参数仍能驱动 shader uniform。
- material texture 参数仍能绑定到 OpenGL texture slot。
- frame `u_ViewProjection` 与 object `u_Transform` 仍能正确提交。
- RHI smoke 和 rendering smoke 的像素/统计行为不回退。
- Asset/material 序列化不应变化。

## 4. 非目标

本阶段不做：

- 不引入 shader reflection。
- 不实现 descriptor heap / descriptor set allocator。
- 不引入 sampler state。
- 不重写 RenderGraph pass 执行模型。
- 不移除 `BeginRenderTarget` / `EndRenderTarget`。
- 不实现 Vulkan/D3D/Metal。
- 不把 `TextureDesc` / `GpuBufferDesc` 完整现代化。
- 不做 pipeline state cache。

## 5. 设计方案

### 5.1 BindGroup 构建 helper

当前 bind group 构建逻辑分散在测试和 `RenderResourceResolver` 内。为了清理 frame/object/material 旧接口，应引入小型 helper。

推荐位置：

```text
HuaEngine/src/HuaEngine/Rendering/RHI/BindGroupBuilder.h
HuaEngine/src/HuaEngine/Rendering/RHI/BindGroupBuilder.cpp
```

职责：

- 创建 frame bind group。
- 创建 object bind group。
- 创建 material bind group。
- 将 `MaterialParameterType` 映射到 `BindingValueType`。
- 跳过暂不支持的 float array。
- texture 参数转换成 `BindingValueType::Texture`。

接口建议：

```cpp
Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection);
Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform);
Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance);
```

如果不希望 RHI 目录依赖 Material 模块，则可以放到：

```text
HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h/.cpp
```

推荐选择 `RenderPipeline/RenderBindGroupBuilder`，原因：

- frame/object/material 都是 renderer 级语义，不是底层 RHI 核心语义。
- 避免 `RHI` 目录反向 include `MaterialCore.h`。
- 保持 RHI 层只定义 bind group 类型和创建接口。

### 5.2 ResolvedRenderItem 扩展

当前 `ResolvedRenderItem` 已有：

- `PipelineStateRef`
- `MaterialBindGroupRef`

需要新增：

- `ObjectBindGroupRef` 可在 pass 内按 item transform 创建，也可 resolver 创建。

不建议 resolver 创建 object bind group，原因：

- object transform 来自 `RenderItem`，每帧每对象变化。
- object bind group 更接近 draw submission 数据，放在 `ForwardOpaquePass` 内构建更直接。

frame bind group 建议在 pass 开始前创建一次，而不是每个 item 创建。

### 5.3 ForwardOpaquePass 迁移策略

当前 pass 内每个 item 调用：

```cpp
SetPipelineState(...)
SetFrameBinding(...)
SetVertexBufferView(...)
SetBindGroup(1, material)
SetObjectBinding(...)
DrawIndexed(...)
```

目标改为：

```cpp
auto frameBindGroup = CreateFrameBindGroup(device, camera.GetViewProjection());

for item:
  auto objectBindGroup = CreateObjectBindGroup(device, item.Transform);
  SetPipelineState(...)
  SetBindGroup(0, frame)
  SetVertexBufferView(...)
  SetBindGroup(1, material)
  SetBindGroup(2, object)
  DrawIndexed(...)
```

短期性能接受：

- 当前 pipeline state/material bind group 已经存在 per item/per resolve 创建成本。
- 本阶段重点是接口退场；缓存可在后续阶段处理。

后续优化：

- frame bind group 每帧缓存。
- material bind group 按 material instance / override hash 缓存。
- object bind group 使用动态 uniform buffer 或 ring buffer。

### 5.4 OpenGL 后端调整

`OpenGLCommandList` 应移除：

- `SetShaderProgram`
- `SetFrameBinding`
- `SetMaterialBinding`
- `SetObjectBinding`
- `FrameBinding m_CurrentFrameBinding`
- `ObjectBinding m_CurrentObjectBinding`

保留：

- `m_CurrentPipelineState`
- `m_CurrentShaderProgram`
- `m_CurrentVertexBufferView`
- `m_HasFrameBinding`
- `m_HasObjectBinding`

说明：

- `m_HasFrameBinding` / `m_HasObjectBinding` 可以继续用于 draw 前校验，但它们应只由 `SetBindGroup` 根据 scope 设置。
- `SetPipelineState` 不再 replay old frame/object binding，因为旧 binding 已不存在。
- 如果先提交 frame/object bind group，再提交 pipeline state，uniform 会找不到 current shader；因此 draw path 应要求 pipeline 先绑定。
- RHI smoke 应覆盖 pipeline first 的顺序。

### 5.5 ShaderProgram 后端化

`ShaderProgram` 公共头移除 uniform setter 后：

- `OpenGLShaderProgram` 继续拥有 setter 方法，但不 `override`。
- `OpenGLCommandList` 通过 `OpenGLShaderProgram&` 调用。
- 其他上层代码不再能通过 `Ref<ShaderProgram>` 直接写 uniform。

需要审计调用点：

```powershell
rg -n -S "->SetInt|->SetFloat|->SetMat|SetShaderProgram\\(" HuaEngine/src Editor/src Tests
```

注意：

- `Material::SetShaderProgram(...)` 是 material 绑定 shader 资源的语义，不是 command path 的 `SetShaderProgram`，不应删除。
- `ShaderProgramLoader` 仍可返回 `Ref<ShaderProgram>`。

## 6. 分阶段任务

### R27-1：BindGroup builder 与 frame/object 迁移

目标：

- 新增 `RenderBindGroupBuilder`。
- Forward pass 使用 frame/object bind group。
- RHI smoke 保持使用 bind group。

验收：

- `ForwardOpaquePass` 不再调用 `SetFrameBinding` / `SetObjectBinding`。
- `RenderingOperationsSmoke` 和 `ApplicationOperationsSmoke` 通过。

### R27-2：移除 CommandList 兼容接口

目标：

- 从 `CommandList` 移除：
  - `SetShaderProgram`
  - `SetFrameBinding`
  - `SetMaterialBinding`
  - `SetObjectBinding`
- 从 `OpenGLCommandList` 移除对应实现。
- 删除 `FrameObjectBinding.h` 的 RHI command 依赖；如果无其他用途则删除。
- 删除 `MaterialBinding` 的 command 依赖；如果无其他用途则删除 `MaterialBinding.h`。

验收：

- `rg -n -S "SetFrameBinding|SetMaterialBinding|SetObjectBinding|CommandList::SetShaderProgram" HuaEngine/src Editor/src Tests` 无公共 command 调用残留。
- 兼容接口删除后全部相关 smoke 通过。

### R27-3：移除 ShaderProgram 公共 uniform setter

目标：

- `ShaderProgram` 公共接口只保留 `GetDesc()`。
- `OpenGLShaderProgram` setter 改为后端方法。
- 所有 uniform 提交只能通过 `SetBindGroup`。

验收：

- `rg -n -S "virtual void SetInt|virtual void SetFloat|virtual void SetMat|override;" HuaEngine/src/HuaEngine/Rendering/RHI/ShaderProgram.h HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h` 无公共 uniform setter。
- `RHICommandListBindingSmoke` 仍通过像素验证。

### R27-4：最终残留审计

目标：

- 确认公共 RHI 不再暴露 OpenGL 迁移期 command/uniform 接口。
- 更新或补充 spec 状态。

验收搜索：

```powershell
rg -n -S "SetShaderProgram\\(|SetFrameBinding\\(|SetMaterialBinding\\(|SetObjectBinding\\(|FrameBinding|ObjectBinding|MaterialBinding|virtual void SetInt|virtual void SetFloat|virtual void SetMat|GetRenderID\\(|GetColorAttachment\\(" HuaEngine/src/HuaEngine/Rendering/RHI HuaEngine/src/HuaEngine/Rendering/RenderPipeline HuaEngine/src/Platform/OpenGL/RHI Tests
```

允许残留：

- `Material::SetShaderProgram(...)`：material 资源语义。
- `OpenGLShader` 内部 uniform setter：OpenGL 后端实现细节。
- `OpenGLRenderTargetStorage::GetRenderID/GetColorAttachment`：OpenGL 后端内部 storage。

## 7. 风险与处理

### 7.1 BindGroup 创建成本变高

风险：

- frame/object/material bind group 都可能在每帧创建，带来 shared_ptr 分配和 layout/entry 拷贝。

处理：

- 本阶段接受小规模 smoke/editor 场景的成本。
- 后续单独做 `BindGroupCache` 或 dynamic uniform buffer 设计。
- 不在本阶段引入复杂缓存，以免和接口清理耦合。

### 7.2 OpenGL uniform 仍按字符串提交

风险：

- 即使公共 RHI 移除 uniform setter，OpenGL 后端内部仍按 uniform name 设置值。

处理：

- 这是 OpenGL backend implementation detail。
- 公共 RHI 只暴露 `BindGroup`，不暴露 uniform location 或 OpenGL texture unit。
- 后续可通过 shader reflection / cached uniform location 优化。

### 7.3 删除 MaterialBinding 影响范围不明

风险：

- `MaterialBinding` 可能仍被测试或旧工具引用。

处理：

- 先用 `rg` 确认调用点。
- 如果只剩 resolver/OpenGL command list，则删除。
- 如果还有上层语义用途，则保留类型但移出 RHI command path。

### 7.4 删除 CommandList 兼容接口破坏测试覆盖

风险：

- 旧 smoke 或测试仍调用兼容接口。

处理：

- 先改测试走 `SetBindGroup`。
- 再删接口。
- 最后用精确搜索确认无调用点。

## 8. 验证基线

每个任务完成后至少运行相关 smoke：

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke MaterialSerializationSmoke RenderingOperationsSmoke AssetServiceSmoke ApplicationOperationsSmoke Editor
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

最终执行：

```powershell
git diff --check
git status --short
```

## 9. 代码勘探摘要

本 spec 基于 2026-07-14 当前代码：

- `CommandList` 已有 `SetPipelineState` 和 `SetBindGroup`，但仍保留四个兼容接口。
- `ShaderProgram` 公共接口仍暴露 OpenGL 风格 uniform setter。
- `BindGroup` 已能表达 frame/material/object scope。
- `OpenGLCommandList::SetBindGroup` 已能根据 scope 标记 frame/object binding，并将 entries 映射到 shader uniform/texture。
- `RHICommandListBindingSmoke` 已使用 bind group 提交 frame/material/object。
- `ForwardOpaquePass` 已使用 `SetPipelineState` 和 material bind group，但 frame/object 仍用兼容接口。
- `RenderResourceResolver` 当前通过 `MaterialBinding` 中间结构构建 material bind group。
- 公共 RHI 已无 `GetRenderID/GetColorAttachment`，后端 ID 只留在 OpenGL backend 内部 storage 与 bridge。

## 10. 实现结果

- 已新增 `RenderBindGroupBuilder`，frame/material/object 均通过 bind group 构建。
- Forward draw 路径已改为 `SetPipelineState` + `SetBindGroup(0/1/2)` + `SetVertexBufferView` + `DrawIndexed`。
- `CommandList` 已移除 `SetShaderProgram`、`SetFrameBinding`、`SetMaterialBinding`、`SetObjectBinding`。
- `ShaderProgram` public uniform setter 已移除，OpenGL uniform 写入收敛为 backend 细节。
- 旧 `FrameObjectBinding` / `MaterialBinding` 类型已删除，`RenderResourceResolver` 不再通过旧 material binding 中间结构构建 GPU 提交资源。
- OpenGL backend 中无调用点的 `OpenGLGpuBuffer::GetRenderID()` 已删除；render target storage 的 `GetColorAttachment` 仍作为 backend bridge 保留。
