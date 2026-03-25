# Lifecycle And Events

## 1. 启动链路

统一入口在 `HuaEngine/src/HuaEngine/EntryPoint.h`：

1. `main()` 调用 `HE::Log::Init()`
2. `main()` 调用 `HE::CreateApplication()`
3. 具体 app 返回 `Application` 子类
4. `Application::Run()` 进入主循环

`Application` 构造阶段当前会做：

- 断言单例唯一
- `Serialization::InitializeSerialization()`
- `Window::Create()`
- `SetEventCallback(...)`
- 创建 `ImguiLayer`
- `PushOverlay(m_GuiLayer)`

## 2. 主循环顺序

`Application::Run()` 当前顺序固定为：

1. 遍历 LayerStack 执行 `OnUpdate()`
2. `ImguiLayer::Begin()`
3. 遍历 LayerStack 执行 `OnGuiRender()`
4. `ImguiLayer::End()`
5. `Window::OnUpdate()`

这意味着：

- GUI 绘制始终包在每帧逻辑更新之后
- 真正的窗口事件轮询发生在 `Window::OnUpdate()` 里

## 3. Layer 与 Overlay

`LayerStack` 维护一个线性容器和 `m_LayerInsertIdx`：

- `PushLayer()` 插到普通层区间尾部
- `PushOverlay()` 直接 append 到容器末尾
- `PopLayer()` / `PopOverlay()` 时会调用 `OnDetach()`
- `LayerStack` 析构时会 delete 所有 layer 指针

`Application::PushLayer/PushOverlay()` 还会在压栈后立即调 `OnAttach()`。

## 4. 事件分发模型

`Application::OnEvent()` 当前分两步：

1. 先用 `EventDispatcher` 处理 `WindowCloseEvent`
2. 再按 LayerStack 逆序把事件发给层

逆序意味着：

- 后加入的 Overlay 优先收到事件
- 任意层把 `event.Handled = true` 之后，后续层不会继续收到该事件

## 5. Event 基类与宏

`Event.h` 提供：

- `EventType`
- `EventCategory`
- `EVENT_CLASS_TYPE(...)`
- `EVENT_CLASS_CATEGORY(...)`
- `EventDispatcher`

大多数新事件类型都按这套宏约定实现。

## 6. Log 与 Editor Console 的关系

`Log::Init()` 会创建：

- `ms_CoreLogger`
- `ms_ClientLogger`
- `ms_LogSink`

`LogSink::sink_it_(...)` 会把格式化后的日志消息推入内存缓冲。Editor Console 实际就是读取这份缓冲并做颜色映射显示。

## Related Skills

- Scene/System 如何被主循环消费：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 渲染提交如何挂在 Layer 更新里：转到 `huaengine-rendering/references/runtime-flow.md`
- Editor 如何基于这些 runtime 能力组织工作台：转到 `huaengine-editor-workbench/references/editor-flow.md`
- 序列化初始化为何发生在 app 构造期：转到 `huaengine-serialization-reflection/references/core-flow.md`