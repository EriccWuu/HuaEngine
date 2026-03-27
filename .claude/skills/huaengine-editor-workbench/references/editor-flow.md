# Editor Flow

## 1. Editor 启动链路

入口在 `Editor/src/EditorApp.cpp`：

- `EditorApp : Application`
- 构造函数中 `PushLayer(new EditorLayer(spec))`
- 共享的 `main()` 仍来自 `HuaEngine/EntryPoint.h`

这意味着 editor 启动仍完全依赖 core runtime，但 workbench 能力入口已经从 GUI 直连回收到统一操作层。

## 2. EditorLayer 的职责

`EditorLayer` 是当前 editor 工作台的总装配点，它在构造或 `OnAttach()` 中准备：

- `EditorCamera`
- workbench project/context
- `Scene`
- scene viewport `FrameBuffer`
- `RenderSystem`
- `SceneHierarchyPanel`
- `InspectorPanel`
- `ConcolePanel`
- `EditorWorkbenchState`
- 可选默认示例场景 bootstrap（当前使用 Sandbox 风格的 mesh/material 资源链）

当前事实是：它已经被拆成“编辑器工作台壳”和“可选 demo scene bootstrap”两段职责。

## 3. OnAttach / OnUpdate / OnGuiRender

### OnAttach

当前会：

- 通过 `ApplicationOperations` 初始化 workbench project
- 检查 project 状态并创建 workbench scene
- 创建 viewport framebuffer
- 通过 `rendering.attach_scene_viewport` 建立 scene viewport render seam
- 刷新统一 validation state
- 如果启用默认 bootstrap，会加载 `assets/SandboxMaterial.material`、默认 mesh 与 `CustomMesh.mesh`，创建可渲染示例实体并保存/校验示例 scene

### OnUpdate

每帧做：

- `m_EditorCamera->OnUpdate()`
- `ApplicationOperations::RenderSceneViewport(...)`
- `m_Scene->Update()`

### OnGuiRender

当前顺序：

- `OnDockingPanel()`
- 失败时显示 `Workbench Status`
- `OnScenePanel()`
- `SceneHierarchyPanel::OnGuiRender()`
- `InspectorPanel::OnGuiRender()`
- `ConcolePanel::OnGuiRender()`

这说明 GUI 已经变成统一结果与统一操作面的可视化消费者，而不是事实上的 domain owner。

## 4. DockSpace 与 Scene Panel

`OnDockingPanel()`：

- 创建全屏主窗口
- 配置 menu bar 和 docking flags
- 调用 `ImGui::DockSpace(...)`
- 首次进入时通过 DockBuilder 固定默认布局：左 `Scene Hierarchy`、右 `Inspector`、下 `Console`、中 `Scene`

`OnScenePanel()`：

- 把 framebuffer color attachment 画到 ImGui Image
- 根据面板大小 resize framebuffer
- 把 viewport 尺寸回写给 `EditorCamera`

这说明 Scene 面板仍然是 editor 和 rendering 的主要耦合点之一，但当前耦合已经被收束到 `ApplicationOperations` 暴露的 rendering seam。

## Related Skills

- Scene 面板里的渲染路径：转到 `huaengine-rendering/references/runtime-flow.md`
- Scene/Entity/Component 的底层容器：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 主循环、ImguiLayer、Window/Input/runtime glue：转到 `huaengine-core-runtime/references/lifecycle-and-events.md`
