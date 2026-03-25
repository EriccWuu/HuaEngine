# Architecture Reference

## 1. 构建图

仓库由根 `CMakeLists.txt` 统一装配：

- 引入 `cmake/Config.cmake`
- 设置 C++17、平台宏和配置宏
- 注册第三方依赖：`glfw`、`GLAD`、`ImGui`
- 添加三个主目标：`HuaEngine`、`Sandbox`、`Editor`
- Visual Studio 默认启动项目设为 `Editor`

三个主目标的职责：

- `HuaEngine/`：静态库，聚合 `src/*.cpp`、`src/*.h`，并带上 STB Image 与 EnTT 头文件
- `Editor/`：可执行程序，链接 `HuaEngine` 和 `ImGui`
- `Sandbox/`：可执行程序，链接 `HuaEngine` 和 `ImGui`

## 2. 目录与分层

### 引擎核心

`HuaEngine/src/HuaEngine/` 下当前可见的一级子目录：

- `Core/`：`Application`、`Window`、`Input`、`Layer`、`LayerStack`、日志与断言
- `ECS/`：`Entity`、`EntityManager`、组件与脚本实体
- `Events/`：窗口、键盘、鼠标事件定义
- `GUI/`：ImGui 层和集成代码
- `Math/`：数学辅助
- `Profiling/`：性能分析工具
- `Reflection/`：编译期反射实现与宏
- `Rendering/`：渲染抽象、材质、网格、Shader、FrameBuffer、RenderPipeline
- `Scene/`：`Scene` 与 `SceneSerializer`
- `Serialization/`：后端注册、序列化核心与 JSON backend
- `Test/`：一些序列化和反射测试代码

### 平台与运行时模块

- `HuaEngine/src/Platform/Windows/`：窗口和输入的 Windows 实现
- `HuaEngine/src/Platform/OpenGL/`：渲染抽象的 OpenGL 后端
- `HuaEngine/src/Module/Rendering/`：把 `Scene`、Camera 和渲染抽象接起来的运行时渲染系统

### 应用层

- `Editor/src/`：编辑器应用入口、主 Layer、SceneHierarchy/Inspector/Console 等面板
- `Sandbox/src/`：示例应用入口，集中验证材质、网格、FrameBuffer、场景保存等能力

## 3. 启动与运行链路

统一入口来自 `HuaEngine/src/HuaEngine/EntryPoint.h`：

1. `main()` 调用 `HE::Log::Init()`
2. `main()` 调用 `HE::CreateApplication()`
3. `Editor` 或 `Sandbox` 提供具体 `CreateApplication()` 实现
4. `Application::Run()` 执行主循环

`Application` 构造阶段会做几件事：

- 校验单例实例
- 调用 `Serialization::InitializeSerialization()` 注册 JSON backend
- 创建窗口并绑定事件回调
- 创建 `ImguiLayer` 并作为 Overlay 压入 LayerStack

`Application::Run()` 的循环顺序是：

1. 遍历 LayerStack 执行 `OnUpdate()`
2. `ImguiLayer::Begin()`
3. 遍历 LayerStack 执行 `OnGuiRender()`
4. `ImguiLayer::End()`
5. `Window::OnUpdate()`

事件分发顺序：

- 先在 `Application::OnEvent()` 中处理 `WindowCloseEvent`
- 再按 LayerStack 逆序分发给各 Layer
- 任意 Layer 设置 `event.Handled = true` 会中断后续传播

## 4. Editor 与 Sandbox 如何接入引擎

### Editor

- `Editor/src/EditorApp.cpp` 定义 `EditorApp : Application`
- 构造函数中 `PushLayer(new EditorLayer())`
- `EditorLayer` 在 `OnAttach()` 中创建测试几何、纹理、FrameBuffer、Scene 与 RenderSystem
- `OnGuiRender()` 负责 DockSpace、Scene 面板、层级树和 Inspector 面板绘制

### Sandbox

- `Sandbox/src/SandboxApp.cpp` 定义 `SandboxApp : Application`
- 构造函数中 `PushLayer(new CustomLayer())`
- `CustomLayer::OnAttach()` 会加载默认网格、材质、FrameBuffer，并创建示例实体
- 同时会把 `RenderSystem` 注册进 `Scene`，并执行场景保存测试

## 5. ECS、Scene、RenderSystem 主链路

关键对象关系：

- `EntityManager` 持有 `entt::registry`
- `Scene` 持有一个 `EntityManager` 和多个 `System`
- `Scene::Update()` 只调用已注册系统的 `Update()`
- `RenderSystem` 继承 `System`，同时持有 `Scene`、Camera、FrameBuffer 引用

工作方式：

- 应用层创建 `Scene`
- 应用层创建实体并挂载组件
- 应用层创建 `RenderSystem` 并通过 `Scene::AddSyetem(...)` 注册
- 每帧由 Layer 的 `OnUpdate()` 驱动 `RenderSystem->RenderSingleCamera(...)` 与 `Scene->Update()`

## 6. 反射与序列化链路

当前核心事实：

- 反射基于 `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- 类型通过 `srefl_class(...)` 宏暴露字段元数据
- 例如 `TransformComponent` 在 `ECS/Components.h` 里声明字段，并在文件尾通过 `srefl_class(TransformComponent, ...)` 注册
- `Serialization::InitializeSerialization()` 当前只注册 JSON backend

这意味着：

- 新增可序列化组件时，至少要检查组件定义、反射声明、具体序列化调用点
- 只改结构体字段而不补反射，序列化层通常拿不到完整字段信息

## 7. 修改落点建议

- 应用主循环、窗口回调、Layer 生命周期：优先看 `Core/` 和 `Application.cpp`
- 编辑器面板与交互：优先看 `Editor/src/EditorLayer.cpp` 和 `Editor/src/Panels/`
- 示例场景、资产加载、材质试验：优先看 `Sandbox/src/SandboxApp.cpp`
- 渲染行为异常：同时检查 `Rendering/`、`Module/Rendering/`、具体 Layer 的 FrameBuffer/Camera 配置
- 场景保存与材质存档：同时检查 `Scene/`、`Serialization/`、`Reflection/` 和目标组件定义

## Related Skills

- 想继续下钻渲染主路径、OpenGL 后端、材质或 mesh：转到 `huaengine-rendering`
- 想继续下钻实体、Scene、系统注册、编辑器层级树：转到 `huaengine-ecs-scene`
- 想继续下钻 `srefl_class(...)`、JSON backend、`Serializer<T>`：转到 `huaengine-serialization-reflection`