# 生命周期与事件

## 1. 启动链

共享入口仍然是 `HuaEngine/src/HuaEngine/EntryPoint.h`：

1. `main()` 调 `HE::Log::Init()`
2. `main()` 调 `HE::CreateApplication()`
3. 具体宿主返回一个 `Application` 子类
4. 宿主进入 `Application::Start()` 或随后 `Run()`

当前 `Application::Start()` 会做这些事：

- 初始化序列化系统
- 仅在 `ApplicationSpecification::EnableWindow` 为真时创建窗口
- 用 `Name / WindowWidth / WindowHeight` 构造 `WindowProps`
- 注册 `ApplicationServices`
- 构建 `ApplicationOperations`
- 为 GUI 宿主创建 `ImguiLayer`
- 在运行时外壳就绪后附着延迟 layer

## 2. 宿主 Shell 配置

`ApplicationSpecification` 现在是正式宿主 shell 配置对象。

关键字段：

- `Name`
- `EnableWindow`
- `EnableGuiLayer`
- `WindowWidth`
- `WindowHeight`

当前宿主事实：

- `Editor.exe` 是大工作台窗口
- `ProjectHub.exe` 是较小的启动器窗口
- headless 宿主完全禁用窗口 shell

## 2.1 日志面

`Log::Init()` 是共享启动契约的一部分，不是 Editor 私有工具。

当前 sink 包括：

- 适用时的控制台输出
- 供 `ConsolePanel` 读取的内存缓冲
- `%LOCALAPPDATA%/HuaEngine/Logs/<host>.log` 下的文件日志

如果启动或宿主切换失败得很早，优先查宿主日志文件，再深入运行时启动链。

## 3. 主循环顺序

对有窗口宿主，`Application::Run()` 仍按这个顺序执行：

1. 每个 layer 的 `OnUpdate()`
2. `ImguiLayer::Begin()`
3. 每个 layer 的 `OnGuiRender()`
4. `ImguiLayer::End()`
5. `Window::OnUpdate()`

这意味着：

- GUI 绘制总是在逻辑更新之后
- GLFW 事件轮询仍在 `Window::OnUpdate()`
- headless 不跑这个窗口循环，但仍共享 `Start()`、services 和 operations

## 4. Layer 与事件模型

- `PushLayer()` 插在 overlay 之前
- `PushOverlay()` 追加到最后
- 事件按 layer 逆序派发
- 任意 layer 都可以通过 `event.Handled = true` 阻断继续传播

## 5. 宿主切换

`HuaEngine/src/HuaEngine/Core/HostLaunch.*` 是宿主间进程切换桥。

当前真实用途：

- `ProjectHub.exe` 拉起 `Editor.exe --project [--scene]`

拉起成功后会通过 `Application::RequestShutdown()` 让原宿主干净退出。

## 相关 Skill

- 看工作台和启动器链路：转 `huaengine-editor-workbench/references/editor-flow.md`
- 看场景更新结构：转 `huaengine-ecs-scene/references/runtime-structure.md`
- 看渲染流：转 `huaengine-rendering/references/runtime-flow.md`
