# Lifecycle And Events

## 1. 启动链路

统一入口在 `HuaEngine/src/HuaEngine/EntryPoint.h`：

1. `main()` 调用 `HE::Log::Init()`
2. `main()` 调用 `HE::CreateApplication()`
3. 具体宿主返回 `Application` 子类
4. 宿主进入 `Application::Start()` 或直接 `Application::Run()`

`Application::Start()` 当前会做：

- 校验并初始化 runtime
- `Serialization::InitializeSerialization()`
- 按 `ApplicationSpecification` 决定是否 `Window::Create()`
- 若有窗口则 `SetEventCallback(...)`
- 组装 `ApplicationServices`
- 创建 `ApplicationOperations`
- 若启用 GUI 则创建 `ImguiLayer` 并 `PushOverlay(m_GuiLayer)`
- 在 runtime 准备好之后再附着普通 Layer

## 2. 主循环顺序

window host 的 `Application::Run()` 当前顺序固定为：

1. 遍历 LayerStack 执行 `OnUpdate()`
2. `ImguiLayer::Begin()`
3. 遍历 LayerStack 执行 `OnGuiRender()`
4. `ImguiLayer::End()`
5. `Window::OnUpdate()`

这意味着：

- GUI 绘制始终包在每帧逻辑更新之后
- 真正的窗口事件轮询发生在 `Window::OnUpdate()` 里
- headless host 不走这个窗口循环，但仍共享同一套 `Start()`、services 和 operations 初始化边界

## 3. Layer 与 Overlay

`LayerStack` 维护一个线性容器和 `m_LayerInsertIdx`：

- `PushLayer()` 插到普通层区间尾部
- `PushOverlay()` 直接 append 到容器末尾
- `PopLayer()` / `PopOverlay()` 时会调用 `OnDetach()`
- `LayerStack` 析构时会 delete 所有 layer 指针

`Application::PushLayer/PushOverlay()` 在 runtime 已初始化之后还会立即补一次 `OnAttach()`。

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

`LogSink::sink_it_(...)` 会把格式化后的日志消息推入内存缓冲。Editor Console 的 `Logs` 视图直接读取这份缓冲。

## 7. 宿主约束

当前 runtime 有两个需要优先记住的约束：

- 宿主对核心能力的正式入口是 `Application::GetOperations()`，不是 raw services
- `ApplicationSpecification::EnableWindow / EnableGuiLayer` 只决定 runtime 外壳，不改变内部服务层与统一结果语义

## Related Skills

- Scene/System 如何被主循环消费：转到 `huaengine-ecs-scene/references/runtime-structure.md`
- 渲染提交流程如何挂在 Layer 更新里：转到 `huaengine-rendering/references/runtime-flow.md`
- Editor 如何基于这些 runtime 能力组织工作台：转到 `huaengine-editor-workbench/references/editor-flow.md`
- 序列化初始化为何发生在 runtime 启动期：转到 `huaengine-serialization-reflection/references/core-flow.md`
