# P29 BindGroupLayout Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `PipelineState` 声明期望的 bind group layouts，并让 `CommandList::SetBindGroup(slot, group)` 根据当前 pipeline contract 校验 slot/scope/entry。

**Architecture:** 在 `PipelineStateDesc` 中新增 slot 到 `BindGroupLayout` 的 contract 描述；OpenGL backend 在创建 pipeline state 时校验 contract，在 `SetBindGroup` 时比较当前 pipeline contract 与提交的 bind group layout。Forward resolver 为每个 pipeline 填入 frame/material/object layout contract；RHI smoke 覆盖正确 contract 和错误 scope/slot 被拒绝。

**Tech Stack:** C++17、HuaEngine Rendering/RHI、OpenGL backend、CMake smoke targets。

---

## 文件结构

- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/PipelineState.h`
  - 新增 `PipelineBindGroupLayoutRef`，扩展 `PipelineStateDesc::BindGroupLayouts`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h/.cpp`
  - 新增标准 frame/object layout 构建 helper，便于 resolver 填 pipeline contract。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
  - 创建 pipeline state 时传入 frame/material/object layout contract。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - 校验 pipeline bind group layout contract，`SetBindGroup` 根据当前 pipeline contract 拒绝不兼容 group。
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
  - pipeline state 创建时传入 slot 0/1/2 layouts；新增错误 scope/slot 覆盖。
- Modify: `Tests/RHIResourceCreationSmoke.cpp`
  - 覆盖 pipeline layout contract round-trip 与 invalid duplicate/null layout。

---

### Task 1: PipelineStateDesc 增加 bind group layout contract

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/PipelineState.h`
- Modify: `Tests/RHIResourceCreationSmoke.cpp`

- [ ] **Step 1: 扩展 PipelineState.h**

在 `PipelineState.h` 中 include：

```cpp
#include <vector>
#include "HuaEngine/Rendering/RHI/BindGroup.h"
```

新增：

```cpp
struct PipelineBindGroupLayoutRef {
	uint32_t Slot = 0;
	Ref<BindGroupLayout> Layout;
};
```

扩展：

```cpp
struct PipelineStateDesc {
	Ref<ShaderProgram> Shader;
	BufferLayout VertexLayout;
	PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
	std::vector<PipelineBindGroupLayoutRef> BindGroupLayouts;
};
```

- [ ] **Step 2: 更新 resource creation smoke round-trip**

在 `Tests/RHIResourceCreationSmoke.cpp` 中，现有 `bindGroupLayout` 创建之后，再创建一个带 contract 的 pipeline state：

```cpp
auto contractedPipelineState = device.CreatePipelineState({
	.Shader = shaderProgram,
	.VertexLayout = layout,
	.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
	.BindGroupLayouts = {
		{
			.Slot = 1,
			.Layout = bindGroupLayout
		}
	}
});
Require(static_cast<bool>(contractedPipelineState), "Expected contracted pipeline state creation to succeed");
Require(contractedPipelineState->GetDesc().BindGroupLayouts.size() == 1, "Expected pipeline bind group layout contract");
Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Slot == 1, "Expected material bind group slot contract");
Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Layout == bindGroupLayout, "Expected material bind group layout contract");
```

- [ ] **Step 3: 构建验证**

Run:

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
```

Expected:
- 编译通过。
- 此时 invalid duplicate/null layout 尚未实现，不需要新增失败断言。

---

### Task 2: OpenGL pipeline contract validation 与 SetBindGroup 校验

**Files:**
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
- Modify: `Tests/RHIResourceCreationSmoke.cpp`

- [ ] **Step 1: 增加 layout 比较 helper**

在 `OpenGLRenderDevice.cpp` anonymous namespace 中新增 helper：

```cpp
bool BindGroupLayoutEntriesMatch(const BindGroupLayoutDesc& expected, const BindGroupLayoutDesc& actual) {
	if (expected.Scope != actual.Scope || expected.Entries.size() != actual.Entries.size()) {
		return false;
	}

	for (size_t i = 0; i < expected.Entries.size(); ++i) {
		const auto& expectedEntry = expected.Entries[i];
		const auto& actualEntry = actual.Entries[i];
		if (expectedEntry.Name != actualEntry.Name ||
			expectedEntry.Type != actualEntry.Type ||
			expectedEntry.Binding != actualEntry.Binding) {
			return false;
		}
	}

	return true;
}
```

新增 pipeline desc 校验：

```cpp
bool ValidatePipelineBindGroupLayouts(const PipelineStateDesc& desc) {
	std::vector<uint32_t> slots;
	slots.reserve(desc.BindGroupLayouts.size());

	for (const auto& layoutRef : desc.BindGroupLayouts) {
		if (!layoutRef.Layout) {
			HE_CORE_ERROR("Pipeline bind group layout contract contains a null layout");
			return false;
		}

		if (std::find(slots.begin(), slots.end(), layoutRef.Slot) != slots.end()) {
			HE_CORE_ERROR("Pipeline bind group layout contract contains duplicate slot {0}", layoutRef.Slot);
			return false;
		}

		slots.push_back(layoutRef.Slot);
	}

	return true;
}
```

如文件未 include `<algorithm>`，新增 include。

- [ ] **Step 2: OpenGLPipelineState 构造和 CreatePipelineState 校验**

在 `OpenGLPipelineState::OpenGLPipelineState` 中新增 assert：

```cpp
HE_CORE_ASSERT(ValidatePipelineBindGroupLayouts(m_Desc), "PipelineState bind group layout contract is invalid");
```

在 `OpenGLRenderDevice::CreatePipelineState` 中将 invalid 条件扩展：

```cpp
if (!desc.Shader || desc.VertexLayout.GetElements().empty() || !ValidatePipelineBindGroupLayouts(desc)) {
	HE_CORE_ERROR("Invalid pipeline state description");
	return nullptr;
}
```

- [ ] **Step 3: SetBindGroup 校验当前 pipeline contract**

在 `OpenGLCommandList::SetBindGroup` 中，移除 `(void)slot;`，在 `auto& shaderProgram...` 前后均可，增加：

```cpp
if (!m_CurrentPipelineState) {
	HE_CORE_WARN("CommandList::SetBindGroup skipped because no pipeline state is bound");
	return;
}

const auto& pipelineDesc = m_CurrentPipelineState->GetDesc();
const auto layoutIt = std::find_if(
	pipelineDesc.BindGroupLayouts.begin(),
	pipelineDesc.BindGroupLayouts.end(),
	[slot](const PipelineBindGroupLayoutRef& layoutRef) {
		return layoutRef.Slot == slot;
	});
if (layoutIt == pipelineDesc.BindGroupLayouts.end()) {
	HE_CORE_WARN("CommandList::SetBindGroup skipped because slot {0} is not declared by the current pipeline", slot);
	return;
}

if (!bindGroup.GetDesc().Layout || !layoutIt->Layout) {
	HE_CORE_WARN("CommandList::SetBindGroup skipped because the bind group layout is missing");
	return;
}

if (!BindGroupLayoutEntriesMatch(layoutIt->Layout->GetDesc(), bindGroup.GetDesc().Layout->GetDesc())) {
	HE_CORE_WARN("CommandList::SetBindGroup skipped because slot {0} layout is incompatible with the current pipeline", slot);
	return;
}
```

然后保留现有 uniform/texture submission。

- [ ] **Step 4: Resource creation smoke invalid coverage**

在 `Tests/RHIResourceCreationSmoke.cpp` 中新增：

```cpp
Require(!device.CreatePipelineState({
	.Shader = shaderProgram,
	.VertexLayout = layout,
	.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
	.BindGroupLayouts = {
		{ .Slot = 1, .Layout = bindGroupLayout },
		{ .Slot = 1, .Layout = bindGroupLayout }
	}
}), "Expected duplicate pipeline bind group layout slots to fail");

Require(!device.CreatePipelineState({
	.Shader = shaderProgram,
	.VertexLayout = layout,
	.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
	.BindGroupLayouts = {
		{ .Slot = 1, .Layout = nullptr }
	}
}), "Expected null pipeline bind group layout to fail");
```

- [ ] **Step 5: 验证**

Run:

```powershell
cmake --build build --config Debug --target RHIResourceCreationSmoke
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

Expected:
- `RHIResourceCreationSmoke passed`

---

### Task 3: RenderBindGroupBuilder 暴露标准 frame/object layout helper

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.cpp`

- [ ] **Step 1: 扩展头文件**

在 `RenderBindGroupBuilder.h` 中 forward declare：

```cpp
class BindGroupLayout;
```

新增声明：

```cpp
Ref<BindGroupLayout> CreateFrameBindGroupLayout(RenderDevice& device);
Ref<BindGroupLayout> CreateObjectBindGroupLayout(RenderDevice& device);
```

- [ ] **Step 2: 实现标准 layout helper**

在 cpp 中新增 helper：

```cpp
Ref<BindGroupLayout> CreateSingleMat4BindGroupLayout(
	RenderDevice& device,
	BindGroupScope scope,
	const char* name) {
	return device.CreateBindGroupLayout({
		.Scope = scope,
		.Entries = {
			{
				.Name = name,
				.Type = BindingValueType::Mat4,
				.Binding = 0
			}
		}
	});
}
```

将 `CreateSingleMat4BindGroup` 改为复用 layout helper。

新增导出函数：

```cpp
Ref<BindGroupLayout> CreateFrameBindGroupLayout(RenderDevice& device) {
	return CreateSingleMat4BindGroupLayout(device, BindGroupScope::Frame, "u_ViewProjection");
}

Ref<BindGroupLayout> CreateObjectBindGroupLayout(RenderDevice& device) {
	return CreateSingleMat4BindGroupLayout(device, BindGroupScope::Object, "u_Transform");
}
```

- [ ] **Step 3: 验证**

Run:

```powershell
cmake --build build --config Debug --target HuaEngine
```

Expected:
- 构建通过。

---

### Task 4: Forward resolver 创建 pipeline contract

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`

- [ ] **Step 1: 填入 pipeline bind group layouts**

在 `RenderResourceResolver::Resolve` 中，`MaterialBindGroupRef` 创建后，pipeline 创建前，新增：

```cpp
auto& device = RenderHardwareInterface::GetDevice();
auto frameBindGroupLayout = CreateFrameBindGroupLayout(device);
auto objectBindGroupLayout = CreateObjectBindGroupLayout(device);
if (!frameBindGroupLayout || !objectBindGroupLayout || !outResolvedItem.MaterialBindGroupRef || !outResolvedItem.MaterialBindGroupRef->GetDesc().Layout) {
	AddDiagnostic(
		diagnostics,
		RenderDiagnosticCode::MissingPipelineState,
		item.SourceEntity,
		"Render item pipeline bind group layout contract could not be created");
	return false;
}
```

将 pipeline 创建改为：

```cpp
outResolvedItem.PipelineStateRef = device.CreatePipelineState({
	.Shader = outResolvedItem.ShaderProgramRef,
	.VertexLayout = outResolvedItem.VertexBufferViewRef->GetDesc().Layout,
	.Topology = PrimitiveTopology::TriangleList,
	.BindGroupLayouts = {
		{ .Slot = 0, .Layout = frameBindGroupLayout },
		{ .Slot = 1, .Layout = outResolvedItem.MaterialBindGroupRef->GetDesc().Layout },
		{ .Slot = 2, .Layout = objectBindGroupLayout }
	}
});
```

- [ ] **Step 2: 验证 Forward smoke**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke ApplicationOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 两个 smoke passed。

---

### Task 5: RHI smoke contract path 和 wrong layout coverage

**Files:**
- Modify: `Tests/RHICommandListBindingSmoke.cpp`

- [ ] **Step 1: pipeline 创建传入 layout contract**

将现有 `pipelineState` 创建移动到 frame/material/object layout 创建之后，或在创建 pipeline state 前先创建三种 layout。

最终 pipeline state 应为：

```cpp
auto pipelineState = device.CreatePipelineState({
	.Shader = shaderProgram,
	.VertexLayout = layout,
	.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
	.BindGroupLayouts = {
		{ .Slot = 0, .Layout = frameBindGroupLayout },
		{ .Slot = 1, .Layout = materialBindGroupLayout },
		{ .Slot = 2, .Layout = objectBindGroupLayout }
	}
});
```

- [ ] **Step 2: 新增 wrong scope bind group**

在 object bind group 后新增：

```cpp
auto wrongObjectBindGroupLayout = device.CreateBindGroupLayout({
	.Scope = HE::Rendering::BindGroupScope::Material,
	.Entries = {
		{
			.Name = "u_Transform",
			.Type = HE::Rendering::BindingValueType::Mat4,
			.Binding = 0
		}
	}
});
Require(static_cast<bool>(wrongObjectBindGroupLayout), "Expected wrong object bind group layout creation to succeed");
auto wrongObjectBindGroup = device.CreateBindGroup({
	.Layout = wrongObjectBindGroupLayout,
	.Entries = {
		{
			.Name = "u_Transform",
			.Type = HE::Rendering::BindingValueType::Mat4,
			.Value = glm::mat4(1.0f),
			.Binding = 0
		}
	}
});
Require(static_cast<bool>(wrongObjectBindGroup), "Expected wrong object bind group creation to succeed");
```

- [ ] **Step 3: 新增 rejected wrong scope draw scenario**

新增一段 render pass：

```cpp
commands.BeginRenderPass({... clear ...});
commands.BeginFrame(camera);
commands.SetPipelineState(*pipelineState);
commands.SetBindGroup(0, *frameBindGroup);
commands.SetVertexBufferView(*vertexBufferView);
commands.SetBindGroup(1, *materialBindGroup);
commands.SetBindGroup(2, *wrongObjectBindGroup);
commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
commands.EndFrame();
commands.EndRenderPass();
VerifyRenderTargetCleared(renderTarget);
```

此场景验证 slot 2 scope mismatch 会被拒绝，object bind group 不会激活，draw 被跳过。

- [ ] **Step 4: 验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
```

Expected:
- `RHICommandListBindingSmoke passed`

---

### Task 6: 最终审计、更新 spec、提交

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`

- [ ] **Step 1: 最终构建和 smoke**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderingOperationsSmoke ApplicationOperationsSmoke Editor
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 构建和 smoke 全部通过。

- [ ] **Step 2: 残留/行为搜索**

Run:

```powershell
rg -n -S "BindGroupLayouts|PipelineBindGroupLayoutRef|SetBindGroup\\(" HuaEngine/src/HuaEngine/Rendering HuaEngine/src/Platform/OpenGL/RHI Tests
```

Expected:
- `PipelineStateDesc` 有 contract。
- Forward pipeline 创建传入 slot 0/1/2 layouts。
- OpenGL `SetBindGroup` 有 contract validation。

- [ ] **Step 3: 更新 spec**

在 spec 末尾追加：

```markdown
### P29 实现结果

- `PipelineStateDesc` 已新增 bind group layout contract。
- OpenGL pipeline 创建会校验 null layout 与重复 slot。
- OpenGL `SetBindGroup` 会按当前 pipeline contract 校验 slot/scope/name/type/binding。
- Forward pipeline 创建已声明 frame/material/object slot contract。
- RHI smoke 已覆盖正确 contract 和错误 scope 被拒绝。
```

- [ ] **Step 4: diff check 与提交**

Run:

```powershell
git diff --check
git add HuaEngine/src Tests
git add -f .workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md .workspace/superpower/plans/2026-07-15-rendering-p29-bind-group-contract-plan.md
git commit -m "refactor(rendering): validate bind group layout contracts"
```

Expected:
- commit 成功。

---

## 自检

- Spec 覆盖：本 plan 只执行 P29，未触碰 P30-P32。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`PipelineBindGroupLayoutRef`、`BindGroupLayouts`、slot 0/1/2 contract 与 spec 一致。
