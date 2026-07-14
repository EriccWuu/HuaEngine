# P28 RenderPassDesc Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增现代 `RenderPassDesc / Attachment` 抽象，让 Forward 主路径使用 `BeginRenderPass/EndRenderPass`，同时保留 `BeginRenderTarget/EndRenderTarget` 兼容 helper。

**Architecture:** 在 RHI 层新增 `RenderPass.h` 定义 load/store op 与 attachment desc；`CommandList` 增加 render pass 入口。OpenGL backend 将 `BeginRenderPass` 映射为绑定当前 `RenderTarget` 并按 attachment load op 执行 clear；旧 `BeginRenderTarget` 内部转为单 attachment、`LoadOp::Load` 的兼容 render pass。

**Tech Stack:** C++17、HuaEngine Rendering/RHI、OpenGL backend、CMake smoke targets。

---

## 文件结构

- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/RenderPass.h`
  - 定义 `LoadOp`、`StoreOp`、`RenderPassColorAttachment`、`RenderPassDepthStencilAttachment`、`RenderPassDesc`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
  - include `RenderPass.h`，新增 `BeginRenderPass/EndRenderPass`。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
  - `OpenGLCommandList` 声明 `BeginRenderPass/EndRenderPass` override。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - 实现新 render pass 接口；旧 render target helper 转调新接口。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`
  - `BindTargetPass`/`ClearTargetPass`/`UnbindTargetPass` 改用 render pass 语义。
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
  - 主路径改用 `BeginRenderPass/EndRenderPass`，保留至少一个旧 helper 兼容覆盖。

---

### Task 1: RHI RenderPass 类型与 CommandList 接口

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/RenderPass.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`

- [ ] **Step 1: 新增 RenderPass.h**

创建 `RenderPass.h`：

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "glm/glm.hpp"

#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class RenderTarget;

	enum class LoadOp : uint8_t {
		Load = 0,
		Clear,
		DontCare
	};

	enum class StoreOp : uint8_t {
		Store = 0,
		DontCare
	};

	struct RenderPassColorAttachment {
		Ref<RenderTarget> Target;
		uint32_t AttachmentIndex = 0;
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		glm::vec4 ClearColor = glm::vec4(0.0f);
	};

	struct RenderPassDepthStencilAttachment {
		Ref<RenderTarget> Target;
		LoadOp DepthLoad = LoadOp::Clear;
		StoreOp DepthStore = StoreOp::Store;
		float ClearDepth = 1.0f;
		uint32_t ClearStencil = 0;
	};

	struct RenderPassDesc {
		std::vector<RenderPassColorAttachment> ColorAttachments;
		std::optional<RenderPassDepthStencilAttachment> DepthStencilAttachment;
	};
}
```

- [ ] **Step 2: 扩展 CommandList.h**

在 `CommandList.h` 中 include：

```cpp
#include "HuaEngine/Rendering/RHI/RenderPass.h"
```

在 `CommandList` 中新增：

```cpp
virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
virtual void EndRenderPass() = 0;
```

保留 `BeginRenderTarget/EndRenderTarget`。

- [ ] **Step 3: 扩展 OpenGLCommandList 声明**

在 `OpenGLRenderDevice.h` 的 `OpenGLCommandList` 中新增 override：

```cpp
void BeginRenderPass(const RenderPassDesc& desc) override;
void EndRenderPass() override;
```

- [ ] **Step 4: 编译验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
```

Expected:
- 此时如果 OpenGL implementation 尚未补齐会编译失败，失败应指向缺少 pure virtual override 或 unresolved implementation。

---

### Task 2: OpenGL RenderPass 实现与兼容 helper

**Files:**
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`

- [ ] **Step 1: include RenderPass**

确保 `OpenGLRenderDevice.cpp` 可见 `RenderPassDesc`。

- [ ] **Step 2: 实现 BeginRenderPass**

实现逻辑：

```cpp
void OpenGLCommandList::BeginRenderPass(const RenderPassDesc& desc) {
	if (desc.ColorAttachments.empty() || !desc.ColorAttachments[0].Target) {
		HE_CORE_WARN("CommandList::BeginRenderPass skipped because no color attachment target was provided");
		return;
	}

	if (desc.ColorAttachments.size() > 1) {
		HE_CORE_WARN("CommandList::BeginRenderPass currently uses only the first color attachment");
	}

	m_CurrentRenderTarget = desc.ColorAttachments[0].Target.get();
	static_cast<OpenGLRenderTarget&>(*m_CurrentRenderTarget).BeginForCommandList();

	GLbitfield clearMask = 0;
	const auto& colorAttachment = desc.ColorAttachments[0];
	if (colorAttachment.Load == LoadOp::Clear) {
		const auto& clearColor = colorAttachment.ClearColor;
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
		clearMask |= GL_COLOR_BUFFER_BIT;
	}

	if (desc.DepthStencilAttachment) {
		const auto& depthStencil = *desc.DepthStencilAttachment;
		if (depthStencil.DepthLoad == LoadOp::Clear) {
			glClearDepth(depthStencil.ClearDepth);
			clearMask |= GL_DEPTH_BUFFER_BIT;
		}
	}

	if (clearMask != 0) {
		glClear(clearMask);
	}
}
```

如当前 OpenGL include 未提供 `GLbitfield`，使用已有 OpenGL include 即可。

- [ ] **Step 3: 实现 EndRenderPass**

实现逻辑：

```cpp
void OpenGLCommandList::EndRenderPass() {
	if (m_CurrentRenderTarget) {
		static_cast<OpenGLRenderTarget*>(m_CurrentRenderTarget)->EndForCommandList();
	}

	m_CurrentRenderTarget = nullptr;
}
```

- [ ] **Step 4: 兼容 BeginRenderTarget/EndRenderTarget**

将 `BeginRenderTarget(RenderTarget& target)` 改为：

```cpp
void OpenGLCommandList::BeginRenderTarget(RenderTarget& target) {
	BeginRenderPass({
		.ColorAttachments = {
			{
				.Target = target.shared_from_this() // 如果 RenderTarget 不支持 shared_from_this，此写法不可用
			}
		}
	});
}
```

注意：当前 `RenderTarget&` 无法直接构造 `Ref<RenderTarget>`。因此实际实现不要强造 shared ptr，应新增 private helper 或直接保留旧绑定逻辑。推荐实际实现：

```cpp
void OpenGLCommandList::BeginRenderTarget(RenderTarget& target) {
	m_CurrentRenderTarget = &target;
	static_cast<OpenGLRenderTarget&>(target).BeginForCommandList();
}
```

`EndRenderTarget()` 改为调用：

```cpp
EndRenderPass();
```

这样旧 helper 保持行为不变，不引入错误所有权。

- [ ] **Step 5: 构建验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
```

Expected:
- 编译通过。

---

### Task 3: ForwardRenderPipeline 迁移到 RenderPass

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`

- [ ] **Step 1: include RenderPass**

新增：

```cpp
#include "HuaEngine/Rendering/RHI/RenderPass.h"
```

- [ ] **Step 2: BindTargetPass 改为 BeginRenderPass**

将：

```cpp
context.Commands->BeginRenderTarget(*context.View->Target);
```

改为：

```cpp
context.Commands->BeginRenderPass({
	.ColorAttachments = {
		{
			.Target = context.View->Target,
			.AttachmentIndex = 0,
			.Load = context.View->ClearColorBuffer ? LoadOp::Clear : LoadOp::Load,
			.Store = StoreOp::Store,
			.ClearColor = context.View->ClearColor
		}
	}
});
```

- [ ] **Step 3: ClearTargetPass 停止 ClearColor**

`ClearTargetPass::Execute` 仍递增 `PassCount`，但不再调用 `ClearColor`。可以保留空 pass 以减少 graph 结构改动：

```cpp
void ClearTargetPass::Execute(RenderPassContext& context) {
	if (!context.View || !context.Commands || !context.Stats) {
		return;
	}

	++context.Stats->PassCount;
}
```

- [ ] **Step 4: UnbindTargetPass 改为 EndRenderPass**

将：

```cpp
context.Commands->EndRenderTarget();
```

改为：

```cpp
context.Commands->EndRenderPass();
```

- [ ] **Step 5: 搜索验证**

Run:

```powershell
rg -n -S "BeginRenderTarget\\(|EndRenderTarget\\(|ClearColor\\(" HuaEngine/src/HuaEngine/Rendering/RenderPipeline
```

Expected:
- Forward/RenderPipeline 主路径不再有这些调用。

- [ ] **Step 6: 构建与 smoke**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke ApplicationOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 两个 smoke passed。

---

### Task 4: RHI smoke 覆盖 RenderPass path 与兼容 helper

**Files:**
- Modify: `Tests/RHICommandListBindingSmoke.cpp`

- [ ] **Step 1: include RenderPass**

新增：

```cpp
#include "HuaEngine/Rendering/RHI/RenderPass.h"
```

- [ ] **Step 2: 把前两个正常绘制场景迁移到 BeginRenderPass**

将正常绘制场景的：

```cpp
commands.BeginRenderTarget(*renderTarget);
commands.ClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
...
commands.EndRenderTarget();
```

改为：

```cpp
commands.BeginRenderPass({
	.ColorAttachments = {
		{
			.Target = renderTarget,
			.AttachmentIndex = 0,
			.Load = HE::Rendering::LoadOp::Clear,
			.Store = HE::Rendering::StoreOp::Store,
			.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
		}
	}
});
...
commands.EndRenderPass();
```

- [ ] **Step 3: 保留一个兼容 helper 覆盖**

保留一个场景继续使用：

```cpp
commands.BeginRenderTarget(*renderTarget);
commands.ClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
...
commands.EndRenderTarget();
```

这样确认旧 helper 没坏。

- [ ] **Step 4: 验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
```

Expected:
- `RHICommandListBindingSmoke passed`

---

### Task 5: 最终审计、更新 spec、提交

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`

- [ ] **Step 1: 最终构建**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RenderingOperationsSmoke ApplicationOperationsSmoke Editor
```

Expected:
- 构建通过。

- [ ] **Step 2: 最终 smoke**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 全部 passed。

- [ ] **Step 3: 残留搜索**

Run:

```powershell
rg -n -S "BeginRenderTarget\\(|EndRenderTarget\\(|ClearColor\\(" HuaEngine/src/HuaEngine/Rendering/RenderPipeline Tests/RHICommandListBindingSmoke.cpp
```

Expected:
- `RenderPipeline` 无旧 pass 调用。
- `Tests/RHICommandListBindingSmoke.cpp` 允许保留一个 compatibility helper 场景。

- [ ] **Step 4: 更新 spec 状态**

在 `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md` 中增加 P28 实现记录：

```markdown
### P28 实现结果

- 已新增 `RenderPass.h`，定义 load/store op 与 render pass attachment desc。
- `CommandList` 已新增 `BeginRenderPass/EndRenderPass`。
- OpenGL backend 已实现 render pass clear/draw 路径。
- Forward 主路径已迁移到 `BeginRenderPass/EndRenderPass`。
- `BeginRenderTarget/EndRenderTarget` 暂时保留为 compatibility helper。
```

- [ ] **Step 5: diff check 与提交**

Run:

```powershell
git diff --check
git add HuaEngine/src Tests
git add -f .workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md .workspace/superpower/plans/2026-07-14-rendering-p28-render-pass-desc-plan.md
git commit -m "refactor(rendering): introduce render pass desc"
```

Expected:
- commit 成功。

---

## 自检

- Spec 覆盖：本 plan 只执行 P28，未触碰 P29-P32。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`RenderPassDesc`、`LoadOp`、`StoreOp` 与 spec 一致；旧 helper 保留。
