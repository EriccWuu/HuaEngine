# 现代输入、命令与快捷键系统实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立平台无关 Input Core 和统一 Editor Command/Action/Binding/Context Resolver，并清除 Editor 中所有直接输入监听。

**Architecture:** GLFW 或 GUI Bridge 只负责产生平台无关 `RawInputEvent`，`InputSystem` 在帧开始发布不可变 `InputSnapshot`。Editor 通过 `EditorInputService` 将 Snapshot 解析为离散 Command 和连续 Action，Panel 只声明 Context 并消费解析结果。

**Tech Stack:** C++20、GLFW、Dear ImGui、HuaEngine Event/FrameContext、JSON Serialization、CMake smoke targets。

**Spec:** `.workspace/superpower/specs/2026-09-01-modern-input-command-system-spec.md`

## 全局约束

- Input Core 不得依赖 Editor、ImGui、GLFW 或图形后端。
- Editor 业务代码最终不得出现 `ImGui::IsKey*`、`ImGui::IsMouse*`、`Input::Is*`、`Input::GetMouse*` 或键鼠 Event 消费。
- UI Widget 返回值和 Hover/Focus 状态查询允许保留。
- 默认绑定与用户覆盖分离；覆盖文件写入 `%LOCALAPPDATA%/HuaEngine/Editor/input-bindings.json`。
- 每个 P 完成定向验证后独立提交。
- 不覆盖或提交执行期间发现的无关工作区变更。

---

### P1：Input Core 与帧快照

**Files:**
- Create: `HuaEngine/src/HuaEngine/Input/InputTypes.h`
- Create: `HuaEngine/src/HuaEngine/Input/InputSnapshot.h`
- Create: `HuaEngine/src/HuaEngine/Input/InputSnapshot.cpp`
- Create: `HuaEngine/src/HuaEngine/Input/InputSystem.h`
- Create: `HuaEngine/src/HuaEngine/Input/InputSystem.cpp`
- Create: `Tests/InputSystemSmoke.cpp`
- Modify: `HuaEngine/src/HuaEngine.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `InputControl`、`InputGesture` 基础类型、`RawInputEvent`、`InputSnapshot`、`InputSystem`。
- Produces: `InputSystem::BeginFrame`、`Submit`、`HandleFocusLost`、`FinalizeFrame`、`GetSnapshot`。

- [ ] **Step 1: 写 InputSystem RED smoke**

测试使用合成事件验证跨帧 Press/Hold/Release、Repeat、Pointer Delta、Scroll、DoublePressed 和 Focus Lost：

```cpp
HE::InputSystem input;
input.BeginFrame();
input.Submit(HE::RawInputEvent::Key(HE::Key::W, HE::InputPhase::Pressed));
const auto& first = input.FinalizeFrame();
Require(first.WasPressed(HE::KeyboardControl(HE::Key::W)), "Expected W press edge");
Require(first.IsDown(HE::KeyboardControl(HE::Key::W)), "Expected W held state");

input.BeginFrame();
const auto& held = input.FinalizeFrame();
Require(!held.WasPressed(HE::KeyboardControl(HE::Key::W)) && held.IsDown(HE::KeyboardControl(HE::Key::W)), "Expected held state without repeated press edge");
```

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target InputSystemSmoke --parallel 4
```

预期：缺少 Input Core 类型或目标。

- [ ] **Step 3: 实现 InputTypes、Snapshot 与 System**

使用定长 Key/Mouse 状态数组和本帧事件向量，禁止通过平台轮询补状态。`HandleFocusLost()` 遍历 Down 状态并填充 Released。

- [ ] **Step 4: 验证 GREEN**

```powershell
cmake --build build --config Debug --target InputSystemSmoke --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\InputSystemSmoke.exe
```

- [ ] **Step 5: 提交 P1**

```powershell
git add CMakeLists.txt HuaEngine/src/HuaEngine/Input HuaEngine/src/HuaEngine.h Tests/InputSystemSmoke.cpp
git commit -m "feat(input): add frame input snapshots"
```

---

### P2：Window 帧顺序与平台输入桥

**Files:**
- Create: `HuaEngine/src/Platform/Windows/GlfwInputTranslator.h`
- Create: `HuaEngine/src/Platform/Windows/GlfwInputTranslator.cpp`
- Create: `HuaEngine/src/HuaEngine/GUI/ImguiInputBridge.h`
- Create: `HuaEngine/src/HuaEngine/GUI/ImguiInputBridge.cpp`
- Modify: `HuaEngine/src/HuaEngine/Core/Window.h`
- Modify: `HuaEngine/src/Platform/Windows/WindowsWindow.h`
- Modify: `HuaEngine/src/Platform/Windows/WindowsWindow.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application.h`
- Modify: `HuaEngine/src/HuaEngine/Application.cpp`
- Modify: `HuaEngine/src/HuaEngine/GUI/ImguiLayer.h`
- Modify: `HuaEngine/src/HuaEngine/GUI/ImguiLayer.cpp`
- Modify: `Tests/InputSystemSmoke.cpp`

**Interfaces:**
- Consumes: `InputSystem::Submit`。
- Produces: `Window::PollEvents`、`Window::Present`、`Window::SetInputCallback`。
- Produces: `Application::GetInputSnapshot()` 与 `GetInputSystem()`。

- [ ] **Step 1: 增加 Translator 与主循环结构 RED 契约**

在 `InputSystemSmoke` 中验证 GLFW Key/Mouse/Modifier 翻译，并读取 `Application.cpp` 断言 `PollEvents` 位于 Layer Update 前、`Present` 位于 GUI 后。

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target InputSystemSmoke --parallel 4
```

- [ ] **Step 3: 拆分 Window 并接入 Application InputSystem**

Application 一帧顺序固定为：

```cpp
m_InputSystem.BeginFrame();
m_Window->PollEvents();
if (m_GuiLayer) m_GuiLayer->PrepareInput(m_InputSystem);
m_InputSystem.FinalizeFrame();
for (auto layer : m_LayerStack) layer->OnUpdate();
// GUI render
m_Window->Present();
```

- [ ] **Step 4: 实现 ImguiInputBridge 状态对齐**

Bridge 仅位于 GUI 层，对 ImGui secondary viewport 聚合状态和主窗口已提交状态做差异对齐，只有状态不同才向 InputSystem 提交事件，避免主窗口重复 Press。

- [ ] **Step 5: 验证 P2**

```powershell
cmake --build build --config Debug --target InputSystemSmoke ApplicationServicesSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\InputSystemSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ApplicationServicesSmoke.exe
```

- [ ] **Step 6: 提交 P2**

```powershell
git add HuaEngine/src/HuaEngine/Application.* HuaEngine/src/HuaEngine/Core/Window.h HuaEngine/src/HuaEngine/GUI HuaEngine/src/Platform/Windows Tests/InputSystemSmoke.cpp
git commit -m "refactor(input): establish current-frame platform input"
```

---

### P3：Editor Command、Binding、Context 与 Resolver

**Files:**
- Create: `Editor/src/Input/EditorCommandRegistry.h`
- Create: `Editor/src/Input/EditorCommandRegistry.cpp`
- Create: `Editor/src/Input/EditorInputBindingRegistry.h`
- Create: `Editor/src/Input/EditorInputBindingRegistry.cpp`
- Create: `Editor/src/Input/EditorInputContextService.h`
- Create: `Editor/src/Input/EditorInputContextService.cpp`
- Create: `Editor/src/Input/EditorInputResolver.h`
- Create: `Editor/src/Input/EditorInputResolver.cpp`
- Create: `Editor/src/Input/EditorInputBindingStorage.h`
- Create: `Editor/src/Input/EditorInputBindingStorage.cpp`
- Create: `Editor/src/Input/EditorInputService.h`
- Create: `Editor/src/Input/EditorInputService.cpp`
- Create: `Tests/EditorInputSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `EditorCommandRegistry::Register/Find/Execute/Clear`。
- Produces: `EditorInputBindingRegistry::RegisterDefault/SetOverride/FindBinding/FindConflicts/GetDisplayText`。
- Produces: `EditorInputContextService::BeginFrame/Activate/SetKeyboardContext/SetPointerContext/CapturePointer/ReleasePointer`。
- Produces: `EditorInputService::BeginFrame/Resolve/WasActionTriggered/GetActionState`。

- [ ] **Step 1: 写 EditorInputSmoke RED**

覆盖：Context 遮蔽、Global fallback、Priority、Consume、CanExecute、硬冲突、互斥 Context 同键、Pointer Capture、Action Scale。

```cpp
commands.Register({ "editor.save", "Save", "Editor", [] { return true; }, [&] { ++saveCount; } });
bindings.RegisterDefault(CommandBinding("editor.save", "Global", Ctrl(Key::S)));
contexts.SetKeyboardContext("SceneViewport");
service.Resolve(snapshot);
Require(saveCount == 1, "Expected global save fallback");
```

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target EditorInputSmoke --parallel 4
```

- [ ] **Step 3: 实现 Registry、Context 与确定性 Resolver**

Binding 使用 vector 和显式 RegistrationOrder。硬冲突时不执行，并返回 `ResultEnvelope` Diagnostic `editor.input.binding_conflict`。

- [ ] **Step 4: 实现用户覆盖存储**

存储路径为 `%LOCALAPPDATA%/HuaEngine/Editor/input-bindings.json`，测试通过显式临时路径调用 Load/Save，禁止污染真实用户目录。

- [ ] **Step 5: 验证 P3**

```powershell
cmake --build build --config Debug --target EditorInputSmoke EditorInteractionSmoke --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\EditorInputSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
```

- [ ] **Step 6: 提交 P3**

```powershell
git add CMakeLists.txt Editor/src/Input Tests/EditorInputSmoke.cpp
git commit -m "feat(editor): add contextual input command resolver"
```

---

### P4：全局命令与菜单迁移

**Files:**
- Create: `Editor/src/Input/EditorCommandUi.h`
- Create: `Editor/src/Input/EditorCommandUi.cpp`
- Modify: `Editor/src/Interaction/EditorInteractionHost.h`
- Modify: `Editor/src/Interaction/EditorInteractionHost.cpp`
- Delete: `Editor/src/Interaction/ShortcutRegistry.h`
- Delete: `Editor/src/Interaction/ShortcutRegistry.cpp`
- Modify: `Editor/src/Interaction/ContextMenuRegistry.h`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Editor/src/Panels/HierarchyPanel.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `DrawCommandMenuItem(EditorInputService&, std::string_view)`。
- Removes: `EditorInteractionHost::Shortcuts()` 与 `ShortcutRegistry`。

- [ ] **Step 1: 将 EditorInteractionSmoke 改为新 Command/Binding RED**

测试注册 `editor.test.undo`，提交 Ctrl+Z Snapshot，验证 Resolver 触发，并验证显示文本为 `Ctrl+Z`。

- [ ] **Step 2: 构建并确认 RED**

- [ ] **Step 3: 在 EditorLayer 注册核心 Command 与默认 Binding**

迁移 `editor.undo`、`editor.redo`、`editor.entity.create`、`editor.entity.delete`、`editor.scene.save`。Command 的 CanExecute 和 Execute 沿用现有闭包语义。

- [ ] **Step 4: 菜单和 Context Menu 使用 Command ID**

`ContextMenuActionDescriptor` 改为保存 CommandId；Hierarchy 绘制时从 EditorInputService 查询标签、快捷键和 Enabled。

- [ ] **Step 5: 删除 ShortcutRegistry 并验证**

```powershell
cmake --build build --config Debug --target EditorInteractionSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
rg -n "ShortcutRegistry|\.Shortcuts\(\)" Editor/src
```

预期：无匹配。

- [ ] **Step 6: 提交 P4**

```powershell
git add -A Editor/src/Interaction Editor/src/Input Editor/src/Panels/HierarchyPanel.cpp Editor/src/EditorLayer.* Tests/EditorInteractionSmoke.cpp CMakeLists.txt
git commit -m "refactor(editor): route global shortcuts through commands"
```

---

### P5：Viewport、Camera 与 Panel 指针迁移

**Files:**
- Modify: `Editor/src/Input/EditorInputService.h`
- Modify: `Editor/src/Input/EditorInputService.cpp`
- Modify: `Editor/src/Viewport/EditorCameraController.h`
- Modify: `Editor/src/Viewport/EditorCameraController.cpp`
- Modify: `Editor/src/Panels/HierarchyPanel.h`
- Modify: `Editor/src/Panels/HierarchyPanel.cpp`
- Modify: `Editor/src/Panels/ProjectPanel.h`
- Modify: `Editor/src/Panels/ProjectPanel.cpp`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/EditorInputSmoke.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`

**Interfaces:**
- Produces: `EditorCameraController::Update(const EditorActionState&, float deltaTime)`。
- Removes: `EditorCameraController::OnEvent` 与 `Update(bool)`。

- [ ] **Step 1: 写 Camera 与 Pointer Action RED**

```cpp
HE::Editor::EditorActionState action;
action.CameraMove.z = 1.0f;
Require(camera.Update(action, 1.0f), "Expected camera action to change pose");
```

同时对源码做结构断言，要求 CameraController 不包含 `Input::`、`EventDispatcher` 或 `ImGui::`。

- [ ] **Step 2: 构建并确认 RED**

- [ ] **Step 3: 迁移 Camera 与 Gizmo**

默认绑定：W/E/R 为 `SceneViewport` Command；WASD、Mouse Right Delta、Mouse Middle Delta、Wheel 为 SceneViewport Action。Ctrl/Alt/Super 激活时 CameraMove 为零。

- [ ] **Step 4: 迁移 Scene Picking**

`editor.scene.pick` 使用 SceneViewport 下 MouseLeft Pressed。EditorLayer 继续用 Image Hover 和 Viewport Origin 计算像素，但只在 Resolver 报告该 Action 时读取 Snapshot PointerPosition。

- [ ] **Step 5: 迁移 Project 与 Hierarchy 指针操作**

- `editor.project.open_item`：Project Context + MouseLeft DoublePressed。
- `editor.hierarchy.clear_selection`：Hierarchy Context + MouseLeft Pressed。
- `editor.hierarchy.select_context`：Hierarchy Context + MouseRight Pressed。
- Ctrl 多选从 Snapshot Modifier 读取，不读取 `ImGuiIO::KeyCtrl`。

- [ ] **Step 6: 验证 P5**

```powershell
cmake --build build --config Debug --target EditorInputSmoke EditorInteractionSmoke ProjectPanelActionSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\EditorInputSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ProjectPanelActionSmoke.exe
```

- [ ] **Step 7: 提交 P5**

```powershell
git add Editor/src/Input Editor/src/Viewport Editor/src/Panels/HierarchyPanel.* Editor/src/Panels/ProjectPanel.* Editor/src/EditorLayer.* Tests
git commit -m "refactor(editor): migrate viewport input actions"
```

---

### P6：彻底清理、配置与全量验证

**Files:**
- Delete: `HuaEngine/src/Platform/Windows/WindowsInput.h`
- Delete: `HuaEngine/src/Platform/Windows/WindowsInput.cpp`
- Modify: `HuaEngine/src/HuaEngine/Core/Input.h`
- Modify: `HuaEngine/src/HuaEngine/GUI/ImguiLayer.cpp`
- Modify: `HuaEngine/src/HuaEngine.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/EditorInputSmoke.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Removes: 旧静态 `Input` 平台实现和 Editor 输入 Event 适配。
- Produces: 最终结构扫描 smoke。

- [ ] **Step 1: 添加禁止模式 RED 结构测试**

测试递归读取 `Editor/src` 的 `.h/.cpp`，禁止以下正则：

```text
ImGui::IsKey
ImGui::IsMouse
Input::Is
Input::GetMouse
KeyPressedEvent
KeyReleasedEvent
MouseScrolledEvent
ShortcutRegistry
```

- [ ] **Step 2: 清理全部匹配**

删除 EditorLayer 输入 OnEvent 分支、旧 WindowsInput、ImguiLayer 未使用 `KeyToImGuiKey`，并将保留的 Window Event OnEvent 限定为生命周期事件。

- [ ] **Step 3: 验证禁止模式**

```powershell
rg -n "ImGui::Is(Key|Mouse)|Input::(Is|GetMouse)|KeyPressedEvent|KeyReleasedEvent|MouseScrolledEvent|ShortcutRegistry" Editor/src
```

预期：无匹配。

- [ ] **Step 4: 完整构建**

```powershell
cmake --build build --config Debug --parallel 4
```

- [ ] **Step 5: 运行 CMake 当前注册的全部 smoke**

从 `configure_smoke_target(...)` 提取目标并逐个执行，要求失败数为 0。

- [ ] **Step 6: 最终检查并提交 P6**

```powershell
git diff --check
git status --short
git add -A HuaEngine/src Editor/src Tests CMakeLists.txt
git commit -m "refactor(input): complete unified editor input migration"
```

---

## 完成检查

- [ ] P1-P6 各有独立提交。
- [ ] InputSystem smoke 覆盖输入帧语义。
- [ ] EditorInput smoke 覆盖 Context、冲突、消费和 Action。
- [ ] Window 在 Update 前 Poll、帧末 Present。
- [ ] Editor Menu 与 Shortcut 使用同一 Command/Binding 来源。
- [ ] Camera、Gizmo、Picking、Project、Hierarchy 全部走 Resolver。
- [ ] Editor 源码禁止模式扫描无匹配。
- [ ] `ShortcutRegistry` 与 `WindowsInput` 已删除。
- [ ] 用户覆盖配置支持 Load/Save/Reset 与无效回退。
- [ ] Debug 全量构建成功。
- [ ] CMake 当前定义的全部 smoke 通过。
