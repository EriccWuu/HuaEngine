# RenderGraph Builder Facade 规格

## 状态

已完成。

## 目标

将业务侧 RenderGraph 构建收敛为 Builder facade。业务 Pass 不直接构造底层图描述，而是通过语义化接口声明资源、读写关系、RenderPass 附件、显式依赖、导出资源和执行回调。

## 范围

- 保持 RenderGraph 的 typed resource 编译、barrier 规划、队列批次、culling、transient allocator 和 fence 回收语义。
- 不在本次接入 DX12，也不扩展异步队列的实际 RHI 提交实现。

## API 设计

`RenderGraphBuilder` 负责整张图：

- `ImportTexture` / `ImportBuffer` 登记图外拥有的资源。
- `CreateTexture` / `CreateBuffer` 声明由图 allocator 创建和回收的 transient 资源。
- `AddPass(pass)` 注册 Pass；Pass 自身提供名称和 `RenderGraphPassType`。
- `Export` 声明图的最终可观察输出，作为 culling 根。

`RenderGraphPassBuilder` 负责单个 Pass 的资源语义：

- `Read` / `Write` 声明资源访问及状态。
- `WriteColor` / `WriteDepth` 声明 graphics attachment。
- `DependsOn` 添加无法由资源访问推导的显式依赖。

`RenderGraphPass` 统一 Pass 对象的构图与执行：

- `GetName` 和 `GetType` 描述自身。
- `Setup` 声明资源使用。
- `Execute` 录制实际命令。

Builder 支持 `Ref<RenderGraphPass>` 重载，图执行回调会持有对象，避免临时 Pass 生命周期问题。

## 目录结构

`Rendering/RenderPipeline/GraphPasses` 保存具体渲染管线 Pass 的定义和实现。目前包括：

- `BeginRendererPass`
- `ForwardOpaquePass`
- `PostProcessPass`
- `EndRendererPass`

`Rendering/RenderGraph` 承载 RenderGraph 核心、资源 allocator 和 Builder。`RenderPipeline` 承载管线策略、Forward pipeline、GraphPasses、资源解析、绑定组和渲染类型；`ForwardRenderPipeline` 只保留资源拓扑构建、图编译和提交控制流。

## 验收结果

- `PassGraph` 已完全更名为 `RenderGraph`，对应 smoke 目标改为 `RenderGraphSmoke`。
- 已消除 `PassGraph` 源码、测试和 CMake 遗留引用。
- 已验证 `RenderGraphSmoke`、`RHIResourceCreationSmoke`、`RHICommandListBindingSmoke`、`RenderingOperationsSmoke`。
