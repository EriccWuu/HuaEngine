# Render 重构 P0-P3 设计文档

## 背景

HuaEngine 当前渲染链路已经能支撑 Editor Game 窗口和 smoke 测试，但渲染可靠性、可验证输出、提交契约、资源边界仍然混在一起。`RenderSystem` 同时负责 ECS 查询、framebuffer 绑定、clear、资源解析和 GPU submit；`FrameBuffer` 缺少 readback 能力；`MeshComponent` 会 lazy load runtime `VertexArray`；`MaterialInstance::GetShader()` 在 base material 为空时存在空指针风险。

本设计覆盖 P0 到 P3。目标不是立刻引入完整 RHI，而是先建立可验证的渲染基线，再逐步拆出提交契约、资源解析边界和轻量 pipeline/pass 结构。真正的 RHI 放到 P4。

## 总体路线

采用分层递进方案：

1. P0：建立渲染可靠性和像素级验证基线。
2. P1：拆出 `RenderView`、`RenderItem`、`SceneRenderExtractor` 和 pipeline 消费契约。
3. P2：集中资源解析和诊断，降低 component 与 GPU runtime resource 的耦合。
4. P3：引入轻量 `ForwardRenderPipeline`、pass 边界和 `RenderStats`。
5. P4：在上述边界稳定后再设计真正 RHI。

这条路线优先保证每一轮都有明确验收方式，避免大重构后只能依靠肉眼判断画面是否正常。

## P0：渲染可靠性与可验证输出

### 目标

P0 只建立可靠性底座，不改变大的渲染架构。范围控制在 bug 修复、防御性诊断、最小 readback、smoke 强化。

### 设计

`FrameBuffer` 增加最小 readback API，优先支持读取 `RGBA8` color attachment 的指定像素或小区域。接口不做完整 RHI 抽象，只服务 smoke 和后续渲染回归验证。

`OpenGLFrameBuffer` 修复 depth attachment 创建逻辑。当前 depth switch 错误匹配 `FrameBufferTextureFormat::RGBA8`，应改为处理 `DEPTH24_STENCIL8`，并通过 `GL_DEPTH_STENCIL_ATTACHMENT` 创建 depth/stencil attachment。

Editor Game framebuffer、smoke framebuffer 默认 attachment 从 `{ RGBA8 }` 调整为 `{ RGBA8, DEPTH24_STENCIL8 }`，保证 depth test 相关场景有正确 target。

`RenderSystem::RenderSingleCamera` 增加 framebuffer 空检查。P0 不强行扩大函数签名，缺失 framebuffer 时记录 warning 并返回；结构化错误返回留给后续阶段。

`MaterialInstance::GetShader()` 在 base material 为空时返回 `nullptr`，避免 `Renderer::Submit()` 判空表达式先触发空指针解引用。`Bind()`、`ApplyParameters()` 继续保留现有 warning 行为。

`RenderingOperationsSmoke` 在当前 operation 成功断言之外，增加真实像素输出验证。测试加载 sandbox scene 后渲染到 framebuffer，并通过 readback 检查固定采样点或小区域中至少一个像素不同于 clear color。

### 验收

- `RenderingOperationsSmoke` 能证明 sandbox scene 渲染到了 framebuffer。
- `MaterialSerializationSmoke` 继续通过。
- `Editor` 目标构建通过。
- Game 窗口仍能显示 `Tests/TestProj` scene。

## P1：拆 RenderSystem 与渲染提交契约

### 目标

`RenderSystem` 不再直接混合 ECS 查询、资源解析、framebuffer clear 和 GPU submit。它只负责把 ECS 世界转换成渲染意图，实际提交交给 pipeline。P1 不做 culling、sorting、pass graph，也不改变资产格式。

### 新概念

`RenderView` 描述一次渲染视图，包含 camera、target framebuffer、viewport、clear color 和 clear flags。Editor viewport 与 runtime scene camera 最终都通过该结构进入 pipeline。

`RenderItem` 描述一个待渲染实体，包含 entity id、transform matrix、mesh 引用信息和 material 引用信息。P1 可以继续携带现有 `Ref<VertexArray>` 与 `Ref<MaterialInstance>` 作为过渡字段；P2 再把资源解析独立出去。

`SceneRenderExtractor` 输入 `World&`，输出 `std::vector<RenderItem>`。初期只查询 `TransformComponent + MeshComponent + MaterialComponent`，无 mesh/material 的实体不进入 render item。

`RenderPipeline` 改成消费 `RenderView + RenderItem list`。它负责 bind framebuffer、clear、`Renderer::Begin`、遍历 submit、`Renderer::End`、unbind。当前空壳 `RenderPipeline` 演进为默认 forward pipeline，内部仍调用现有 `Renderer::Submit`。

### 数据流

Runtime scene camera：

```text
RenderSystem::Update(SystemContext&)
  -> query CameraComponent
  -> build RenderView
  -> SceneRenderExtractor::Extract(World&)
  -> RenderPipeline::Render(RenderView, RenderItems)
```

Editor Game viewport：

```text
ApplicationOperations::RenderSceneViewport(scene, editorCamera)
  -> build RenderView
  -> SceneRenderExtractor::Extract(scene.GetWorld())
  -> RenderPipeline::Render(RenderView, RenderItems)
```

### 验收

- `RenderSystem` 中不再直接遍历 renderable entity 并调用 `Renderer::Submit`。
- Editor Game viewport 和 runtime scene camera 共用 `RenderPipeline(RenderView, RenderItems)`。
- `RenderingOperationsSmoke` 仍通过，并沿用 P0 的像素验证。
- 不引入 RHI，不引入排序/culling，不改变 scene schema。

## P2：整理资源与材质边界

### 目标

把 component、asset registry、GPU runtime resource 的职责分开。P2 不要求一次性改完序列化格式，但要开始把“组件自己 lazy load GPU 对象”的模式迁移出去。

### 设计

新增 `RenderResourceResolver`。它接收 `RenderItem` 中的稳定引用，输出 pipeline 可提交的 runtime resource。第一阶段继续复用 `MeshManager`、`MaterialLibrary`、`MaterialInstance`，但资源解析调用集中到 resolver，而不是散落在 component、pipeline、system 中。

`MeshComponent` 第一阶段保留 `MeshAssetName` 和现有 `m_CachedVertexArray`，避免破坏旧代码。新 pipeline 不再把 `MeshComponent::GetVertexArray()` 作为主要提交入口，而是由 resolver 根据 `MeshAssetName` 解析。

`MaterialComponent` 第一阶段继续持有 `Ref<MaterialInstance>`，但 resolver 负责校验 material instance、base material、shader 是否可用。缺失资源不再只靠 `Renderer::Submit` warning，而是在 resolver 或 pipeline 层记录诊断。

新增轻量 `RenderDiagnostics`，记录 skipped item 原因，例如 `MissingMeshAsset`、`MissingVertexArray`、`MissingMaterialInstance`、`MissingBaseMaterial`、`MissingShader`。P2 先服务 smoke、pipeline 断言和日志，后续可以接 Editor Console 与 ValidationService。

scene schema v3 保持兼容。scene 仍保存 `MeshAssetName` 与 `MaterialInstance`。设计原则是 scene 数据不能依赖 `VertexArray`、`Shader`、`Texture` 这类 backend runtime 指针作为核心状态。

### 验收

- 渲染提交前有统一资源解析入口。
- 缺失 mesh/material/shader 有明确诊断和 skipped 计数。
- 新 pipeline 不再依赖 `MeshComponent::GetVertexArray()` 作为主要资源解析路径。
- scene serialization 行为保持兼容。
- `RenderingOperationsSmoke` 可断言 submitted/skipped 或 diagnostics，且像素验证继续通过。

## P3：轻量 RenderPipeline / Pass 化

### 目标

在 P1/P2 边界稳定后，把当前 forward 渲染流程组织成可扩展 pipeline。P3 只建立 pass 边界、统计和可扩展接口，不做复杂 frame graph。

### 设计

`ForwardRenderPipeline` 作为默认 pipeline 实现，负责当前 opaque forward submit。它消费 `RenderView`、`RenderItem` 列表和 `RenderResourceResolver`，输出 `RenderStats` 与 `RenderDiagnostics`。

`RenderPass` 采用轻量接口，避免过早引入复杂基类层级。第一阶段可以是 `ForwardOpaquePass::Execute(RenderPassContext&)`。`RenderPassContext` 包含 view、items、resolver、stats 和 diagnostics。

`RenderStats` 至少记录：

- `SubmittedItems`
- `SkippedItems`
- `DrawCalls`
- `VisibleItems`
- `PassCount`

P3 中 `VisibleItems` 暂时等于输入 item 数，不做 culling。smoke 可以用 stats 断言真实提交数量。

Pipeline 对单个资源缺失采取 skip item 策略，记录原因后继续提交其它 item。只有 camera、target framebuffer 这类 view 级错误才导致整帧失败或返回失败状态。

### 验收

- 默认渲染路径通过 `ForwardRenderPipeline` 和至少一个 pass 执行。
- `RenderStats` 能被 `RenderingOperationsSmoke` 或 `ApplicationOperations` 观察到。
- 缺失资源不会 crash，会产生 skipped 统计和诊断。
- Editor viewport 与 runtime render path 保持一致。
- 不实现 selection outline、depth prepass、透明排序，只预留合理接口。

## P4 延后项：真正 RHI

P0-P3 不引入 `RenderDevice`、`CommandList`、`PipelineState`、`Swapchain` 等完整 RHI 概念。当前 `Renderer`、`RenderCommand`、`FrameBuffer`、`VertexArray`、`Shader` 仍然是 OpenGL-oriented backend。

推迟 RHI 的原因是当前资源生命周期和提交契约还没有稳定。先完成 P0-P3，可以把未来 RHI 替换点集中到 pipeline、resolver 和 render target 边界后面。

## 测试与验证命令

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe

cmake --build build --config Debug --target MaterialSerializationSmoke
& .\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe

cmake --build build --config Debug --target Editor
```

如果涉及 scene/resource 行为，再追加：

```powershell
& .\build\bin\Debug-Windows-x64\smoke\SceneServiceSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
```

## 非目标

- 不直接实现完整 RHI。
- 不改变 scene schema v3。
- 不实现 culling、sorting、transparent pass、selection outline、depth prepass。
- 不把 `MeshManager`、`MaterialLibrary` 一次性替换成新资产系统。
- 不把 Editor Console / ValidationService 集成作为 P0-P3 的硬性要求。

## 风险与缓解

像素验证可能因为相机、几何位置或 clear color 变化而不稳定。缓解方式是读取固定多点或小区域，而不是只依赖中心像素。

P1 过渡期可能同时存在旧 submit 路径和新 pipeline 路径。缓解方式是优先让 `ApplicationOperations::RenderSceneViewport` 与 `RenderSystem::Update` 共用新入口，再逐步删除旧 helper。

P2 仍保留 `MeshComponent::m_CachedVertexArray`，短期内不会完全消除耦合。缓解方式是在新 pipeline 中停止依赖该字段，并把删除 runtime cache 作为后续清理任务。

P3 如果 pass 接口设计过重，会提前变成 frame graph。缓解方式是第一阶段只保留一个 opaque pass 和简单 context。
