# P33 Forward Explicit Vertex / Index Binding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Forward 主路径从 `SetVertexBufferView` compatibility path 迁移到 explicit `SetVertexBuffer` / `SetIndexBuffer` path。

**Architecture:** `ResolvedRenderItem` 保留 `VertexBufferViewRef` 作为 compatibility/layout 来源，同时新增 `VertexBufferBinding` 和 `IndexBufferBinding`。`RenderResourceResolver` 从 mesh 的 `VertexBufferViewDesc` 拆出 explicit binding；`ForwardOpaquePass` 使用 explicit binding 提交 draw。

**Tech Stack:** C++20、CMake、OpenGL backend、现有 smoke 测试框架。

---

## 文件结构

- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
  - include `VertexInputBinding.h`。
  - `ResolvedRenderItem` 增加 explicit vertex/index binding。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
  - 从 `VertexBufferViewRef->GetDesc()` 填充 binding。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`
  - 主 draw path 使用 `SetVertexBuffer` / `SetIndexBuffer`。
- Modify: `Tests/RenderingOperationsSmoke.cpp`
  - 添加源码约束，确保 Forward 主路径不再调用 `SetVertexBufferView`。
- Modify: `.workspace/superpower/specs/2026-07-15-rendering-modern-rhi-mainline-consumption-spec.md`
  - 追加 P33 实现结果。

## Task 1: RED - Forward 主路径禁止 SetVertexBufferView

**Files:**
- Modify: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: 写失败测试**

加入 include：

```cpp
#include <fstream>
```

在匿名 namespace 中加入：

```cpp
	bool ForwardPipelineUsesExplicitVertexIndexBinding() {
		const std::filesystem::path sourcePath =
			std::filesystem::current_path() / ".." / "HuaEngine" / "src" / "HuaEngine" / "Rendering" / "RenderPipeline" / "ForwardRenderPipeline.cpp";
		std::ifstream source(sourcePath);
		if (!source.is_open()) {
			return false;
		}

		const std::string content((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
		return content.find("SetVertexBufferView") == std::string::npos
			&& content.find("SetVertexBuffer(") != std::string::npos
			&& content.find("SetIndexBuffer(") != std::string::npos;
	}
```

在 `main()` 初始化 application 后加入：

```cpp
	Require(ForwardPipelineUsesExplicitVertexIndexBinding(), "Expected ForwardRenderPipeline main draw path to use explicit vertex/index binding");
```

- [ ] **Step 2: 运行并确认失败**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: smoke 运行失败，提示 Forward 主路径还在使用 `SetVertexBufferView`。

## Task 2: GREEN - ResolvedRenderItem 增加 explicit binding

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`

- [ ] **Step 1: 扩展 ResolvedRenderItem**

在 `RenderTypes.h` include 加入：

```cpp
#include "HuaEngine/Rendering/RHI/VertexInputBinding.h"
```

在 `ResolvedRenderItem` 中加入：

```cpp
		VertexBufferBinding VertexBinding;
		IndexBufferBinding IndexBinding;
```

- [ ] **Step 2: resolver 填充 binding**

在 `RenderResourceResolver::Resolve()` 中 `outResolvedItem.VertexBufferViewRef = mesh->GetVertexBufferView();` 后加入：

```cpp
		const auto& vertexBufferViewDesc = outResolvedItem.VertexBufferViewRef->GetDesc();
		outResolvedItem.VertexBinding = {
			.Buffer = vertexBufferViewDesc.VertexBuffer,
			.Offset = 0,
			.Stride = vertexBufferViewDesc.VertexBuffer ? vertexBufferViewDesc.VertexBuffer->GetDesc().Stride : 0
		};
		outResolvedItem.IndexBinding = {
			.Buffer = vertexBufferViewDesc.IndexBuffer,
			.Offset = 0,
			.Format = vertexBufferViewDesc.IndexFormatValue,
			.IndexCount = vertexBufferViewDesc.IndexCount
		};
```

## Task 3: GREEN - Forward 主路径使用 explicit binding

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`

- [ ] **Step 1: 替换 draw path**

将：

```cpp
if (resolvedItem.PipelineStateRef && resolvedItem.VertexBufferViewRef && resolvedItem.MaterialBindGroupRef && objectBindGroup) {
	context.Commands->SetPipelineState(*resolvedItem.PipelineStateRef);
	context.Commands->SetBindGroup(0, *frameBindGroup);
	context.Commands->SetVertexBufferView(*resolvedItem.VertexBufferViewRef);
	context.Commands->SetBindGroup(1, *resolvedItem.MaterialBindGroupRef);
	context.Commands->SetBindGroup(2, *objectBindGroup);
	context.Commands->DrawIndexed(resolvedItem.VertexBufferViewRef->GetDesc().IndexCount);
}
```

替换为：

```cpp
if (resolvedItem.PipelineStateRef
	&& resolvedItem.VertexBinding.Buffer
	&& resolvedItem.IndexBinding.Buffer
	&& resolvedItem.IndexBinding.IndexCount > 0
	&& resolvedItem.MaterialBindGroupRef
	&& objectBindGroup) {
	context.Commands->SetPipelineState(*resolvedItem.PipelineStateRef);
	context.Commands->SetBindGroup(0, *frameBindGroup);
	context.Commands->SetVertexBuffer(0, resolvedItem.VertexBinding);
	context.Commands->SetIndexBuffer(resolvedItem.IndexBinding);
	context.Commands->SetBindGroup(1, *resolvedItem.MaterialBindGroupRef);
	context.Commands->SetBindGroup(2, *objectBindGroup);
	context.Commands->DrawIndexed(resolvedItem.IndexBinding.IndexCount);
}
```

## Task 4: GREEN verification

**Files:**
- No additional production files.

- [ ] **Step 1: 构建并运行 P33 smoke**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RHICommandListBindingSmoke RHIResourceCreationSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
```

Expected:

```text
RenderingOperationsSmoke passed
RHICommandListBindingSmoke passed
RHIResourceCreationSmoke passed
```

- [ ] **Step 2: 搜索确认**

Run:

```powershell
rg -n "SetVertexBufferView" HuaEngine/src/HuaEngine/Rendering/RenderPipeline Tests -S
```

Expected: `ForwardRenderPipeline.cpp` 不再出现；`RHICommandListBindingSmoke.cpp` 可继续出现 compatibility path 覆盖。

## Task 5: regression verification and docs

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-15-rendering-modern-rhi-mainline-consumption-spec.md`
- Modify: `.workspace/superpower/plans/2026-07-15-rendering-p33-forward-explicit-binding-plan.md`

- [ ] **Step 1: 运行相关回归**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke RHICommandListBindingSmoke RHIResourceCreationSmoke RenderPassGraphSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
git diff --check
```

Expected: 四个 smoke 均 passed；diff check clean。

- [ ] **Step 2: 更新 spec**

在 spec 末尾追加：

```markdown
### P33 实现结果

- `ResolvedRenderItem` 已新增 explicit `VertexBinding` 与 `IndexBinding`。
- `RenderResourceResolver` 已从 mesh `VertexBufferView` 拆出 vertex/index binding。
- `ForwardOpaquePass` 已改用 `SetVertexBuffer` / `SetIndexBuffer`。
- `VertexBufferView` 保留为 compatibility/layout 来源。
- `RenderingOperationsSmoke` 已覆盖 Forward 主路径不再调用 `SetVertexBufferView`。
```

---

## 自检

- Spec 覆盖：本计划覆盖 P33 最小交付边界。
- 占位符扫描：无 TBD/TODO/implement later。
- 类型一致性：`VertexBinding`、`IndexBinding`、`SetVertexBuffer`、`SetIndexBuffer` 命名一致。

## P33 执行结果

- `ResolvedRenderItem` 已新增 `VertexBinding` 与 `IndexBinding`。
- `RenderResourceResolver` 已从 `VertexBufferViewDesc` 拆出 explicit vertex/index binding。
- `ForwardOpaquePass` 已使用 `SetVertexBuffer` / `SetIndexBuffer`，不再调用 `SetVertexBufferView`。
- `VertexBufferViewRef` 保留用于 layout 与 compatibility 过渡。
- 已按 TDD 验证 RED：`RenderingOperationsSmoke` 先因 Forward 仍使用 compatibility path 失败。
- 已验证 GREEN：

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderPassGraphSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderPassGraphSmoke.exe
```

结果：

```text
RenderingOperationsSmoke passed
RHICommandListBindingSmoke passed
RHIResourceCreationSmoke passed
RenderPassGraphSmoke passed
```
