# 现代输入、命令与快捷键系统规格

## 1. 背景

HuaEngine 当前同时存在三套输入路径：

- `WindowsWindow` 将 GLFW 回调转换为继承式 `Event`，再按 Layer 逆序传播。
- `Input` 静态门面通过 `WindowsInput` 直接轮询 GLFW 状态。
- Editor 的 `ShortcutRegistry`、Viewport、Camera 与各 Panel 直接读取 ImGui 或 `Input`。

这导致输入所有权不清晰、同一帧状态来源不一致、快捷键显示与执行分离、上下文冲突无法确定性裁决。当前 `ShortcutRegistry` 还依赖 `unordered_map` 遍历，多个绑定命中时没有稳定胜者。

## 2. 最终目标

建立一个平台无关的现代 Input Core，并在其上建立 Editor Command、Action、Binding、Context 与 Resolver：

1. 所有平台键鼠输入先进入 `InputSystem`，形成当前帧不可变 `InputSnapshot`。
2. Editor 的离散操作统一注册为 Command，连续操作统一注册为 Action。
3. 所有快捷键和指针手势统一由 Resolver 根据上下文、优先级和消费规则裁决。
4. Panel 只声明焦点、悬停、捕获状态，并通过 Command ID 绘制或执行操作。
5. 默认绑定与用户覆盖分离，支持持久化、冲突检测和诊断。
6. 完成后 Editor 业务代码不得保留任何直接按键、鼠标或旧输入事件监听。

## 3. 非目标

- 本轮不实现完整快捷键设置窗口，只提供配置加载、保存和冲突查询 API。
- 本轮不实现游戏侧完整 Action Map、手柄方案或本地多人设备分配。
- 本轮不引入全局异步 EventBus。
- 本轮不改变 Undo/Redo、场景保存、资产 Apply、Picking 或 Camera 的用户可见行为。
- 本轮不支持按键序列，例如 `Ctrl+K, Ctrl+C`；数据结构应允许未来扩展。

## 4. 核心边界

### 4.1 Input Core

Input Core 位于 `HuaEngine/src/HuaEngine/Input`，不得依赖 Editor、ImGui、GLFW 或具体渲染后端。

核心类型：

```cpp
enum class InputDeviceType : uint8_t { Keyboard, Mouse };
enum class InputPhase : uint8_t { Pressed, Released, Repeated, Moved, Scrolled, Text };

struct InputControl {
    InputDeviceType Device = InputDeviceType::Keyboard;
    uint16_t Code = 0;
    bool operator==(const InputControl&) const = default;
};

struct RawInputEvent {
    InputControl Control;
    InputPhase Phase = InputPhase::Pressed;
    InputModifiers Modifiers = InputModifiers::None;
    glm::vec2 Value = {};
    char32_t Codepoint = 0;
    uint64_t Sequence = 0;
};
```

`InputSnapshot` 提供：

```cpp
bool IsDown(InputControl control) const;
bool WasPressed(InputControl control) const;
bool WasReleased(InputControl control) const;
bool WasRepeated(InputControl control) const;
glm::vec2 GetPointerPosition() const;
glm::vec2 GetPointerDelta() const;
glm::vec2 GetScrollDelta() const;
InputModifiers GetModifiers() const;
std::span<const RawInputEvent> GetEvents() const;
```

`InputSystem` 提供：

```cpp
void BeginFrame();
void Submit(RawInputEvent event);
void HandleFocusLost();
const InputSnapshot& FinalizeFrame();
const InputSnapshot& GetSnapshot() const;
```

约束：

- `BeginFrame()` 清空边沿状态、Pointer Delta、Scroll Delta 和事件列表，不清空 Held 状态。
- `Submit()` 按 Sequence 保持事件顺序并更新工作状态。
- `HandleFocusLost()` 为所有 Held Control 合成 Release，避免 Alt+Tab 后卡键。
- `FinalizeFrame()` 发布本帧不可变快照。
- 输入状态只允许 Application 主线程写入。

### 4.2 帧生命周期

当前 `Window::OnUpdate()` 同时 Poll 和 Present，且位于帧尾。必须拆成：

```text
InputSystem::BeginFrame
Window::PollEvents
InputSystem::FinalizeFrame
Publish InputSnapshot to FrameContext
Layer::OnUpdate
Gui Begin / Render / End
Window::Present
```

Editor Command Resolver 和连续 Action 必须读取同一个 Snapshot。ECS 通过 `FrameContext` 消费只读 Input Snapshot，不允许 System 直接访问平台窗口。

### 4.3 平台与 ImGui Bridge

- GLFW Key、Mouse、Scroll、Cursor、Char 回调统一转换为 `RawInputEvent`。
- 主窗口和 ImGui detached viewport 都必须注册到同一个平台输入路由。
- 回调链中的每个原始事件只能提交给 `InputSystem` 一次。
- `ImguiInputBridge` 将同一输入源转换给 ImGui；Input Core 不包含 ImGui 类型。
- 主窗口失焦必须调用 `InputSystem::HandleFocusLost()`。
- WindowClose、Resize、Focus 等应用生命周期事件继续走现有 `Event` 边界。

### 4.4 Command 与 Action

离散编辑器操作使用 Command：

```cpp
struct EditorCommandDescriptor {
    std::string Id;
    std::string DisplayName;
    std::string Category;
    std::function<bool()> CanExecute;
    std::function<void()> Execute;
};
```

连续编辑器操作使用 Action：

```cpp
struct EditorActionState {
    glm::vec3 CameraMove = {};
    glm::vec2 CameraLook = {};
    glm::vec2 CameraPan = {};
    float CameraDolly = 0.0f;
    bool CameraLooking = false;
    bool CameraPanning = false;
};
```

Command 适用于 Save、Undo、Delete、W/E/R 等一次触发操作。Action 适用于 WASD、鼠标拖动、滚轮等持续或模拟量操作。不得把持续操作伪装成每帧 Command。

### 4.5 Binding

```cpp
enum class InputTrigger : uint8_t { Pressed, Released, Repeated, Held, DoublePressed };

struct InputGesture {
    InputControl Primary;
    InputModifiers Modifiers = InputModifiers::None;
    InputTrigger Trigger = InputTrigger::Pressed;
    bool ExactModifiers = true;
};

struct InputBinding {
    std::string Id;
    std::string CommandId;
    std::string ContextId;
    InputGesture Gesture;
    int Priority = 0;
    bool Consume = true;
    BindingSource Source = BindingSource::Default;
};
```

Action Binding 使用相同 Context 和 Control，但额外提供 Scale。Binding 不存储执行闭包，Command Registry 与 Binding Registry 必须分离。

### 4.6 Context

内置上下文由高到低为：

```text
ShortcutCapture
Modal
TextInput
SceneViewportCapture
GameViewportCapture
SceneViewport
GameViewport
Hierarchy
Inspector
Project
Global
```

规则：

- 键盘上下文由当前获得键盘焦点的窗口决定。
- 指针上下文由悬停窗口和显式 Capture 决定。
- Capture 在鼠标释放前持续有效，不因移出窗口而丢失。
- `ShortcutCapture`、`Modal`、`TextInput` 默认阻断下层键盘命令。
- SceneViewport 与 GameViewport 同时存在时只能有一个键盘上下文激活。
- Panel 每帧只上报 Context 状态，不自行解析按键。

### 4.7 Resolver 与冲突规则

Resolver 是 Command 与 Action 的唯一输入入口：

1. 按激活 Context 从高到低收集候选。
2. 同一 Context 内按 Priority、注册序号稳定排序。
3. 跳过 `CanExecute == false` 的 Command。
4. 执行第一个可执行候选；`Consume == true` 时停止继续传播。
5. 相同 Gesture 在互斥 Context 中允许共存。
6. 同一 Context、Gesture、Priority 存在多个绑定时属于硬冲突，不执行任何一个，并产生 Diagnostic。
7. 禁止通过容器遍历顺序决定胜者。

### 4.8 UI 边界

允许保留的 UI 语义 API：

- `ImGui::Button`、`MenuItem`、`Selectable`、Combo 等控件返回值。
- `IsItemHovered`、`IsWindowFocused`、`IsWindowHovered` 等上下文状态查询。
- ImGuizmo 自身的 `IsUsing`、`IsOver` 等工具状态。

Editor 业务代码禁止：

- `ImGui::IsKey*`
- `ImGui::IsMouse*`
- `Input::Is*`、`Input::GetMouse*`
- 键鼠 `EventDispatcher`
- `KeyPressedEvent`、`KeyReleasedEvent`、`MouseScrolledEvent` 等输入 Event 消费
- 直接依赖 GLFW Key 或 Mouse 常量

Project 双击、Hierarchy 空白点击、右键选中、Scene Picking 等交互也必须通过 Resolver 产生的指针 Action/Command，不得作为例外保留直接监听。

### 4.9 Command UI

菜单和上下文菜单通过 Command ID 绘制：

```cpp
bool DrawCommandMenuItem(EditorCommandService& commands, std::string_view commandId);
```

该函数统一读取 DisplayName、Binding 显示文本、CanExecute、Tooltip 并调用 Execute。禁止菜单手写 `"Ctrl+S"` 等快捷键字符串。

Command 按领域注册：

- Editor Core：Save、Undo、Redo。
- Scene：Create/Delete Entity、Add/Remove Component。
- Scene Viewport：Translate、Rotate、Scale、Frame Selected。
- Project：Refresh、Reimport、Open Scene/Shader。

### 4.10 用户配置

- 默认绑定由 Editor 代码注册。
- 用户覆盖文件位于 `%LOCALAPPDATA%/HuaEngine/Editor/input-bindings.json`。
- 文件只保存相对默认值的覆盖项，不复制完整默认表。
- 未知 Command、无效 Control、非法 Modifier 和硬冲突必须产生 Diagnostic。
- 无效覆盖被忽略并回退默认绑定。
- 本轮提供 Load、Save、Reset Override、Find Conflicts API，不实现完整设置 UI。

## 5. 目录结构

```text
HuaEngine/src/HuaEngine/Input/
  InputTypes.h
  InputSnapshot.h
  InputSnapshot.cpp
  InputSystem.h
  InputSystem.cpp

HuaEngine/src/Platform/Windows/
  GlfwInputTranslator.h
  GlfwInputTranslator.cpp

Editor/src/Input/
  EditorCommandRegistry.h/.cpp
  EditorInputBindingRegistry.h/.cpp
  EditorInputContextService.h/.cpp
  EditorInputResolver.h/.cpp
  EditorInputBindingStorage.h/.cpp
  EditorCommandUi.h/.cpp
  EditorInputService.h/.cpp
```

`EditorInteractionHost` 不再持有 `ShortcutRegistry`。`ShortcutRegistry.h/.cpp` 在迁移完成后删除。

## 6. 数据流

```text
GLFW callback
  -> GlfwInputTranslator
  -> InputSystem::Submit
  -> InputSnapshot
  -> EditorInputContextService + EditorInputResolver
      -> EditorCommandRegistry::Execute
      -> EditorActionState
          -> EditorCameraController
```

菜单点击不经过 Binding Resolver，但仍调用同一个 Command Registry，因此菜单、快捷键和命令执行条件只有一份定义。

## 7. 实施阶段

### P1：Input Core 与帧快照

- 建立平台无关 Input 类型、Snapshot 与 System。
- 使用合成事件验证边沿状态、Delta、Repeat、顺序与失焦释放。
- 将 InputSnapshot 发布到 ECS FrameContext。
- 独立提交。

### P2：Window 帧顺序与 GLFW/ImGui Bridge

- 拆分 PollEvents 与 Present。
- 接入 GLFW Translator 与多窗口输入路由。
- 调整 Application 帧顺序。
- 保留旧 Input/Event 适配，确保过渡期行为可用。
- 独立提交。

### P3：Editor Command、Binding、Context 与 Resolver

- 建立纯 C++ Editor 输入核心。
- 验证上下文、优先级、消费、禁用候选、硬冲突与绑定文本。
- 建立用户覆盖配置模型与存储。
- 独立提交。

### P4：全局命令与菜单迁移

- 迁移 Save、Undo、Redo、Create/Delete Entity。
- 菜单和 Context Menu 改为查询 Command/Binding。
- 删除旧 ShortcutRegistry。
- 独立提交。

### P5：Viewport、Camera 与 Panel 指针操作迁移

- 迁移 Gizmo W/E/R。
- 迁移 Camera WASD、Look、Pan、Dolly。
- 迁移 Scene Picking、Project 双击、Hierarchy 点击等直接鼠标监听。
- 建立 Scene/Game Viewport 互斥与 Capture。
- 独立提交。

### P6：清理、配置与全量验证

- 删除 Editor 对旧 Input Event 和静态 Input 门面的全部依赖。
- 删除未使用的 ImGui Key 映射与旧 WindowsInput 实现。
- 对禁止模式执行结构扫描 smoke。
- 执行 Debug 全量构建和 CMake 当前定义的全部 smoke。
- 独立提交。

## 8. 测试策略

新增 `InputSystemSmoke`：

- Press、Hold、Release 跨帧状态。
- Repeat 不重复产生 Pressed。
- Pointer Delta 与 Scroll 每帧清零。
- Focus Lost 合成 Release。
- 事件 Sequence 稳定。

新增 `EditorInputSmoke`：

- Context 遮蔽与 Global fallback。
- Priority 与 Consume。
- 禁用 Command 后继续查找。
- 同 Context 硬冲突不执行并产生 Diagnostic。
- 互斥 Context 允许同键。
- 用户覆盖序列化和无效覆盖回退。
- Action 解析与鼠标 Capture。

扩展 `EditorInteractionSmoke`：

- Command 注册与执行。
- 菜单快捷键文本来自 Binding Registry。
- CameraController 只消费 `EditorActionState`。

结构扫描：

```powershell
rg -n "ImGui::Is(Key|Mouse)|Input::(Is|GetMouse)|KeyPressedEvent|KeyReleasedEvent|MouseScrolledEvent|ShortcutRegistry" Editor/src
```

最终结果必须无匹配。允许的 `ImGui::IsItemHovered`、`IsWindowFocused` 等 UI 上下文状态不在禁止范围内。

## 9. 验收条件

1. Input Core 不依赖 Editor、ImGui、GLFW 或渲染后端。
2. `glfwPollEvents()` 在业务 Update 前执行，输入在当前帧生效。
3. Editor 的 Command 与 Action 只由 Resolver 消费输入。
4. 快捷键冲突具有确定结果和可查询 Diagnostic。
5. 菜单显示绑定与实际触发绑定来自同一 Registry。
6. TextInput、Modal、Scene/Game Viewport 与 Capture 上下文正确阻断或接管输入。
7. Editor Camera 不直接访问 Event、Input、ImGui 或 GLFW。
8. Project、Hierarchy、Inspector、Viewport 不保留直接 Key/Mouse 监听。
9. `ShortcutRegistry`、`WindowsInput` 和重复 ImGui Key 映射被删除。
10. 用户绑定覆盖可加载、保存，并在无效时回退默认值。
11. Debug 全量构建通过。
12. CMake 当前定义的全部 smoke 通过。

## 10. 风险控制

- ImGui Multi-Viewport 会创建额外 GLFW Window，输入路由必须覆盖这些窗口，否则 detached Panel 会丢失快捷键。
- 调整 PollEvents 顺序可能暴露原先的一帧延迟依赖，必须以 InputSystem 合成 smoke 和 Editor 集成 smoke 固定新语义。
- Mouse Capture 必须使用显式生命周期，不能只依赖 Hover，否则拖动移出 Viewport 会中断。
- 用户覆盖配置不进入项目，不参与 Asset Library 或版本控制。
- 每个 P 只迁移明确范围，不同时重构无关的 Event、ECS 或 GUI 架构。
