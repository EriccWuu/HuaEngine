# P30 Command Buffer / Queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RHI 增加最小 `CommandBuffer` / `RenderQueue` 提交模型骨架，同时保留现有 immediate command list 路径。

**Architecture:** 新增 `CommandSubmission.h` 定义命令缓冲与图形队列接口；`RenderDevice` 暴露 `CreateCommandBuffer()` 与 `GetGraphicsQueue()`。OpenGL backend 先实现 immediate-backed 空命令缓冲和同步 queue submit，不迁移 Forward 主路径，不引入真实录制。

**Tech Stack:** C++20、CMake、OpenGL backend、现有 smoke 测试框架。

---

## 文件结构

- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandSubmission.h`
  - 定义 `CommandBufferUsage`、`CommandBufferDesc`、`CommandBuffer`、`RenderQueue`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/RenderDevice.h`
  - include command submission 头文件。
  - 增加 `CreateCommandBuffer` 和 `GetGraphicsQueue` 纯虚接口。
  - capabilities 增加 `SupportsCommandSubmission`。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
  - 增加 `OpenGLCommandBuffer`、`OpenGLRenderQueue`。
  - `OpenGLRenderDevice` 持有 queue 并实现新接口。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - 实现最小 command buffer/queue。
  - OpenGL queue submit 对 empty command buffer 为同步 no-op。
- Modify: `Tests/RHIResourceCreationSmoke.cpp`
  - TDD 覆盖 command submission capability、command buffer 创建、queue submit。
- Modify: `CMakeLists.txt`
  - 如果复用 `RHIResourceCreationSmoke`，无需新增 target。
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
  - 追加 P30 实现结果。

## Task 1: RED - command submission smoke

**Files:**
- Modify: `Tests/RHIResourceCreationSmoke.cpp`

- [ ] **Step 1: 写失败测试**

在 capability 检查后加入：

```cpp
Require(device.GetCapabilities().SupportsCommandSubmission, "Expected command submission support capability");
```

在 bind group 创建验证附近加入：

```cpp
auto commandBuffer = device.CreateCommandBuffer({
	.Usage = HE::Rendering::CommandBufferUsage::Graphics,
	.DebugName = "RHIResourceCreationSmoke empty command buffer"
});
Require(static_cast<bool>(commandBuffer), "Expected command buffer creation to succeed");
Require(commandBuffer->GetDesc().Usage == HE::Rendering::CommandBufferUsage::Graphics, "Expected graphics command buffer usage");
Require(commandBuffer->GetDesc().DebugName == "RHIResourceCreationSmoke empty command buffer", "Expected command buffer debug name");
device.GetGraphicsQueue().Submit(*commandBuffer);

Require(!device.CreateCommandBuffer({ .Usage = HE::Rendering::CommandBufferUsage::Invalid }), "Expected invalid command buffer creation to fail");
```

- [ ] **Step 2: 运行并确认失败**

Run:

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
```

Expected: 编译失败，提示 `SupportsCommandSubmission`、`CreateCommandBuffer`、`CommandBufferUsage` 或 `GetGraphicsQueue` 未定义。

## Task 2: GREEN - public RHI submission API

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandSubmission.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/RenderDevice.h`

- [ ] **Step 1: 新增 RHI command submission 类型**

`CommandSubmission.h` 内容：

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace HE::Rendering {
	enum class CommandBufferUsage : uint8_t {
		Invalid = 0,
		Graphics
	};

	struct CommandBufferDesc {
		CommandBufferUsage Usage = CommandBufferUsage::Graphics;
		std::string DebugName;
	};

	class CommandBuffer {
	public:
		virtual ~CommandBuffer() = default;

		virtual const CommandBufferDesc& GetDesc() const = 0;
	};

	class RenderQueue {
	public:
		virtual ~RenderQueue() = default;

		virtual void Submit(CommandBuffer& commandBuffer) = 0;
	};
}
```

- [ ] **Step 2: 扩展 RenderDevice**

在 `RenderDevice.h` include 列表加入：

```cpp
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"
```

在 `RenderDeviceCapabilities` 加入：

```cpp
bool SupportsCommandSubmission = true;
```

在 `RenderDevice` public 接口中加入：

```cpp
virtual Ref<CommandBuffer> CreateCommandBuffer(const CommandBufferDesc& desc) = 0;
virtual RenderQueue& GetGraphicsQueue() = 0;
```

## Task 3: GREEN - OpenGL immediate-backed implementation

**Files:**
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`

- [ ] **Step 1: 声明 OpenGL command buffer 与 queue**

在 `OpenGLRenderDevice.h` 中 `OpenGLCommandList` 后加入：

```cpp
class OpenGLCommandBuffer final : public CommandBuffer {
public:
	explicit OpenGLCommandBuffer(const CommandBufferDesc& desc);

	const CommandBufferDesc& GetDesc() const override;

private:
	CommandBufferDesc m_Desc;
};

class OpenGLRenderQueue final : public RenderQueue {
public:
	void Submit(CommandBuffer& commandBuffer) override;
};
```

在 `OpenGLRenderDevice` public 接口加入：

```cpp
Ref<CommandBuffer> CreateCommandBuffer(const CommandBufferDesc& desc) override;
RenderQueue& GetGraphicsQueue() override;
```

在 private 成员加入：

```cpp
OpenGLRenderQueue m_GraphicsQueue;
```

- [ ] **Step 2: 实现 OpenGL command submission**

在 `OpenGLRenderDevice.cpp` 的 `namespace HE::Rendering` 内加入：

```cpp
OpenGLCommandBuffer::OpenGLCommandBuffer(const CommandBufferDesc& desc)
	: m_Desc(desc) {
	HE_CORE_ASSERT(m_Desc.Usage == CommandBufferUsage::Graphics, "OpenGL command buffer requires graphics usage");
}

const CommandBufferDesc& OpenGLCommandBuffer::GetDesc() const {
	return m_Desc;
}

void OpenGLRenderQueue::Submit(CommandBuffer& commandBuffer) {
	if (commandBuffer.GetDesc().Usage != CommandBufferUsage::Graphics) {
		HE_CORE_WARN("OpenGL graphics queue skipped non-graphics command buffer");
		return;
	}
}
```

在 `OpenGLRenderDevice` 方法区加入：

```cpp
Ref<CommandBuffer> OpenGLRenderDevice::CreateCommandBuffer(const CommandBufferDesc& desc) {
	if (desc.Usage != CommandBufferUsage::Graphics) {
		HE_CORE_ERROR("Invalid command buffer description");
		return nullptr;
	}

	return CreateRef<OpenGLCommandBuffer>(desc);
}

RenderQueue& OpenGLRenderDevice::GetGraphicsQueue() {
	return m_GraphicsQueue;
}
```

## Task 4: GREEN verification

**Files:**
- No additional production files.

- [ ] **Step 1: 构建目标**

Run:

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
```

Expected: build succeeds.

- [ ] **Step 2: 运行 smoke**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

Expected:

```text
RHIResourceCreationSmoke passed
```

## Task 5: regression verification and docs

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
- Modify: `.workspace/superpower/plans/2026-07-15-rendering-p30-command-buffer-queue-plan.md`

- [ ] **Step 1: 运行相关回归**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: 三个 smoke 均 passed。

- [ ] **Step 2: 更新 spec**

在 spec 末尾追加：

```markdown
### P30 实现结果

- RHI 已新增 `CommandBuffer`、`CommandBufferDesc`、`CommandBufferUsage` 与 `RenderQueue`。
- `RenderDevice` 已暴露 `CreateCommandBuffer` 与 `GetGraphicsQueue`。
- OpenGL backend 已提供 immediate-backed command buffer/queue 最小实现。
- 现有 `GetImmediateCommandList()` 与 Forward 主路径保持不变。
- `RHIResourceCreationSmoke` 已覆盖 command submission capability、graphics command buffer 创建、空提交和 invalid usage 拒绝。
```

- [ ] **Step 3: diff check**

Run:

```powershell
git diff --check
git status --short
```

Expected: diff check clean；状态只包含本轮 P30 文件。

---

## 自检

- Spec 覆盖：本计划覆盖 P30 的 command buffer、queue、OpenGL 最小实现、保留 immediate path、smoke 覆盖。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`CommandBufferUsage`、`CommandBufferDesc`、`CommandBuffer`、`RenderQueue`、`CreateCommandBuffer`、`GetGraphicsQueue` 在任务间命名一致。

## P30 执行结果

- 已新增 `HuaEngine/src/HuaEngine/Rendering/RHI/CommandSubmission.h`。
- `RenderDevice` 已增加 command submission capability、`CreateCommandBuffer` 与 `GetGraphicsQueue`。
- OpenGL backend 已新增 `OpenGLCommandBuffer` 与 `OpenGLRenderQueue`，空 graphics command buffer 提交为同步 no-op。
- `Tests/RHIResourceCreationSmoke.cpp` 已覆盖 command buffer 创建、desc round-trip、queue submit 与 invalid usage 拒绝。
- 已按 TDD 验证 RED：构建先因缺少 command submission API 失败。
- 已验证 GREEN 与回归：

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

结果：

```text
RHICommandListBindingSmoke passed
RHIResourceCreationSmoke passed
RenderingOperationsSmoke passed
```
