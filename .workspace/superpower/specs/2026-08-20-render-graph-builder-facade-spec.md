# RenderGraph Builder Facade 规格

## 状态

执行中。

## 目标

将业务侧 RenderGraph 构建收敛为 Builder facade。业务 Pass 不再直接构造 `PassGraphPassDesc`、`PassGraphResourceUsage` 或 `PassGraphRenderPassAttachment`，而是通过语义化接口声明资源、读写关系、RenderPass 附件、显式依赖、导出资源和执行回调。

## 不在范围内

- 不重写 `PassGraph::Compile()` 的依赖分析、barrier 规划、队列批次或 culling。
- 不改变 transient allocator、资源池和 fence 回收策略。
- 不接入 DX12，也不扩展异步队列的实际 RHI 提交实现。

## API 设计

`RenderGraphBuilder` 负责一次图构建：

- `ImportTexture` / `ImportBuffer`：登记图外拥有的资源。
- `CreateTexture` / `CreateBuffer`：声明由图 allocator 创建并回收的 transient 资源。
- `AddGraphicsPass` / `AddComputePass` / `AddCopyPass`：创建对应类型的 Pass。
- `Export`：声明图的最终可观察输出，作为 culling 根。

`RenderGraphPassBuilder` 负责单个 Pass 的资源语义：

- `Read` / `Write`：声明任意状态的读写访问。
- `WriteColor` / `WriteDepth`：声明 graphics attachment；底层编译器继续自动生成对应 write usage。
- `DependsOn`：添加无法从资源访问推导的显式边。
- `SetExecute`：绑定录制回调。

## 迁移步骤

1. 新增 facade 并迁移 `ForwardRenderPipeline::BuildGraph()`。
2. 迁移全部 RenderGraph smoke，验证 facade 覆盖现有编译、barrier、队列、依赖和 culling 语义。
3. 将 `PassGraph` 的低层资源、Pass 和输出构造接口转为 Builder 私有实现细节。

## 验收标准

- Forward 路径不直接调用 `PassGraph::Add*`，也不构造 `PassGraphPassDesc`。
- 三个 RenderGraph 相关 smoke 只使用 Builder 业务接口构建图。
- `RenderPassGraphSmoke`、`RHIResourceCreationSmoke`、`RHICommandListBindingSmoke` 与 `RenderingOperationsSmoke` 通过。
- `PassGraph` 保持 typed resource compile 语义和既有结果不变。
