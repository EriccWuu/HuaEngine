# P31 Resource State / Barrier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RHI 和 PassGraph 增加资源状态与 barrier 的最小可演进骨架。

**Architecture:** 新增 RHI `ResourceBarrier.h` 定义 `ResourceState` 与 texture barrier；`CommandList` 增加 `ResourceBarrier` 接口，OpenGL backend 接收并 no-op。PassGraph compile 阶段基于现有 `Inputs` / `Outputs` 生成 barrier plan：input 映射为 `ShaderRead`，output 映射为 `RenderTarget`。

**Tech Stack:** C++20、CMake、OpenGL backend、现有 smoke 测试框架。

---

## 文件结构

- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/ResourceBarrier.h`
  - 定义 `ResourceState` 和 `ResourceBarrier`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
  - include `ResourceBarrier.h`。
  - 增加 `ResourceBarrier(const ResourceBarrier& barrier)`。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
  - `OpenGLCommandList` 实现 barrier 接口。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - OpenGL barrier 为 no-op；非法 empty texture barrier 只 warn。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/PassGraph.h`
  - 增加 `PassGraphResourceBarrier` 与 `GetBarrierPlan()`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/PassGraph.cpp`
  - compile 阶段生成最小 barrier plan。
- Modify: `Tests/RenderPassGraphSmoke.cpp`
  - 覆盖 barrier plan。
- Modify: `Tests/RHIResourceCreationSmoke.cpp`
  - 覆盖 `CommandList::ResourceBarrier` 接口。
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
  - 追加 P31 实现结果。

## Task 1: RED - resource barrier smoke

**Files:**
- Modify: `Tests/RenderPassGraphSmoke.cpp`
- Modify: `Tests/RHIResourceCreationSmoke.cpp`

- [ ] **Step 1: 写 PassGraph barrier plan 失败测试**

在 typed graph compile 后加入：

```cpp
const auto& barrierPlan = typedGraph.GetBarrierPlan();
Require(barrierPlan.size() == 2, "Expected typed graph barrier plan for input and output resources");
Require(barrierPlan[0].PassName == "ForwardOpaqueTyped", "Expected first barrier pass name");
Require(barrierPlan[0].ResourceName == "RenderTarget", "Expected imported input resource barrier");
Require(barrierPlan[0].Before == HE::Rendering::ResourceState::Undefined, "Expected imported input barrier before state");
Require(barrierPlan[0].After == HE::Rendering::ResourceState::ShaderRead, "Expected imported input shader-read state");
Require(barrierPlan[1].ResourceName == "SceneColor", "Expected transient output resource barrier");
Require(barrierPlan[1].Before == HE::Rendering::ResourceState::Undefined, "Expected transient output barrier before state");
Require(barrierPlan[1].After == HE::Rendering::ResourceState::RenderTarget, "Expected transient output render-target state");
```

- [ ] **Step 2: 写 RHI barrier no-op 失败测试**

在 `RHIResourceCreationSmoke.cpp` 创建 `texture` 后加入：

```cpp
device.GetImmediateCommandList().ResourceBarrier({
	.Texture = texture,
	.Before = HE::Rendering::ResourceState::Undefined,
	.After = HE::Rendering::ResourceState::ShaderRead
});
```

- [ ] **Step 3: 运行并确认失败**

Run:

```powershell
cmake --build build --config Debug --target RenderPassGraphSmoke RHIResourceCreationSmoke
```

Expected: 编译失败，提示 `GetBarrierPlan`、`ResourceState` 或 `ResourceBarrier` 未定义。

## Task 2: GREEN - RHI resource barrier API

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RHI/ResourceBarrier.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`

- [ ] **Step 1: 新增 ResourceBarrier 类型**

`ResourceBarrier.h` 内容：

```cpp
#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class TextureResource;

	enum class ResourceState : uint32_t {
		Undefined = 0,
		RenderTarget,
		DepthStencilWrite,
		ShaderRead,
		CopySrc,
		CopyDst,
		VertexBuffer,
		IndexBuffer,
		Present
	};

	struct ResourceBarrier {
		Ref<TextureResource> Texture;
		ResourceState Before = ResourceState::Undefined;
		ResourceState After = ResourceState::Undefined;
	};
}
```

- [ ] **Step 2: 扩展 CommandList**

在 `CommandList.h` include 加入：

```cpp
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
```

在 `CommandList` public 接口中加入：

```cpp
virtual void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) = 0;
```

- [ ] **Step 3: OpenGL 实现 no-op barrier**

在 `OpenGLCommandList` 声明加入：

```cpp
void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) override;
```

在 `OpenGLRenderDevice.cpp` 中实现：

```cpp
void OpenGLCommandList::ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) {
	if (!barrier.Texture) {
		HE_CORE_WARN("OpenGLCommandList::ResourceBarrier skipped null texture barrier");
		return;
	}
}
```

## Task 3: GREEN - PassGraph barrier plan

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/PassGraph.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/PassGraph.cpp`

- [ ] **Step 1: 声明 barrier plan 类型**

在 `PassGraph.h` include 中加入：

```cpp
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
```

新增：

```cpp
struct PassGraphResourceBarrier {
	std::string PassName;
	std::string ResourceName;
	uint32_t PassIndex = 0;
	ResourceState Before = ResourceState::Undefined;
	ResourceState After = ResourceState::Undefined;
};
```

在 `PassGraph` public 接口加入：

```cpp
[[nodiscard]] const std::vector<PassGraphResourceBarrier>& GetBarrierPlan() const { return m_BarrierPlan; }
```

在 private 成员加入：

```cpp
std::vector<PassGraphResourceBarrier> m_BarrierPlan;
```

- [ ] **Step 2: compile 时生成 barrier plan**

在 `Compile()` 开头清空：

```cpp
m_BarrierPlan.clear();
```

在本地变量区增加：

```cpp
std::unordered_map<std::string, ResourceState> resourceStates;
```

在处理 input 时，`lastUsePass[input] = passIndex;` 后加入：

```cpp
const auto before = resourceStates.contains(input) ? resourceStates[input] : ResourceState::Undefined;
if (before != ResourceState::ShaderRead) {
	m_BarrierPlan.push_back({
		.PassName = pass.Name,
		.ResourceName = input,
		.PassIndex = passIndex,
		.Before = before,
		.After = ResourceState::ShaderRead
	});
	resourceStates[input] = ResourceState::ShaderRead;
}
```

在处理 output 时，`lastUsePass[output] = passIndex;` 后加入：

```cpp
const auto before = resourceStates.contains(output) ? resourceStates[output] : ResourceState::Undefined;
if (before != ResourceState::RenderTarget) {
	m_BarrierPlan.push_back({
		.PassName = pass.Name,
		.ResourceName = output,
		.PassIndex = passIndex,
		.Before = before,
		.After = ResourceState::RenderTarget
	});
	resourceStates[output] = ResourceState::RenderTarget;
}
```

在 `Reset()` 中清空：

```cpp
m_BarrierPlan.clear();
```

如果 compile 失败，barrier plan 可以保留诊断过程中的候选数据；本轮 smoke 只断言成功 graph。

## Task 4: GREEN verification

**Files:**
- No additional production files.

- [ ] **Step 1: 构建目标**

Run:

```powershell
cmake --build build --config Debug --target RenderPassGraphSmoke RHIResourceCreationSmoke
```

Expected: build succeeds.

- [ ] **Step 2: 运行 smoke**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

Expected:

```text
RenderPassGraphSmoke passed
RHIResourceCreationSmoke passed
```

## Task 5: regression verification and docs

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-modern-rhi-structural-abstractions-spec.md`
- Modify: `.workspace/superpower/plans/2026-07-15-rendering-p31-resource-state-barrier-plan.md`

- [ ] **Step 1: 运行相关回归**

Run:

```powershell
cmake --build build --config Debug --target RenderPassGraphSmoke RHIResourceCreationSmoke RHICommandListBindingSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: 四个 smoke 均 passed。

- [ ] **Step 2: 更新 spec**

在 spec 末尾追加：

```markdown
### P31 实现结果

- RHI 已新增 `ResourceState` 与 `ResourceBarrier`。
- `CommandList` 已新增 `ResourceBarrier` 接口，OpenGL backend 当前接受 texture barrier 并 no-op。
- `PassGraph` compile 阶段会基于现有 `Inputs`/`Outputs` 生成最小 barrier plan。
- 当前映射规则：input -> `ShaderRead`，output -> `RenderTarget`。
- 本轮未实现真实 GPU barrier、transient resource 创建、读写冲突自动排序或 Vulkan/D3D12 状态转换。
```

- [ ] **Step 3: diff check**

Run:

```powershell
git diff --check
git status --short
```

Expected: diff check clean；状态只包含 P30 未提交文件与本轮 P31 文件。

---

## 自检

- Spec 覆盖：本计划覆盖 P31 的资源状态类型、barrier 类型、OpenGL no-op 接口、RenderGraph barrier plan。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`ResourceState`、`ResourceBarrier`、`PassGraphResourceBarrier`、`GetBarrierPlan` 在任务间命名一致。

## P31 执行结果

- 已新增 `HuaEngine/src/HuaEngine/Rendering/RHI/ResourceBarrier.h`。
- `CommandList` 已新增 `ResourceBarrier` 接口，OpenGL backend 已实现 texture barrier no-op。
- `PassGraph` 已新增 `PassGraphResourceBarrier` 与 `GetBarrierPlan()`。
- `PassGraph::Compile()` 已基于 `Inputs` / `Outputs` 生成最小状态迁移计划。
- 当前映射规则：input -> `ShaderRead`，output -> `RenderTarget`。
- 已按 TDD 验证 RED：构建先因 `GetBarrierPlan` 与 `ResourceState` 未定义失败。
- 已验证 GREEN 与回归：

```powershell
cmake --build build --config Debug --target RenderPassGraphSmoke RHIResourceCreationSmoke RHICommandListBindingSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

结果：

```text
RenderPassGraphSmoke passed
RHIResourceCreationSmoke passed
RHICommandListBindingSmoke passed
RenderingOperationsSmoke passed
```
