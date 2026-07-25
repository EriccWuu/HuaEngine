# HuaEngine 现代 RHI 架构基线与 D3D12 接入 Spec

状态：草案
日期：2026-07-25
基线提交：`807dd52 feat(rendering): cull graph passes from explicit outputs`

## 1. 目标与边界

本阶段目标是让 HuaEngine 具备现代显式图形 API 所需的核心抽象，并以 D3D12 作为首个现代后端验证该抽象。

本阶段不追求商业级 RHI 的全覆盖，也不把高级 renderer feature、性能工具链或多平台后端作为阻塞项。完成定义是：Forward 主路径能够同时由 OpenGL 与 D3D12 驱动，RHI/RenderGraph 的资源、同步、绑定和提交契约不依赖 OpenGL 特性。

### 1.1 已完成基线

- 显式 pipeline、bind group、vertex/index binding 与 command buffer 主路径。
- graphics/compute/copy queue 与 timeline wait/signal 契约。
- texture transient pool、fence 保护、scene color/depth 后处理链路。
- RenderGraph attachment、resource state、拓扑执行、显式输出与 pass culling。

### 1.2 本阶段必须完成

- P65-P70：现代 RHI 架构基线。
- P71-P76：D3D12 最小可用后端与 Forward 主路径验证。

### 1.3 明确后置的扩展项

- 高级 renderer feature：阴影、透明、TAA、Bloom、IBL、GPU culling 等。
- Vulkan、Metal 等第二后端。
- GPU profiling、RenderDoc/PIX 深度集成、marker 与性能调优。
- bindless、资源驻留、streaming、复杂内存预算、并行录制等规模化优化。

## 2. 设计原则

1. 先稳定跨后端 contract，再增加 rendering feature。
2. 新能力必须进入 RHI/RenderGraph 主路径，不能只在 smoke 或单一后端中存在。
3. OpenGL 可保留兼容实现，但不得成为 API 语义的来源。
4. 每个 P 独立实现、更新本 spec、运行 smoke 并创建独立提交。
5. D3D12 首版以 Windows desktop 为范围，不承诺 Xbox、UWP、多适配器或 DXR。

## 3. 架构基线阶段

### P65：全图依赖解析与循环诊断

#### 目标

让 RenderGraph 在完整图上解析资源 producer/consumer，而不要求 producer 写在 consumer 前面。

#### 内容

- 建立资源 writer 表和 reader 表，再构建 dependency DAG。
- 支持 future producer；缺 producer、多个不兼容 writer、循环依赖给出确定 diagnostic。
- 保持显式 output、side effect 与 pass culling 的语义。

#### 验收

- consumer 写在 producer 前仍能得到 producer-first execution order。
- cycle graph compile 失败且包含参与 pass/resource 的 diagnostic。
- P64 的 output culling smoke 保持通过。

#### 实现结果

- compiler 现先收集全图 resource writer/reader，再建立 producer-to-consumer dependency，legacy input/output 与 typed explicit usage 均复用该解析路径。
- future producer 不再被按声明顺序误判为缺失；拓扑执行计划会将 writer 安排在 reader 前。
- 编译器会在最终 execution order 上重建 barrier before/after 状态，避免 future producer 图沿用声明顺序的错误 transition。
- Kahn 排序无法覆盖全部 pass 时报告 `CyclicDependency` diagnostic。
- `RenderPassGraphSmoke` 覆盖 future producer 重排、真实执行顺序及循环依赖失败；四个 RHI/Rendering smoke 均通过。

### P66：RenderGraph Queue 调度与同步计划

#### 目标

让 graphics/compute/copy pass 的类型真正参与 queue 选择与 timeline wait/signal 生成。

#### 内容

- 为每个 pass 生成 queue assignment 和 submit batch。
- 对跨 queue dependency 生成 wait fence/value；同 queue 维持顺序提交。
- OpenGL 保持串行模拟，但必须消费同一份调度计划。

#### 验收

- graph 包含 graphics -> compute -> copy 依赖时，计划包含正确 queue 与 wait/signal 链。
- 不满足 wait 的 submit 被拒绝；满足后按计划执行。
- Forward 纯 graphics 图不产生额外跨 queue wait。

#### 实现结果

- `PassGraph` 现在在编译完成后输出连续 queue segment 的 `PassGraphQueueBatch`，并将 graphics/compute/copy pass 映射到对应 `RenderQueueType`。
- compiler 根据已解析的 dependency DAG 为跨 queue consumer batch 记录 producer batch wait；同 queue pass 保留在同一 batch 内按 execution order 执行。
- 当前 Forward 仍是纯 graphics 单 batch，OpenGL 继续串行消费现有执行计划；没有在缺少 compute dispatch/copy 命令的阶段伪造多 queue 回放。
- `RenderPassGraphSmoke` 验证 graphics -> compute -> copy 生成三个 batch，compute 等待 graphics，copy 等待 compute；Forward 冒烟继续通过。

### P67：Transient Buffer 与统一资源生命周期

#### 目标

将 transient 管理从 texture 扩展到 buffer，使 uniform、storage、copy 中间结果具备同样的 lifetime、aliasing 和 fence 保护。

#### 内容

- RenderGraph runtime resource 支持 texture 与 buffer。
- allocator 支持 buffer pool、描述匹配、同帧非重叠别名和跨帧 fence 复用。
- 扩展 usage/state，覆盖 uniform、storage、copy source/destination 的最小集合。

#### 验收

- 同规格且 lifetime 不重叠的 transient buffer 使用同一物理 buffer。
- 未完成 fence 阻止复用，完成后允许复用。
- compute/copy smoke 能通过 graph 取得 runtime buffer。

#### 实现结果

- `RenderGraphResourceDesc` 与 runtime resource table 现支持 `RuntimeBuffer`/`Buffer`，imported buffer 和 transient buffer 都可由 handle 解析。
- allocator 新增 persistent transient buffer pool，按 size、stride、usage 匹配，支持同帧非重叠 lifetime 别名和以 completed fence 为条件的跨帧复用。
- `ReleaseTransientResources()` 同时登记活动 texture pool 和 buffer pool 的 fence value，保持统一资源生命周期语义。
- `RHIResourceCreationSmoke` 使用真实 OpenGL device 验证 buffer 同帧别名、未完成 fence 阻止复用和完成后复用；四个 RHI/Rendering smoke 均通过。

### P68：BindGroup 与 Pipeline Layout 收敛

#### 目标

让资源绑定契约可无歧义映射到 D3D12 root signature 和 descriptor heap。

#### 内容

- BindGroupLayoutEntry 增加 shader stage visibility。
- buffer binding 增加类型、offset、size/range；为未来 dynamic offset 预留语义。
- layout signature 稳定化，pipeline 与 bind group 必须通过 signature 匹配。
- Material 继续基于现有 schema，不在本 P 引入完整 shader reflection。

#### 验收

- 不可见 stage 或 range 不匹配时验证失败。
- 相同 schema 的 bind group layout 可以复用。
- D3D12 后端可以由 layout 生成对应 root parameter/descriptor range。

#### 实现结果

- `BindGroupLayoutEntry` 新增 shader stage visibility 与 minimum binding size；新增 uniform/storage buffer binding value type，以及 bind group entry 的 offset/size range。
- 提供稳定的 `CalculateBindGroupLayoutSignature()`，签名覆盖 scope、名称、类型、binding、visibility 和 minimum binding size，可作为未来 pipeline/root-signature cache key。
- OpenGL backend 在创建 layout 时验证重复 binding、空名称、空 visibility 和非法 non-buffer size；创建 bind group 时验证 layout 完整匹配、buffer range 与 minimum size。
- 现阶段 OpenGL 不伪装 UBO/SSBO shader 绑定实现；新增字段首先作为跨后端 layout contract，由 D3D12 descriptor/root signature 映射消费。
- `RHIResourceCreationSmoke` 验证 visibility、稳定签名、合法 uniform buffer range 和越界 range 拒绝；四个 RHI/Rendering smoke 均通过。

### P69：Upload、Readback 与资源初始化路径

#### 目标

建立统一的 CPU-GPU 数据传输模型，移除后端私有的临时初始化路径。

#### 内容

- 定义 upload/staging buffer 与 copy command contract。
- 支持 buffer 初始数据、buffer 更新、texture 初始数据的最小路径。
- 定义 readback request 与完成 fence，不要求异步资源 streaming。

#### 验收

- 通过 copy queue 或模拟 copy queue 上传 vertex/index/uniform 数据。
- texture upload 后可被 shader sampling；readback 在 fence 完成前不可读取。
- OpenGL 与 D3D12 使用相同的公开 RHI API。

### P70：Texture Subresource 与 Resolve 模型

#### 目标

让 texture state 与 view 描述覆盖现代 API 的基本 subresource 维度，而不是只处理整个 texture。

#### 内容

- TextureView 支持 mip range、array layer range、aspect。
- Resource barrier 支持 subresource range，whole-resource 作为默认快捷语义。
- 定义 MSAA color attachment 与 resolve source/destination pass usage。
- 不要求 cubemap、3D texture、sparse resource 或完整 format matrix。

#### 验收

- 同一 texture 的不同 mip 可表达不同 view 与 transition。
- MSAA render target 能显式 resolve 到 sampled texture。
- 无效 subresource range 在创建期或 graph compile 期失败。

## 4. D3D12 后端阶段

### P71：D3D12 Device、Adapter 与 Swapchain 骨架

#### 目标

在 Windows 上创建可呈现的 D3D12 device，接入现有 window 生命周期。

#### 内容

- DXGI adapter 枚举与 hardware adapter 选择。
- D3D12 device、direct command queue、fence、swapchain、back buffer RTV。
- resize、present、VSync 的最小契约。

#### 验收

- D3D12 backend 初始化、清屏、present、resize 可用。
- 无兼容 adapter 时给出可读 diagnostic，不影响 OpenGL backend。

### P72：D3D12 CommandBuffer、Queue 与 Fence

#### 目标

把现有 CommandBuffer 生命周期映射为 D3D12 command allocator/list、queue execute 和 fence。

#### 内容

- graphics command list 的 begin/end/reset/close/replay-free submit。
- command allocator 按 frames-in-flight 管理。
- queue submit 消费现有 wait/signal descriptor。

#### 验收

- recorded command buffer 在 D3D12 上完成 clear/draw。
- fence timeline 的 signal/completed/wait 行为与 RHI contract 一致。

### P73：D3D12 Resource、View 与 State Barrier

#### 目标

映射 TextureResource、GpuBuffer、TextureView 和 ResourceBarrier 到 D3D12 resource/descriptor/state。

#### 内容

- default/upload/readback heap 的最小资源创建。
- RTV/DSV/SRV/UAV descriptor 创建与资源 state transition。
- 支持 P70 定义的 subresource range，允许 whole-resource fallback。

#### 验收

- graph attachment 写入、shader read、copy transition 在 D3D12 上可执行。
- texture view 与 render target format 不匹配时创建或绑定失败。

### P74：D3D12 Pipeline 与 BindGroup 映射

#### 目标

把 PipelineState 与 P68 的 layout contract 映射到 PSO、root signature、descriptor heap。

#### 内容

- HLSL 编译路径与现有 shader abstraction 的兼容策略。
- graphics PSO、input layout、raster/depth/blend state。
- frame/material/object bind group 映射为 root table 或 root descriptor。

#### 验收

- Forward fallback mesh 能经 D3D12 pipeline 绘制。
- pipeline layout 不匹配的 bind group 被拒绝。

### P75：D3D12 Forward RenderGraph 主路径

#### 目标

让 `ForwardRenderPipeline` 不改业务逻辑地运行在 D3D12。

#### 内容

- attachment-driven render pass、scene color/depth、post-process、present。
- transient allocator 与 D3D12 fence 联动。
- OpenGL/D3D12 共享 RenderGraph 与 Forward 流程。

#### 验收

- `RenderingOperationsSmoke` 在 D3D12 backend 下完成 fallback、材质颜色与后处理像素验证。
- 图统计、resource state、frames-in-flight 语义与 OpenGL 一致。

### P76：D3D12 Compute/Copy 最小验证

#### 目标

验证 P66-P69 的抽象可映射到 D3D12 compute/copy queue，而不是只存在于 API 层。

#### 内容

- compute queue、copy queue 的 command list/allocator。
- 跨 queue wait/signal 的实际 fence 提交。
- 最小 compute 或 copy graph 验证，不引入渲染 feature。

#### 验收

- graphics -> compute -> copy 图在 D3D12 上按计划提交。
- 最终 readback 或资源状态证明依赖已正确满足。

## 5. 现代 RHI 架构基线完成定义

满足以下条件即视为本阶段完成：

- P65-P70 的图编译、资源、描述符、传输和 subresource contract 已落实。
- D3D12 可以运行 Forward RenderGraph 主路径并通过核心 smoke。
- OpenGL 仍作为兼容后端通过相同 RHI 主路径运行。
- renderer feature、profiling 与高性能扩展均不再阻塞该完成定义。

## 6. 后续扩展目录

以下项目按需进入独立 spec，不影响第 5 节完成定义。

### 6.1 现代后端与平台

- Vulkan 后端：用于跨平台与第二后端抽象校验。
- Metal、Android Vulkan、Linux presentation。
- 多 adapter、GPU preference、device lost/recovery。

### 6.2 资源与性能规模化

- heap allocator、memory budget、residency、eviction。
- bindless / descriptor indexing。
- dynamic uniform ring、resource streaming、异步 upload。
- pass merge、barrier batching、parallel command recording。

### 6.3 工具与诊断

- GPU timestamp/query、debug marker、PIX/RenderDoc integration。
- validation layer、DRED、资源泄漏与 state mismatch 报告。

### 6.4 渲染学习与功能

- 通用 post-process stack、Bloom、tone mapping、TAA。
- depth prepass、透明排序、shadow map、IBL、Forward+/clustered。
- indirect draw、GPU culling、visibility buffer。

## 7. 推荐执行顺序

```text
P65 -> P66 -> P67 -> P68 -> P69 -> P70
P71 -> P72 -> P73 -> P74 -> P75 -> P76
扩展项按引擎目标与学习计划单独排期
```

P65-P70 先稳定与 backend 无关的 contract；P71-P76 再用 D3D12 验证。若 D3D12 实现中暴露 contract 缺口，应回补对应 P 的规格和测试，不直接在 D3D12 层添加专用逃生路径。
