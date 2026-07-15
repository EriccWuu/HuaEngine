# 渲染现代 RHI 结构性抽象下一阶段 Spec

状态：草案  
日期：2026-07-14  
范围：`9ca27d1 refactor(rendering): retire rhi compatibility bindings` 之后，继续补齐现代 RHI 的结构性抽象  

## 1. 背景

当前渲染 RHI 已完成一轮关键清理：

- draw 提交主路径已收敛到 `PipelineState + VertexBufferView + BindGroup + Draw`。
- `CommandList` 已移除 `SetShaderProgram`、`SetFrameBinding`、`SetMaterialBinding`、`SetObjectBinding`。
- `ShaderProgram` public uniform setter 已移除。
- `FrameObjectBinding` / `MaterialBinding` 已删除。
- frame/material/object 数据已统一通过 `BindGroup` 提交。

这意味着公共 RHI 已经不再直接暴露 OpenGL uniform 和旧 binding 迁移接口。

但当前 RHI 仍不是完整现代 RHI。后续需要补齐的不是“再删一批 OpenGL API”，而是现代显式图形 API 需要的结构性抽象：

- render pass / attachment 描述。
- bind group layout 与 pipeline binding contract。
- command buffer / queue / submit。
- resource state / barrier。
- RenderGraph 驱动资源生命周期和 pass execution。
- vertex input / buffer binding 进一步拆分。

本 spec 将这些方向整理为下一阶段路线，按 P28-P32 分阶段推进。

## 2. 当前代码形状

### 2.1 CommandList

当前 `CommandList` 暴露：

```cpp
BeginRenderTarget(RenderTarget&)
ClearColor(const glm::vec4&)
BeginFrame(Camera&)
SetPipelineState(PipelineState&)
SetVertexBufferView(VertexBufferView&)
SetBindGroup(uint32_t, BindGroup&)
DrawIndexed(uint32_t)
EndFrame()
EndRenderTarget()
```

其中 `SetPipelineState`、`SetVertexBufferView`、`SetBindGroup`、`DrawIndexed` 已经是当前现代 draw path。

仍偏旧的部分是：

- `BeginRenderTarget/EndRenderTarget` 仍是 render target bind/unbind 语义。
- `ClearColor` 是独立清屏命令，而不是 render pass attachment load op。
- `BeginFrame(Camera&)` 仍是高层 renderer/frame 语义，不像底层 RHI command pass 概念。

### 2.2 RenderTarget

当前 `RenderTarget` 仍承担：

- resize
- clear attachment
- read pixel
- color attachment view
- specification 查询

这能支持 Editor/Smoke，但还没有和 `RenderPassDesc`、attachment load/store、resource state 明确分离。

### 2.3 BindGroup / PipelineState

当前 `BindGroup` 已有：

- `BindGroupScope`
- `BindGroupLayoutEntry`
- `BindGroupEntry`
- `BindGroupLayout`
- `BindGroup`

当前 `PipelineStateDesc` 只有：

```cpp
Ref<ShaderProgram> Shader;
BufferLayout VertexLayout;
PrimitiveTopology Topology;
```

缺少 pipeline 对 bind group layout 的声明，因此 `CommandList::SetBindGroup` 只能根据 runtime entry 写 OpenGL uniform，无法校验：

- slot 是否符合 pipeline 预期。
- scope 是否符合 pipeline 预期。
- binding name/type 是否符合 shader contract。
- material bind group 是否兼容当前 pipeline。

### 2.4 Command Submission

当前通过：

```cpp
RenderDevice::GetImmediateCommandList()
```

直接使用 immediate command list。

这适合 OpenGL 和当前 smoke，但不适合 Vulkan/D3D12/Metal 的 command buffer + queue 模型。

### 2.5 RenderGraph

当前已有 typed render graph resource 与 imported/transient resource 雏形，但 RenderGraph 还没有真正驱动：

- transient resource 创建。
- render pass begin/end。
- attachment load/store。
- resource state transition。
- barrier 插入。

## 3. 总目标

下一阶段的总目标是：

```text
在不破坏当前 OpenGL backend 和 smoke 的前提下，把 RHI 从“现代 draw path”推进到“现代 render pass + binding contract + command submission + resource state 的可演进骨架”。
```

阶段完成后应达到：

- public RHI 不再以 `BeginRenderTarget/EndRenderTarget` 作为唯一 pass 入口。
- pipeline 能声明 bind group layout contract。
- command submission 有 command buffer/queue 雏形，仍可兼容 immediate path。
- resource state/barrier 有基础类型和最小验证。
- RenderGraph 能逐步基于 render pass/resource access 运行，而不是只做 pass graph 编排。
- vertex input 与 vertex/index buffer binding 的边界更清晰。

## 4. 非目标

本阶段不做：

- 不实现 Vulkan/D3D12/Metal backend。
- 不引入完整 shader reflection。
- 不实现 descriptor heap / descriptor pool allocator。
- 不实现完整 GPU memory allocator。
- 不实现多 queue ownership transfer。
- 不重写整个 RenderGraph。
- 不删除 `GetImmediateCommandList`，只增加可演进路径。
- 不一次性拆掉 `VertexBufferView`，先做兼容性拆分或 contract 提前量。

## 5. 分阶段设计

## P28：RenderPassDesc / Attachment 抽象

### 5.1 目标

新增现代 render pass 描述，让 pass 入口从：

```cpp
BeginRenderTarget(RenderTarget&)
EndRenderTarget()
```

过渡到：

```cpp
BeginRenderPass(const RenderPassDesc&)
EndRenderPass()
```

短期保留 `BeginRenderTarget/EndRenderTarget` 作为兼容 helper，后续再删除。

### 5.2 建议新增类型

建议新增文件：

```text
HuaEngine/src/HuaEngine/Rendering/RHI/RenderPass.h
```

建议类型：

```cpp
enum class LoadOp : uint8_t {
	Load = 0,
	Clear,
	DontCare
};

enum class StoreOp : uint8_t {
	Store = 0,
	DontCare
};

struct RenderPassColorAttachment {
	Ref<RenderTarget> Target;
	uint32_t AttachmentIndex = 0;
	LoadOp Load = LoadOp::Clear;
	StoreOp Store = StoreOp::Store;
	glm::vec4 ClearColor = glm::vec4(0.0f);
};

struct RenderPassDepthStencilAttachment {
	Ref<RenderTarget> Target;
	LoadOp DepthLoad = LoadOp::Clear;
	StoreOp DepthStore = StoreOp::Store;
	float ClearDepth = 1.0f;
	uint32_t ClearStencil = 0;
};

struct RenderPassDesc {
	std::vector<RenderPassColorAttachment> ColorAttachments;
	std::optional<RenderPassDepthStencilAttachment> DepthStencilAttachment;
};
```

### 5.3 CommandList 接口

新增：

```cpp
virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
virtual void EndRenderPass() = 0;
```

短期保留：

```cpp
virtual void BeginRenderTarget(RenderTarget& target) = 0;
virtual void EndRenderTarget() = 0;
```

兼容策略：

- `BeginRenderTarget(RenderTarget&)` 在 OpenGL backend 内部转成单 color attachment render pass。
- `ClearColor` 暂时保留，但 Forward 主路径应优先通过 `LoadOp::Clear` 表达 clear。
- Forward pipeline 先迁移到 `BeginRenderPass/EndRenderPass`。

### 5.4 OpenGL backend 策略

OpenGL 实现中：

- `BeginRenderPass` 绑定目标 render target。
- 根据 `ColorAttachments` 的 `LoadOp::Clear` 执行 `glClearColor/glClear`。
- `EndRenderPass` 解除当前 render target。
- 只支持当前已有 `RenderTarget` attachment 形态。
- 多 color attachment 可以先定义类型，但实现可先限制为当前支持范围并给出 warning/diagnostic。

### 5.5 验收

验收条件：

- `CommandList` 新增 render pass 接口。
- `ForwardRenderPipeline` 使用 `BeginRenderPass/EndRenderPass`。
- `BeginRenderTarget/EndRenderTarget` 仍可用，作为 compatibility helper。
- RHI smoke 增加 render pass clear/draw 覆盖。
- 搜索确认 Forward 主路径不再调用 `BeginRenderTarget/EndRenderTarget`。

建议验证：

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RenderingOperationsSmoke ApplicationOperationsSmoke Editor
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

## P29：BindGroupLayout 与 PipelineState Binding Contract

### 5.6 目标

让 `PipelineState` 声明它期望的 bind group layouts，使 `SetBindGroup(slot, group)` 不只是“传参”，而是有 contract 可校验。

### 5.7 PipelineStateDesc 扩展

建议将 `PipelineStateDesc` 扩展为：

```cpp
struct PipelineBindGroupLayoutRef {
	uint32_t Slot = 0;
	Ref<BindGroupLayout> Layout;
};

struct PipelineStateDesc {
	Ref<ShaderProgram> Shader;
	BufferLayout VertexLayout;
	PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
	std::vector<PipelineBindGroupLayoutRef> BindGroupLayouts;
};
```

短期目标不是 shader reflection，而是让 renderer 明确传入：

- slot 0 frame layout
- slot 1 material layout
- slot 2 object layout

### 5.8 校验规则

`CreatePipelineState` 应校验：

- shader 非空。
- vertex layout 非空。
- bind group layout slot 不重复。
- layout 非空。

`CommandList::SetBindGroup(slot, group)` 在当前 pipeline 已绑定时应校验：

- slot 是否存在于 pipeline contract。
- group layout scope 是否匹配。
- group layout entries name/type/binding 是否匹配。

OpenGL backend 可在 warning 后拒绝提交不兼容 group。

### 5.9 Material bind group layout 缓存

当前 material bind group builder 每次根据 material 参数构建 layout。

P29 可以先不做全局缓存，但应明确：

- pipeline contract 要求的 material layout 应来自 material/base material contract。
- material instance override 不应改变 layout，只改变 entry value。
- 新增 override 如果 base material 没有该参数，短期可以继续加入 bind group，但后续应收敛为 material contract validation。

### 5.10 验收

验收条件：

- `PipelineStateDesc` 能声明 bind group layouts。
- Forward pipeline 创建 pipeline state 时传入 frame/material/object layout contract。
- `SetBindGroup` 能拒绝 slot/scope/type 不匹配。
- smoke 覆盖：
  - 正确 layout 可绘制。
  - 错误 slot 或错误 scope 不绘制或返回 diagnostic/warning。

## P30：CommandBuffer / Queue 雏形

### 5.11 目标

引入现代 command submission 骨架，但不立即替换所有 immediate path。

目标形态：

```cpp
class CommandBuffer {
public:
	virtual ~CommandBuffer() = default;
};

class RenderQueue {
public:
	virtual void Submit(CommandBuffer& commandBuffer) = 0;
};
```

更完整的方向：

```cpp
Ref<CommandBuffer> RenderDevice::CreateCommandBuffer(const CommandBufferDesc& desc);
RenderQueue& RenderDevice::GetGraphicsQueue();
```

### 5.12 兼容策略

OpenGL backend 可以先实现为 immediate-backed command buffer：

- record 阶段可以直接执行，或记录轻量 command list。
- `GetImmediateCommandList()` 保留。
- smoke 和 Forward 可先不迁移。

这个阶段的主要价值是建立 API 形状，而不是实现真正多线程录制。

### 5.13 验收

验收条件：

- `RenderDevice` 暴露 command buffer/queue 创建或访问接口。
- OpenGL backend 有最小实现。
- 不破坏 immediate command list。
- smoke 覆盖创建 command buffer、submit 空 command buffer 或简单 pass。

## P31：Resource State / Barrier / RenderGraph Execution

### 5.14 目标

补齐现代显式 API 所需的资源状态基础类型，并让 RenderGraph 有能力基于资源 access 推导 pass 间依赖。

### 5.15 建议新增类型

```cpp
enum class ResourceState : uint32_t {
	Undefined = 0,
	RenderTarget,
	DepthStencilWrite,
	ShaderRead,
	CopySrc,
	CopyDst,
	VertexBuffer,
	IndexBuffer,
	Present
};

struct ResourceBarrier {
	Ref<TextureResource> Texture;
	ResourceState Before = ResourceState::Undefined;
	ResourceState After = ResourceState::Undefined;
};
```

后续可扩展 buffer barrier。

### 5.16 RenderGraph 接入方向

RenderGraph pass 应逐步声明：

- read resources
- write resources
- attachment usage
- imported/exported resources

RenderGraph compile 阶段应生成：

- pass order
- resource lifetime
- resource access diagnostics
- barrier plan

OpenGL backend 可以先把 barrier 实现为空操作，但类型和验证应先建立。

### 5.17 验收

验收条件：

- 有 resource state/barrier 类型。
- RenderGraph resource access 能产生基础 barrier plan。
- 对无效状态或读写冲突产生 diagnostic。
- OpenGL backend 可接受 barrier no-op。
- RenderPassGraphSmoke 增加资源状态/冲突覆盖。

## P32：Vertex Input / Buffer Binding 进一步拆分

### 5.18 目标

降低 `VertexBufferView` 的过渡性，把 vertex input contract、vertex buffer binding、index buffer binding 的边界拆清楚。

当前：

```text
VertexBufferView = vertex buffer + index buffer + vertex layout + index count
```

长期目标：

```text
PipelineStateDesc::VertexLayout
VertexBufferBinding
IndexBufferBinding
DrawIndexedDesc / DrawIndexed(indexCount)
```

### 5.19 分阶段策略

不建议一次删除 `VertexBufferView`。

建议先新增：

```cpp
struct VertexBufferBinding {
	Ref<GpuBuffer> Buffer;
	uint32_t Offset = 0;
	uint32_t Stride = 0;
};

struct IndexBufferBinding {
	Ref<GpuBuffer> Buffer;
	uint32_t Offset = 0;
	IndexFormat Format = IndexFormat::UInt32;
};
```

然后新增 command 接口：

```cpp
SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding)
SetIndexBuffer(const IndexBufferBinding& binding)
```

`SetVertexBufferView` 暂时作为 compatibility helper，内部展开到新接口。

### 5.20 验收

验收条件：

- 新增 vertex/index buffer binding 类型。
- OpenGL backend 支持新 binding 接口。
- `VertexBufferView` 可以继续存在，但不再是唯一提交方式。
- RHI smoke 覆盖新 binding path。

## 6. 推荐执行顺序

推荐顺序：

```text
P28 RenderPassDesc / Attachment 抽象
P29 BindGroupLayout 与 PipelineState binding contract
P30 CommandBuffer / Queue 雏形
P31 Resource State / Barrier / RenderGraph Execution
P32 Vertex Input / Buffer Binding 拆分
```

原因：

1. P28 直接清理当前最明显的旧式 pass 入口。
2. P29 强化刚完成的 bind group 路径，让它从“参数袋子”升级为 pipeline contract。
3. P30 建立现代提交模型骨架，但不急着迁移全部渲染路径。
4. P31 依赖 P28/P30 的 pass 和 submission 形状，适合后置。
5. P32 会影响 mesh/resource 侧，和 pass/descriptor 解耦后再做风险更低。

## 7. 代码勘探摘要

本 spec 基于 2026-07-14 当前代码勘探：

- `CommandList.h` 当前仍有 `BeginRenderTarget/EndRenderTarget/ClearColor/BeginFrame`。
- `ForwardRenderPipeline.cpp` 当前 `BindTargetPass` / `UnbindTargetPass` 仍调用 `BeginRenderTarget/EndRenderTarget`。
- `BindGroup.h` 已有 layout 与 entry 类型，但 pipeline state 不持有 bind group layout contract。
- `PipelineState.h` 当前只描述 shader、vertex layout、topology。
- `RenderDevice.h` 当前只有 `GetImmediateCommandList()`，还没有 command buffer / queue。
- `RenderGraphResource.h` / `PassGraph.h` 已有 typed resource 与 pass graph 雏形，但还没有 resource state/barrier。
- `VertexBufferView.h` 当前仍把 vertex buffer、index buffer、layout、index count 打包在一个 view 中。
- OpenGL backend 已能基于 `SetPipelineState`、`SetBindGroup`、`SetVertexBufferView`、`DrawIndexed` 完成绘制。

## 8. 下一步建议

下一步建议先为 P28 单独写 implementation plan 并执行。

P28 的最小可交付边界应是：

- 新增 `RenderPass.h`。
- `CommandList` 新增 `BeginRenderPass/EndRenderPass`。
- OpenGL backend 实现新接口。
- Forward pass 使用新接口。
- `BeginRenderTarget/EndRenderTarget` 保留为兼容 helper。
- `RHICommandListBindingSmoke` 或新增 smoke 覆盖 render pass clear/draw。

完成 P28 后，再根据实际接口形状细化 P29 的 pipeline binding contract。

### P28 实现结果

- 已新增 `RenderPass.h`，定义 `LoadOp`、`StoreOp` 与 render pass attachment desc。
- `CommandList` 已新增 `BeginRenderPass/EndRenderPass`。
- OpenGL backend 已实现 render pass clear/draw 路径。
- Forward 主路径已迁移到 `BeginRenderPass/EndRenderPass`。
- `BeginRenderTarget/EndRenderTarget` 暂时保留为 compatibility helper。
- `PassGraph` 原有节点描述已从 `RenderPassDesc` 改名为 `PassGraphPassDesc`，避免与 RHI render pass 描述冲突。
- P28 暂不支持 MRT、非 0 color attachment index、独立 depth/stencil target；OpenGL backend 会拒绝这些未支持的 desc。


### P29 ????

- `PipelineStateDesc` ??? `BindGroupLayouts`??? `PipelineBindGroupLayoutRef` ?? pipeline ??? bind group slot ? layout contract?
- OpenGL pipeline ??????? bind group layout contract???? layout ??? slot???? contract pipeline ??????????
- OpenGL `CommandList::SetBindGroup(slot, group)` ???? pipeline contract ?? slot?scope?entry name/type/binding?????????? bind group?
- `RenderBindGroupBuilder` ??? `CreateFrameBindGroupLayout` ? `CreateObjectBindGroupLayout`???? resolver ?????? frame/object layout contract?
- Forward resolver ?? pipeline state ???? slot 0 frame?slot 1 material?slot 2 object ? layout contract?material bind group ??????????????? item?
- `RHIResourceCreationSmoke` ??? pipeline layout contract round-trip??? slot ???? layout ???
- `RHICommandListBindingSmoke` ????? contract ???pipeline ????? frame/object bind group ??????? scope ? object bind group ???? draw ????

P29 ???????

- material layout ???? material bind group builder ????????????? base material ????? layout cache?
- `SetBindGroup` ? layout ?????????????? shader reflection ????????
- `BeginRenderTarget/EndRenderTarget` ? `SetVertexBufferView` ???????????? P30-P32 ???? command submission?resource state/barrier?vertex/index binding ???

P29 ?????

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke
.uildin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.uildin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

### P30 实现结果

- RHI 已新增 `CommandBuffer`、`CommandBufferDesc`、`CommandBufferUsage` 与 `RenderQueue`。
- `RenderDevice` 已暴露 `CreateCommandBuffer` 与 `GetGraphicsQueue`。
- OpenGL backend 已提供 immediate-backed command buffer/queue 最小实现；当前提交空 graphics command buffer 是同步 no-op。
- 现有 `GetImmediateCommandList()` 与 Forward 主路径保持不变。
- `RHIResourceCreationSmoke` 已覆盖 command submission capability、graphics command buffer 创建、空提交和 invalid usage 拒绝。
- 已验证 `RHICommandListBindingSmoke`、`RHIResourceCreationSmoke`、`RenderingOperationsSmoke` 均通过。

### P31 实现结果

- RHI 已新增 `ResourceState` 与 `ResourceBarrier`。
- `CommandList` 已新增 `ResourceBarrier` 接口，OpenGL backend 当前接受 texture barrier 并 no-op。
- `PassGraph` compile 阶段会基于现有 `Inputs` / `Outputs` 生成最小 barrier plan。
- 当前 barrier plan 映射规则：input -> `ShaderRead`，output -> `RenderTarget`。
- `RenderPassGraphSmoke` 已覆盖 typed graph 的 input/output barrier plan。
- `RHIResourceCreationSmoke` 已覆盖 immediate command list 接受 texture barrier。
- 本轮未实现真实 GPU barrier、transient resource 创建、读写冲突自动排序或 Vulkan/D3D12 状态转换。

### P32 实现结果

- RHI 已新增 `VertexBufferBinding` 与 `IndexBufferBinding`。
- `CommandList` 已新增 `SetVertexBuffer` 与 `SetIndexBuffer`。
- OpenGL backend 已支持 explicit vertex/index binding path，并保留 `SetVertexBufferView` compatibility path。
- explicit binding path 当前使用当前 pipeline 的 `VertexLayout` 建立内部 VAO；支持 slot 0 vertex buffer 与 `UInt32` index buffer。
- Forward 主路径暂未迁移，仍使用 `VertexBufferView`。
- `RHICommandListBindingSmoke` 已覆盖 explicit binding path 绘制。
- `RHIResourceCreationSmoke` 已覆盖 binding desc 的字段 round-trip。
