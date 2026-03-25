# Runtime Flow

## 1. 当前主渲染链路

仓库当前一帧渲染的实际主路径是：

1. 应用层或 Layer 创建 `Scene`、`RenderSystem`、`Camera`、`FrameBuffer`
2. `RenderSystem` 通过 `Scene::View<...>()` 找到带渲染组件的实体
3. `RenderSystem::RenderSingleCamera()` 绑定 FrameBuffer、清屏并调用 `Renderer::Begin(...)`
4. `Renderer::Submit(...)` 为 shader 写入 `u_ViewProjection`、`u_Transform`
5. `MaterialInstance::Bind()` 或 VertexArray 绑定完成后，`RenderCommand::DrawIndexed(...)` 下发绘制
6. `RenderCommand` 委托 `RendererAPI`
7. `RendererAPI::Create()` 当前固定返回 `OpenGLRendererAPI`
8. 最终由 `Platform/OpenGL/*` 调用 OpenGL API

## 2. RenderSystem 层

关键文件：

- `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- `HuaEngine/src/Module/Rendering/RenderingComponent.h`

当前行为：

- `Update()` 遍历场景里所有 `CameraComponent`
- 对每个相机都调用 `RenderSingleCamera(...)`
- `Primary` 标记目前没有在这里参与筛选
- 目前主提交通道依赖 `TransformComponent + MeshComponent + MaterialComponent`
- 旧的 `RendererComponent` 仍保留作兼容结构，但新的主路径已经偏向材质实例方案

实际排查顺序：

- 先看目标实体是否有 `TransformComponent`、`MeshComponent`、`MaterialComponent`
- 再看 `MeshComponent::GetVertexArray()` 是否能从缓存或 `MeshManager` 成功拿到 VAO
- 再看 `MaterialInstance` 是否有有效 shader
- 最后再看 OpenGL 后端状态

## 3. Renderer / RenderCommand / RendererAPI 分层

### Renderer

`Renderer.cpp` 当前职责很薄：

- 保存当前 Camera
- 为 shader 设置标准矩阵 uniform
- 触发 `DrawIndexed`
- 在材质路径中调用 `MaterialInstance::Bind()/Unbind()`

它目前没有：

- 批处理
- Render graph
- Render queue
- 状态缓存回收
- `End()` 阶段的实际整理逻辑

### RenderCommand

`RenderCommand` 是静态门面，负责：

- `Init()`
- `Clear()`
- `SetClearColor()`
- `SetViewport()`
- `DrawIndexed()`

### RendererAPI

`RendererAPI::m_API` 当前固定为 `OpenGL`，`Create()` 只会返回 `OpenGLRendererAPI`。

这意味着：

- 当前项目还没有真正的多后端切换流程
- 改 `RendererAPI` 相关逻辑时，基本可以默认最终行为落在 `Platform/OpenGL/`

## 4. OpenGL 后端落点

最常用入口：

- `OpenGLRendererAPI.cpp`：清屏、viewport、indexed draw
- `OpenGLShader.cpp`：源码/文件读取、`#type` 预处理、编译链接、uniform 写入
- `OpenGLFrameBuffer.cpp`：颜色附件、深度附件、resize、MRT 设置
- `OpenGLTexture2D.cpp`：纹理资源创建与绑定
- `OpenGLVertexArray.cpp` / `OpenGLVertexBuffer.cpp` / `OpenGLIndexBuffer.cpp`：几何缓冲对象

Shader 使用注意点：

- `Shader::CreateFromFile(shaderPath)` 支持组合 shader 文件，靠 `#type vertex` / `#type fragment` 分段
- `OpenGLShader::Unbind()` 当前为空
- shader 编译失败会直接打日志并触发断言

## 5. Camera 与 FrameBuffer

### Camera

- `Camera` 是极薄基类，只保存投影和视图矩阵
- `EditorCamera` 在 `OnUpdate()` 中更新投影和视图
- `GetViewProjection()` 使用 `Projection * View`

`EditorCamera` 的关键事实：

- viewport 由外部 `SetViewport()` 提供
- `UpdateProjectionMat()` 会用 `m_Viewport.x / m_Viewport.y`
- 调用侧若没保证高度大于 0，会有除零风险

### FrameBuffer

- 抽象接口在 `FrameBuffer.h`
- 工厂实现位于真实文件 `FranmeBuffer.cpp`
- OpenGL 实现在 `OpenGLFrameBuffer.cpp`

排查 FrameBuffer 问题时优先看：

1. 规格是否设置了正确的 `Width/Height/Attachments`
2. `Bind()` / `Resize()` 是否在正确时机调用
3. OpenGL 附件格式分支是否真的覆盖当前需求
4. Scene 面板或运行窗口的尺寸更新是否先于渲染调用

## 6. RenderPipeline 的当前位置

`Rendering/RenderPipeline/` 目录存在抽象类，但当前主要渲染路径仍直接由 `RenderSystem` 和 `Renderer` 驱动。

因此：

- 做现有 bug 定位时，不要先把问题归因到 `RenderPipeline`
- 如果是设计扩展或未来重构，`RenderPipeline` 才更像可扩展落点

## Related Skills

- 如果这一帧的输入数据本身就不对，例如实体没创建、组件没挂、系统没注册：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 如果问题和场景、材质或 mesh 的持久化有关：转到 `huaengine-serialization-reflection/references/extension-and-integration.md`
- 如果要先确认入口来自 `Editor` 还是 `Sandbox`：转到 `huaengine-architecture/references/architecture.md`