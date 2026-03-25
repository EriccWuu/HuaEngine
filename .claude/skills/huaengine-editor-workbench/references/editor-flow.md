# Editor Flow

## 1. Editor 启动链路

入口在 `Editor/src/EditorApp.cpp`：

- `EditorApp : Application`
- 构造函数中 `PushLayer(new EditorLayer())`
- 共享的 `main()` 仍来自 `HuaEngine/EntryPoint.h`

这意味着 editor 启动仍完全依赖 core runtime：

- `Log::Init()`
- `Application` 构造
- `ImguiLayer` Overlay
- `Window` 创建与事件回调

## 2. EditorLayer 的职责

`EditorLayer` 是当前 editor 工作台的总装配点，它在构造或 `OnAttach()` 中准备：

- `EditorCamera`
- `Scene`
- `RenderSystem`
- `SceneHierarchyPanel`
- `InspectorPanel`
- `ConcolePanel`
- 测试几何、shader、texture、framebuffer

当前事实是：它同时承担“编辑器工作台”和“示例场景初始化”两类职责。

## 3. OnAttach / OnUpdate / OnGuiRender

### OnAttach

当前会：

- 创建测试方块 VAO/VBO/IBO
- 构造 shader 和 texture
- 创建 framebuffer
- 创建若干实体并挂渲染组件
- 把 `RenderSystem` 注册进 Scene

### OnUpdate

每帧做：

- `m_EditorCamera->OnUpdate()`
- `m_RenderSystem->RenderSingleCamera(...)`
- `m_Scene->Update()`

### OnGuiRender

当前顺序：

- `OnDockingPanel()`
- `OnScenePanel()`
- `SceneHierarchyPanel::OnGuiRender()`
- `InspectorPanel::OnGuiRender()`
- `ConcolePanel::OnGuiRender()`

## 4. DockSpace 与 Scene Panel

`OnDockingPanel()`：

- 创建全屏主窗口
- 配置 menu bar 和 docking flags
- 调用 `ImGui::DockSpace(...)`

`OnScenePanel()`：

- 把 framebuffer color attachment 画到 ImGui Image
- 根据面板大小 resize framebuffer
- 把 viewport 尺寸回写给 `EditorCamera`

这说明 Scene 面板其实是 editor 和 rendering 的主要耦合点之一。

## Related Skills

- Scene 面板里的渲染路径：转到 `huaengine-rendering/references/runtime-flow.md`
- Scene/Entity/Component 的底层容器：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 主循环、ImguiLayer、window/input/runtime glue：转到 `huaengine-core-runtime/references/lifecycle-and-events.md`