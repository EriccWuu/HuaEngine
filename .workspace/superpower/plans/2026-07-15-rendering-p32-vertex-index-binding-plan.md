# P32 Vertex / Index Buffer Binding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RHI 增加显式 `VertexBufferBinding` / `IndexBufferBinding`，降低 `VertexBufferView` 作为唯一提交入口的过渡性。

**Architecture:** 新增 `VertexInputBinding.h` 定义 vertex/index buffer binding；`CommandList` 增加 `SetVertexBuffer` 与 `SetIndexBuffer`。OpenGL backend 在新 binding path 下根据当前 pipeline vertex layout、vertex buffer 和 index buffer 创建内部 VAO，旧 `SetVertexBufferView` 继续保留并作为 compatibility path，不迁移 Forward 主路径。

**Tech Stack:** C++20、CMake、OpenGL backend、现有 smoke 测试框架。

---

## 文件结构

- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/VertexInputBinding.h`
  - 定义 `VertexBufferBinding`、`IndexBufferBinding`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
  - include `VertexInputBinding.h`。
  - 新增 `SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding)`。
  - 新增 `SetIndexBuffer(const IndexBufferBinding& binding)`。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
  - `OpenGLCommandList` 新增两个接口实现。
  - 增加内部 explicit binding 状态和 VAO handle。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - 实现 explicit binding path。
  - `SetVertexBufferView` 保持旧行为，同时清理 explicit binding 状态。
  - `DrawIndexed` 支持 old view path 和 new explicit binding path。
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
  - 新增使用 `SetVertexBuffer` / `SetIndexBuffer` 绘制成功的 smoke 分支。
- Modify: `Tests/RHIResourceCreationSmoke.cpp`
  - 覆盖 binding desc round-trip 级别的类型使用。
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
  - 追加 P32 实现结果。

## Task 1: RED - explicit vertex/index binding smoke

**Files:**
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
- Modify: `Tests/RHIResourceCreationSmoke.cpp`

- [ ] **Step 1: 写 RHIResourceCreationSmoke 类型覆盖**

在 `vertexBufferView` 创建后加入：

```cpp
HE::Rendering::VertexBufferBinding vertexBinding{
	.Buffer = vertexBuffer,
	.Offset = 0,
	.Stride = 3 * sizeof(float)
};
Require(vertexBinding.Buffer == vertexBuffer, "Expected vertex binding buffer to round-trip");
Require(vertexBinding.Stride == 3 * sizeof(float), "Expected vertex binding stride");

HE::Rendering::IndexBufferBinding indexBinding{
	.Buffer = indexBuffer,
	.Offset = 0,
	.Format = HE::Rendering::IndexFormat::UInt32,
	.IndexCount = 3
};
Require(indexBinding.Buffer == indexBuffer, "Expected index binding buffer to round-trip");
Require(indexBinding.IndexCount == 3, "Expected index binding count");
```

- [ ] **Step 2: 写 RHICommandListBindingSmoke 绘制覆盖**

在一次成功绘制场景后新增一段 render pass：

```cpp
commands.BeginRenderPass({
	.ColorAttachments = {
		{
			.Target = renderTarget,
			.Load = HE::Rendering::LoadOp::Clear,
			.Store = HE::Rendering::StoreOp::Store,
			.ClearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f)
		}
	}
});
commands.BeginFrame(camera);
commands.SetPipelineState(*pipelineState);
commands.SetVertexBuffer(0, {
	.Buffer = vertexBuffer,
	.Offset = 0,
	.Stride = 3 * sizeof(float)
});
commands.SetIndexBuffer({
	.Buffer = indexBuffer,
	.Offset = 0,
	.Format = HE::Rendering::IndexFormat::UInt32,
	.IndexCount = 3
});
commands.SetBindGroup(0, *frameBindGroup);
commands.SetBindGroup(1, *materialBindGroup);
commands.SetBindGroup(2, *objectBindGroup);
commands.DrawIndexed(3);
commands.EndFrame();
commands.EndRenderPass();
VerifyRenderTargetSamples(renderTarget);
```

- [ ] **Step 3: 运行并确认失败**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke
```

Expected: 编译失败，提示 `VertexBufferBinding`、`IndexBufferBinding`、`SetVertexBuffer` 或 `SetIndexBuffer` 未定义。

## Task 2: GREEN - public RHI binding API

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/VertexInputBinding.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`

- [ ] **Step 1: 新增 binding 类型**

`VertexInputBinding.h` 内容：

```cpp
#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	struct VertexBufferBinding {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		uint32_t Stride = 0;
	};

	struct IndexBufferBinding {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		IndexFormat Format = IndexFormat::UInt32;
		uint32_t IndexCount = 0;
	};
}
```

- [ ] **Step 2: 扩展 CommandList**

在 `CommandList.h` include 加入：

```cpp
#include "HuaEngine/Rendering/RHI/VertexInputBinding.h"
```

在 `SetVertexBufferView` 前加入：

```cpp
virtual void SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) = 0;
virtual void SetIndexBuffer(const IndexBufferBinding& binding) = 0;
```

## Task 3: GREEN - OpenGL explicit binding path

**Files:**
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`

- [ ] **Step 1: 声明 OpenGLCommandList 状态**

在 `OpenGLCommandList` public 加入：

```cpp
void SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) override;
void SetIndexBuffer(const IndexBufferBinding& binding) override;
```

在 private 加入：

```cpp
void RebuildExplicitVertexArray();
void ReleaseExplicitVertexArray();

VertexBufferBinding m_CurrentVertexBufferBinding;
IndexBufferBinding m_CurrentIndexBufferBinding;
uint32_t m_ExplicitVertexArray = 0;
bool m_HasExplicitVertexBuffer = false;
bool m_HasExplicitIndexBuffer = false;
```

- [ ] **Step 2: 实现 explicit VAO 管理**

在 `OpenGLRenderDevice.cpp` 的 `OpenGLCommandList` 方法区加入：

```cpp
void OpenGLCommandList::ReleaseExplicitVertexArray() {
	if (m_ExplicitVertexArray != 0) {
		glDeleteVertexArrays(1, &m_ExplicitVertexArray);
		m_ExplicitVertexArray = 0;
	}
}

void OpenGLCommandList::RebuildExplicitVertexArray() {
	ReleaseExplicitVertexArray();

	if (!m_CurrentPipelineState || !m_HasExplicitVertexBuffer || !m_HasExplicitIndexBuffer) {
		return;
	}

	if (!m_CurrentVertexBufferBinding.Buffer || !m_CurrentIndexBufferBinding.Buffer) {
		return;
	}

	if (m_CurrentVertexBufferBinding.Buffer->GetDesc().Usage != GpuBufferUsage::Vertex
		|| m_CurrentIndexBufferBinding.Buffer->GetDesc().Usage != GpuBufferUsage::Index) {
		HE_CORE_WARN("CommandList explicit vertex/index binding skipped because buffer usage does not match");
		return;
	}

	const auto& layout = m_CurrentPipelineState->GetDesc().VertexLayout;
	if (layout.GetElements().empty()) {
		return;
	}

	GLint previousVertexArray = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
	GLint previousArrayBuffer = 0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

	glGenVertexArrays(1, &m_ExplicitVertexArray);
	glBindVertexArray(m_ExplicitVertexArray);
	static_cast<OpenGLGpuBuffer&>(*m_CurrentVertexBufferBinding.Buffer).BindForCommandList();

	uint32_t index = 0;
	const auto stride = m_CurrentVertexBufferBinding.Stride != 0 ? m_CurrentVertexBufferBinding.Stride : layout.GetStride();
	for (const auto& element : layout) {
		const auto offset = static_cast<std::uintptr_t>(m_CurrentVertexBufferBinding.Offset + element.Offset);
		if (element.Type == ShaderDataType::Mat3 || element.Type == ShaderDataType::Mat4) {
			const uint32_t columnCount = element.Type == ShaderDataType::Mat3 ? 3 : 4;
			const uint32_t componentCount = VertexAttribComponentCount(element.Type);
			const uint32_t columnSize = static_cast<uint32_t>(sizeof(float)) * componentCount;

			for (uint32_t column = 0; column < columnCount; ++column) {
				glEnableVertexAttribArray(index);
				glVertexAttribPointer(
					index,
					componentCount,
					GL_FLOAT,
					element.Normalized ? GL_TRUE : GL_FALSE,
					stride,
					reinterpret_cast<const void*>(offset + columnSize * column));
				++index;
			}

			continue;
		}

		glEnableVertexAttribArray(index);
		if (IsIntegerVertexAttrib(element.Type)) {
			glVertexAttribIPointer(
				index,
				VertexAttribComponentCount(element.Type),
				ToOpenGLType(element.Type),
				stride,
				reinterpret_cast<const void*>(offset));
		}
		else {
			glVertexAttribPointer(
				index,
				VertexAttribComponentCount(element.Type),
				ToOpenGLType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				stride,
				reinterpret_cast<const void*>(offset));
		}
		++index;
	}

	static_cast<OpenGLGpuBuffer&>(*m_CurrentIndexBufferBinding.Buffer).BindForCommandList();
	glBindBuffer(GL_ARRAY_BUFFER, previousArrayBuffer);
	glBindVertexArray(previousVertexArray);
}
```

- [ ] **Step 3: 实现 SetVertexBuffer / SetIndexBuffer**

```cpp
void OpenGLCommandList::SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) {
	if (slot != 0) {
		HE_CORE_WARN("CommandList::SetVertexBuffer skipped because only slot 0 is supported");
		return;
	}

	if (!binding.Buffer || binding.Buffer->GetDesc().Usage != GpuBufferUsage::Vertex) {
		HE_CORE_WARN("CommandList::SetVertexBuffer skipped invalid vertex buffer binding");
		return;
	}

	m_CurrentVertexBufferView = nullptr;
	m_CurrentVertexBufferBinding = binding;
	m_HasExplicitVertexBuffer = true;
	RebuildExplicitVertexArray();
}

void OpenGLCommandList::SetIndexBuffer(const IndexBufferBinding& binding) {
	if (!binding.Buffer || binding.Buffer->GetDesc().Usage != GpuBufferUsage::Index || binding.IndexCount == 0) {
		HE_CORE_WARN("CommandList::SetIndexBuffer skipped invalid index buffer binding");
		return;
	}

	if (binding.Format != IndexFormat::UInt32) {
		HE_CORE_WARN("CommandList::SetIndexBuffer skipped unsupported index format");
		return;
	}

	m_CurrentVertexBufferView = nullptr;
	m_CurrentIndexBufferBinding = binding;
	m_HasExplicitIndexBuffer = true;
	RebuildExplicitVertexArray();
}
```

- [ ] **Step 4: 更新 SetPipelineState / SetVertexBufferView / DrawIndexed / EndFrame**

`SetPipelineState` 绑定 shader 后调用：

```cpp
RebuildExplicitVertexArray();
```

`SetVertexBufferView` 开头清理 explicit path：

```cpp
ReleaseExplicitVertexArray();
m_HasExplicitVertexBuffer = false;
m_HasExplicitIndexBuffer = false;
```

`DrawIndexed` 中的 vertex binding 检查改为支持两种路径：

```cpp
const bool hasVertexBinding = m_CurrentVertexBufferView || m_ExplicitVertexArray != 0;
if (!hasVertexBinding) {
	HE_CORE_WARN("CommandList::DrawIndexed skipped because no vertex/index binding is active");
	return;
}
```

available index count 改为：

```cpp
const uint32_t availableIndexCount = m_CurrentVertexBufferView
	? m_CurrentVertexBufferView->GetDesc().IndexCount
	: m_CurrentIndexBufferBinding.IndexCount;
```

draw 前如果使用 explicit path，绑定 VAO：

```cpp
if (m_ExplicitVertexArray != 0) {
	glBindVertexArray(m_ExplicitVertexArray);
}
```

`EndFrame` 中调用：

```cpp
ReleaseExplicitVertexArray();
m_HasExplicitVertexBuffer = false;
m_HasExplicitIndexBuffer = false;
m_CurrentVertexBufferBinding = {};
m_CurrentIndexBufferBinding = {};
```

## Task 4: GREEN verification

**Files:**
- No additional production files.

- [ ] **Step 1: 构建目标**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke
```

Expected: build succeeds.

- [ ] **Step 2: 运行 smoke**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

Expected:

```text
RHICommandListBindingSmoke passed
RHIResourceCreationSmoke passed
```

## Task 5: regression verification and docs

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
- Modify: `.workspace/superpower/plans/2026-07-15-rendering-p32-vertex-index-binding-plan.md`

- [ ] **Step 1: 运行相关回归**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderPassGraphSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: 四个 smoke 均 passed。

- [ ] **Step 2: 更新 spec**

在 spec 末尾追加：

```markdown
### P32 实现结果

- RHI 已新增 `VertexBufferBinding` 与 `IndexBufferBinding`。
- `CommandList` 已新增 `SetVertexBuffer` 与 `SetIndexBuffer`。
- OpenGL backend 已支持 explicit vertex/index binding path，并保留 `SetVertexBufferView` compatibility path。
- Forward 主路径暂未迁移，仍使用 `VertexBufferView`。
- `RHICommandListBindingSmoke` 已覆盖 explicit binding path 绘制。
```

- [ ] **Step 3: diff check**

Run:

```powershell
git diff --check
git status --short
```

Expected: diff check clean；状态包含 P30/P31 未提交文件与本轮 P32 文件。

---

## 自检

- Spec 覆盖：本计划覆盖 P32 的 vertex/index binding 类型、新 command 接口、OpenGL 支持和 smoke 覆盖。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`VertexBufferBinding`、`IndexBufferBinding`、`SetVertexBuffer`、`SetIndexBuffer` 在任务间命名一致。

## P32 执行结果

- 已新增 `HuaEngine/src/HuaEngine/Rendering/RHI/VertexInputBinding.h`。
- `CommandList` 已新增 `SetVertexBuffer` 与 `SetIndexBuffer`。
- OpenGL backend 已新增 explicit vertex/index binding path，基于当前 pipeline `VertexLayout` 构建内部 VAO。
- `SetVertexBufferView` compatibility path 保持可用；Forward 主路径暂未迁移。
- 当前 explicit path 支持 slot 0 vertex buffer 与 `UInt32` index buffer。
- 已按 TDD 验证 RED：构建先因 `SetVertexBuffer` / `SetIndexBuffer` 未定义失败。
- 已验证 GREEN 与回归：

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderPassGraphSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

结果：

```text
RHICommandListBindingSmoke passed
RHIResourceCreationSmoke passed
RenderPassGraphSmoke passed
RenderingOperationsSmoke passed
```
