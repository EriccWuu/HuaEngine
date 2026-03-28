# 窗口、输入与 ImGui

## 1. Window 抽象与 Windows 实现

`Window.h` 定义了抽象窗口接口：

- `OnUpdate()`
- `GetWidth()/GetHeight()`
- `SetEventCallback(...)`
- `SetVSync()/IsVSync()`
- `GetNativeWindow()`
- `Window::Create(...)`

当前平台实现是 `WindowsWindow`：

- 用 GLFW 创建原生窗口
- 构造 `OpenGLContext`
- 设置用户指针和一组 GLFW 回调
- `OnUpdate()` 中执行 `glfwPollEvents()` 和 `SwapBuffers()`

## 2. GLFW 回调到 HuaEngine Event 的映射

`WindowsWindow.cpp` 当前直接把 GLFW 回调转成：

- `WindowResizeEvent`
- `WindowMovedEvent`
- `WindowCloseEvent`
- `WindowFocusEvent`
- `WindowLostFocusEvent`
- `MouseButtonPressed/ReleasedEvent`
- `MouseMovedEvent`
- `MouseScrolledEvent`
- `KeyPressed/ReleasedEvent`
- `KeyTypedEvent`

这些事件都通过 `WindowData.EventCallback` 立即回抛给 `Application`。

## 3. Input 静态门面

`Input` 的 API 很薄：

- `IsKeyPressed(...)`
- `IsMousePressed(...)`
- `GetMouseX()`
- `GetMouseY()`

当前实例固定是 `new WindowsInput()`。

`WindowsInput` 的事实：

- 每次查询都从 `Application::GetInstance().GetWindow().GetNativeWindow()` 取 GLFWwindow
- 键盘和鼠标状态直接查 GLFW 当前状态
- 鼠标坐标也是实时查询，而不是事件缓存

## 4. ImguiLayer 在运行时中的位置

`ImguiLayer` 当前职责：

- `OnAttach()`：创建 ImGui context，开启 keyboard/gamepad/docking/viewports，绑定 GLFW/OpenGL backend
- `Begin()`：`ImGui_ImplOpenGL3_NewFrame()` + `ImGui_ImplGlfw_NewFrame()` + `ImGui::NewFrame()`
- `End()`：设置 `DisplaySize`，渲染 draw data，多 viewport 时保存/恢复当前 GLFW context
- `OnDetach()`：关闭 backend 并销毁 context

这说明：

- ImGui 帧边界是运行时层能力，不是 Editor 私有能力
- Editor 和未来其他工具程序都共享同一套 GUI 运行时胶水层

## 5. 当前值得注意的实现细节

- `ImguiLayer` 中有一大段 GLFW key -> ImGuiKey 映射工具，但当前主链路里并没有看到更高层输入适配封装
- `WindowsWindow` 里 `ms_GLFWWindowCount` 只在 Shutdown 减，没有在 Init 加
- `WindowProps` 默认尺寸当前是 `1960x1080`

## 相关 Skill

- 如果要看这些窗口/输入能力如何被 Editor 面板消费：转到 `huaengine-editor-workbench/references/editor-flow.md`
- 如果窗口尺寸和 FrameBuffer resize 联动导致渲染异常：转到 `huaengine-rendering/references/runtime-flow.md`
- 如果事件最终影响 Scene/Selection/层级树交互：转到 `huaengine-ecs-scene/references/runtime-structure.md`
