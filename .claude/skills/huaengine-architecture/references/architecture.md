# Architecture Reference

## 1. 构建图

项目由根 `CMakeLists.txt` 统一装配：

- 引入 `cmake/Config.cmake`
- 设置 C++20、平台宏和配置宏
- 注册第三方依赖：`glfw`、`GLAD`、`ImGui`
- 添加四类主目标：`HuaEngine`、`Sandbox`、`Editor`、`HuaEngineHeadless`
- 同时注册 smoke targets，覆盖 `Project / Scene / Asset / Script / Validation / Operations / Headless / Host Consistency`
- Visual Studio 默认启动项目仍设为 `Editor`
- 根目录构建入口已统一为 `Build.bat`，辅助构建逻辑下沉到 `Scripts/`

四类主目标的职责：

- `HuaEngine/`：静态库，承载核心 runtime、services、operations 与 rendering/scene/serialization 等能力
- `Editor/`：GUI 宿主，当前通过 `ApplicationOperations` 消费正式控制面
- `Sandbox/`：示例与验证宿主，适合验证渲染、材质、网格、场景序列化等引擎能力
- `Headless/`：无 GUI 宿主，复用同一套 runtime/service/operation contracts，并把结果以 JSON 打到 stdout

## 2. 目录与分层

### 引擎核心

`HuaEngine/src/HuaEngine/` 下当前最重要的一级子目录：

- `Application/`：`ApplicationServices`、`ApplicationOperations`、`OperationRegistry`
- `Asset/`：`AssetRegistry` 与 `AssetService`
- `Automation/`：`AgentHostAdapter`
- `Core/`：`Application`、`Window`、`Input`、`Layer`、`LayerStack`、日志与断言
- `ECS/`：`Entity`、`EntityManager`、组件与脚本实体
- `Events/`：窗口、键盘、鼠标事件定义
- `GUI/`：ImGui 层和集成代码
- `Math/`：数学辅助
- `Profiling/`：性能分析工具
- `Project/`：项目上下文与项目服务
- `Reflection/`：编译期反射实现与宏
- `Rendering/`：渲染抽象、材质、网格、Shader、FrameBuffer、RenderPipeline
- `Scene/`：`Scene` 与 `SceneSerializer`
- `Script/`：脚本生命周期与运行时服务
- `Serialization/`：后端注册、序列化核心与 JSON backend
- `Validation/`：跨五类能力的统一校验服务

### 平台与运行时模块

- `HuaEngine/src/Platform/Windows/`：窗口和输入的 Windows/GLFW 实现
- `HuaEngine/src/Platform/OpenGL/`：渲染抽象的 OpenGL 后端
- `HuaEngine/src/Module/Rendering/`：把 `Scene`、`Camera` 和渲染抽象接起来的运行时渲染系统

### 应用层

- `Editor/src/`：编辑器应用入口、主 Layer、SceneHierarchy/Inspector/Console 与 workbench state
- `Sandbox/src/`：示例应用入口
- `Headless/src/`：CLI/headless 宿主与命令分发
- `Tests/`：smoke/regression targets，直接验证正式控制面是否仍然成立

## 3. 启动与运行链路

统一入口来自 `HuaEngine/src/HuaEngine/EntryPoint.h`：

1. `main()` 调用 `HE::Log::Init()`
2. `main()` 调用 `HE::CreateApplication()`
3. 具体宿主返回 `Application` 子类
4. 宿主显式或隐式进入 `Application::Start()`
5. window host 再进入 `Application::Run()`

`Application::Start()` 当前会做：

- 初始化序列化主通路
- 按 `ApplicationSpecification` 决定是否创建窗口
- 注册 `ApplicationServices`
- 构建 `ApplicationOperations`
- 若启用 GUI，则创建 `ImguiLayer`
- 在 runtime 就绪后再附着普通 `Layer`

这意味着：宿主之间的区别主要在 window/gui 外壳，而不是核心能力边界。

## 4. Editor、Sandbox、Headless 如何接入引擎

### Editor

- `Editor/src/EditorApp.cpp` 定义 `EditorApp : Application`
- `EditorLayer` 现在通过 `ApplicationOperations` 初始化 workbench project / scene / validation
- GUI 面板通过 `EditorWorkbenchState` 消费 `ResultEnvelope / ValidationReport`
- Scene viewport 通过 `rendering.attach_scene_viewport / rendering.render_scene_viewport` 接入渲染热路径

### Sandbox

- `Sandbox/src/SandboxApp.cpp` 定义 `SandboxApp : Application`
- 仍更偏向样例与实验宿主
- 可继续直接验证渲染、材质、mesh 和场景读写

### Headless

- `Headless/src/main.cpp` 定义正式 CLI/headless 宿主
- `HeadlessCommandRunner` 只消费 `ApplicationOperations`
- 所有正式命令都返回同一套 `ResultEnvelope` 并序列化为稳定 JSON

## 5. ECS、Scene、RenderSystem 主链路

关键对象关系：

- `EntityManager` 持有 `entt::registry`
- `Scene` 持有一个 `EntityManager` 和多个 `System`
- `Scene::Update()` 只调用已注册系统的 `Update()`
- `RenderSystem` 继承 `System`，同时持有 `Scene`、`Camera`、`FrameBuffer` 引用

工作方式：

- 应用层创建 `Scene`
- 应用层创建实体并挂载组件
- 宿主或操作层通过 `rendering.attach_scene_viewport` 创建/复用 `RenderSystem`
- 每帧由 `rendering.render_scene_viewport` 驱动 `RenderSystem::RenderSingleCamera(...)`
- `Scene::Update()` 继续驱动其他已注册系统

## 6. 反射与序列化链路

当前核心事实：

- 反射基于 `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- 类型通过 `srefl_class(...)` 暴露字段元数据
- `Serialization::InitializeSerialization()` 当前注册 JSON backend
- Scene、Asset、Validation 等正式能力都建立在这条对象化主通路上

这意味着：

- 新增可序列化组件时，至少要同时检查组件定义、反射声明、序列化入口
- 只改结构体字段而不补反射，序列化层通常拿不到完整字段信息

## 7. 正式控制面边界

当前对外正式控制面不是原始的五类 service，而是：

- `ApplicationServices`：服务层内部组合根
- `ApplicationOperations`：宿主唯一公开操作入口
- `OperationRegistry`：可发现的正式操作目录
- `ResultEnvelope`：统一结果与诊断协议

这意味着：

- GUI、Headless、Agent 都应该先问“有没有对应 operation”，而不是直接找某个 raw service
- 新能力要进入正式控制面时，优先沿 `ApplicationOperations` 扩展，而不是先做 Editor 专属入口
- 如果某个行为只能在 GUI 下触发，通常说明它还没有被正式纳入控制面

## 8. 修改落点建议

- 应用主循环、窗口回调、Layer 生命周期：优先看 `Core/`、`Application.*` 与 `Application/`
- 编辑器面板与交互：优先看 `Editor/src/EditorLayer.cpp`、`Editor/src/Panels/`、`Editor/src/Workbench/`
- Headless/CLI/AI automation：优先看 `Headless/src/`、`Automation/`、`ApplicationOperations`
- 渲染行为异常：同时检查 `Rendering/`、`Module/Rendering/`、宿主侧 framebuffer/camera 配置
- 场景保存与资产读写：同时检查 `Scene/`、`Asset/`、`Serialization/`、`Validation/`

## Related Skills

- 想继续下钻渲染主路径、OpenGL 后端、材质或 mesh：转到 `huaengine-rendering`
- 想继续下钻实体、Scene、系统注册、编辑器层级树：转到 `huaengine-ecs-scene`
- 想继续下钻 `srefl_class(...)`、JSON backend、`Serializer<T>`：转到 `huaengine-serialization-reflection`
- 想看 runtime/service/operation 边界：转到 `huaengine-core-runtime`
- 想看 GUI 如何消费统一操作层：转到 `huaengine-editor-workbench`
