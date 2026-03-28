# Lifecycle And Events

## 1. Startup Chain

The shared entry is still `HuaEngine/src/HuaEngine/EntryPoint.h`:

1. `main()` calls `HE::Log::Init()`
2. `main()` calls `HE::CreateApplication()`
3. the concrete host returns an `Application` subclass
4. the host enters `Application::Start()` or directly `Application::Run()`

`Application::Start()` currently:

- initializes serialization
- creates a window only when `ApplicationSpecification::EnableWindow` is true
- uses `ApplicationSpecification::Name`, `WindowWidth`, and `WindowHeight` to construct `WindowProps`
- registers `ApplicationServices`
- builds `ApplicationOperations`
- creates `ImguiLayer` for GUI hosts
- attaches deferred layers after the runtime shell is ready

## 2. Host Shell Configuration

`ApplicationSpecification` is now the formal host-shell configuration object.

Relevant fields:

- `Name`
- `EnableWindow`
- `EnableGuiLayer`
- `WindowWidth`
- `WindowHeight`

Examples:

- `Editor.exe` remains a large workbench-oriented host
- `ProjectHub.exe` now starts with a smaller launcher-oriented default window
- headless hosts disable the window shell entirely

## 3. Main Loop Order

For window-enabled hosts, `Application::Run()` still executes:

1. each layer `OnUpdate()`
2. `ImguiLayer::Begin()`
3. each layer `OnGuiRender()`
4. `ImguiLayer::End()`
5. `Window::OnUpdate()`

This means:

- GUI draw always happens after logical per-frame update
- actual GLFW event polling still happens in `Window::OnUpdate()`
- headless hosts do not use this window loop, but they still share `Start()`, services, and operations

## 4. Layer And Event Model

- `PushLayer()` inserts before overlays
- `PushOverlay()` appends at the end
- events are dispatched in reverse layer order
- any layer can stop propagation by setting `event.Handled = true`

## 5. Host Handoff

`HuaEngine/src/HuaEngine/Core/HostLaunch.*` is the shared bridge for process-to-process host transitions.

Current real use:

- `ProjectHub.exe` launches `Editor.exe --project [--scene]`

`Application::RequestShutdown()` is used so the launching host can cleanly exit after handoff succeeds.

## Related Skills

- For workbench and launcher flow, go to `huaengine-editor-workbench/references/editor-flow.md`
- For scene update structure, go to `huaengine-ecs-scene/references/runtime-structure.md`
- For rendering flow, go to `huaengine-rendering/references/runtime-flow.md`
