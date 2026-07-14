# 渲染 RHI 兼容接口退场 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 draw 提交流程收敛到 `PipelineState + VertexBufferView + BindGroup + Draw`，删除公开 RHI 中迁就 OpenGL 的兼容接口。

**Architecture:** 在 RenderPipeline 层新增 `RenderBindGroupBuilder`，集中构建 frame/material/object bind group，避免 RHI 目录反向依赖 Material。Forward pass 使用 slot 0/1/2 的 bind group 提交资源，OpenGL backend 只在 `SetBindGroup` 内部把 bind group entry 映射为 uniform/texture。最后删除 `CommandList` 兼容方法和 `ShaderProgram` public uniform setter。

**Tech Stack:** C++17、HuaEngine Rendering/RHI、OpenGL backend、CMake smoke targets。

---

## 文件结构

- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h`
  - 声明 `CreateFrameBindGroup`、`CreateObjectBindGroup`、`CreateMaterialBindGroup`。
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.cpp`
  - 实现 frame/object/material bind group 构建和 `MaterialParameterType` 到 `BindingValueType` 的映射。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
  - 删除本地 `MaterialBinding` 构建逻辑，直接调用 `RenderBindGroupBuilder::CreateMaterialBindGroup`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
  - 删除 `MaterialBinding` include 和 `ResolvedRenderItem::MaterialBindingRef`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`
  - 删除 `FrameObjectBinding.h` include，使用 `CreateFrameBindGroup` / `CreateObjectBindGroup` 和 `SetBindGroup`。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
  - 删除 `SetShaderProgram`、`SetFrameBinding`、`SetMaterialBinding`、`SetObjectBinding` 和旧 binding include/forward declaration。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
  - 删除 command list 兼容 override 和旧 binding 成员；从 `OpenGLShaderProgram` uniform setter 声明上移除 `override`。
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
  - 删除 `SetShaderProgram`、`SetFrameBinding`、`SetMaterialBinding`、`SetObjectBinding` 实现；`SetPipelineState` 不 replay 旧 binding；`SetBindGroup` cast 到 `OpenGLShaderProgram` 后提交 backend uniform。
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/ShaderProgram.h`
  - 删除 public uniform setter，只保留 `GetDesc()`。
- Delete if unused: `HuaEngine/src/HuaEngine/Rendering/RHI/FrameObjectBinding.h`
- Delete if unused: `HuaEngine/src/HuaEngine/Rendering/Material/MaterialBinding.h`
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
  - 删除未使用的旧 binding include/helper。

---

### Task 1: 新增 RenderBindGroupBuilder 并迁移 material bind group 构建

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`

- [ ] **Step 1: 写入 builder 头文件**

`RenderBindGroupBuilder.h` 应声明：

```cpp
#pragma once

#include "HuaEngine/Core/Core.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	class BindGroup;
	class MaterialInstance;
	class RenderDevice;

	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection);
	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance);
}
```

- [ ] **Step 2: 实现 builder**

`RenderBindGroupBuilder.cpp` 应包含：

```cpp
#include "enginepch.h"
#include "RenderBindGroupBuilder.h"

#include <type_traits>

#include "HuaEngine/Rendering/Material/MaterialCore.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	namespace {
		BindingValueType ToBindingValueType(MaterialParameterType type) {
			switch (type) {
				case MaterialParameterType::Int: return BindingValueType::Int;
				case MaterialParameterType::Float: return BindingValueType::Float;
				case MaterialParameterType::Vec2: return BindingValueType::Float2;
				case MaterialParameterType::Vec3: return BindingValueType::Float3;
				case MaterialParameterType::Vec4: return BindingValueType::Float4;
				case MaterialParameterType::Mat3: return BindingValueType::Mat3;
				case MaterialParameterType::Mat4: return BindingValueType::Mat4;
				case MaterialParameterType::IntArray: return BindingValueType::IntArray;
				case MaterialParameterType::Texture2D:
				case MaterialParameterType::TextureCube: return BindingValueType::Texture;
				case MaterialParameterType::FloatArray: break;
			}

			return BindingValueType::Float;
		}

		Ref<BindGroup> CreateSingleMat4BindGroup(
			RenderDevice& device,
			BindGroupScope scope,
			const char* name,
			const glm::mat4& value) {
			auto layout = device.CreateBindGroupLayout({
				.Scope = scope,
				.Entries = {
					{
						.Name = name,
						.Type = BindingValueType::Mat4,
						.Binding = 0
					}
				}
			});
			if (!layout) {
				return nullptr;
			}

			return device.CreateBindGroup({
				.Layout = layout,
				.Entries = {
					{
						.Name = name,
						.Type = BindingValueType::Mat4,
						.Value = value,
						.Binding = 0
					}
				}
			});
		}

		void AddMaterialParameterEntry(
			std::vector<BindGroupEntry>& entries,
			const MaterialParameter& parameter,
			uint32_t textureSlot) {
			if (parameter.Type == MaterialParameterType::FloatArray) {
				return;
			}

			if (parameter.Type == MaterialParameterType::Texture2D || parameter.Type == MaterialParameterType::TextureCube) {
				const auto* texture = std::get_if<Ref<TextureResource>>(&parameter.Value);
				if (!texture || !*texture) {
					return;
				}

				entries.push_back({
					.Name = parameter.Name,
					.Type = BindingValueType::Texture,
					.Value = *texture,
					.Binding = static_cast<uint32_t>(entries.size()),
					.TextureSlot = textureSlot
				});
				return;
			}

			std::visit([&](auto&& value) {
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, std::vector<float>> || std::is_same_v<T, Ref<TextureResource>>) {
					return;
				}
				else {
					entries.push_back({
						.Name = parameter.Name,
						.Type = ToBindingValueType(parameter.Type),
						.Value = value,
						.Binding = static_cast<uint32_t>(entries.size())
					});
				}
			}, parameter.Value);
		}
	}

	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection) {
		return CreateSingleMat4BindGroup(device, BindGroupScope::Frame, "u_ViewProjection", viewProjection);
	}

	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform) {
		return CreateSingleMat4BindGroup(device, BindGroupScope::Object, "u_Transform", transform);
	}

	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance) {
		auto baseMaterial = materialInstance.GetBaseMaterial();
		if (!baseMaterial) {
			return nullptr;
		}

		std::vector<BindGroupEntry> entries;
		entries.reserve(baseMaterial->GetParameters().size() + materialInstance.GetParameterOverrides().size());

		for (const auto& [name, parameter] : baseMaterial->GetParameters()) {
			const auto* overrideParameter = materialInstance.GetParameterOverride(name);
			const auto& selectedParameter = overrideParameter ? *overrideParameter : parameter;
			AddMaterialParameterEntry(entries, selectedParameter, baseMaterial->GetTextureSlot(name));
		}

		for (const auto& [name, parameter] : materialInstance.GetParameterOverrides()) {
			if (baseMaterial->HasParameter(name)) {
				continue;
			}

			AddMaterialParameterEntry(entries, parameter, baseMaterial->GetTextureSlot(name));
		}

		if (entries.empty()) {
			return nullptr;
		}

		std::vector<BindGroupLayoutEntry> layoutEntries;
		layoutEntries.reserve(entries.size());
		for (const auto& entry : entries) {
			layoutEntries.push_back({
				.Name = entry.Name,
				.Type = entry.Type,
				.Binding = entry.Binding
			});
		}

		auto layout = device.CreateBindGroupLayout({
			.Scope = BindGroupScope::Material,
			.Entries = std::move(layoutEntries)
		});
		if (!layout) {
			return nullptr;
		}

		return device.CreateBindGroup({
			.Layout = layout,
			.Entries = std::move(entries)
		});
	}
}
```

- [ ] **Step 3: 迁移 resolver**

在 `RenderResourceResolver.cpp` 中：
- include `RenderBindGroupBuilder.h`。
- 删除匿名 namespace 内的 `AddMaterialBindingParameter`、`BuildMaterialBinding`、`ToBindingValueType`、`AddMaterialBindGroupEntry`、`BuildMaterialBindGroup`。
- 将 resolve 末尾改为：

```cpp
outResolvedItem.MaterialInstanceRef = materialInstance;
outResolvedItem.MaterialBindGroupRef = CreateMaterialBindGroup(RenderHardwareInterface::GetDevice(), *materialInstance);
outResolvedItem.VertexBufferViewRef = mesh->GetVertexBufferView();
```

- [ ] **Step 4: 删除 resolved material binding 字段**

在 `RenderTypes.h` 中删除：

```cpp
#include "HuaEngine/Rendering/Material/MaterialBinding.h"
Ref<MaterialBinding> MaterialBindingRef;
```

- [ ] **Step 5: 构建验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RenderingOperationsSmoke
```

Expected:
- 编译通过。
- 如果新增 cpp 未进入构建，检查 `HuaEngine/CMakeLists.txt` 的 recursive glob 是否覆盖新文件。

---

### Task 2: Forward pass 改用 frame/object bind group

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`

- [ ] **Step 1: 替换 include**

删除：

```cpp
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
```

新增：

```cpp
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
```

- [ ] **Step 2: 在 pass 内创建 frame bind group**

在 `ForwardOpaquePass::Execute` 的 pass count 之后、item loop 之前添加：

```cpp
auto frameBindGroup = CreateFrameBindGroup(
	RenderHardwareInterface::GetDevice(),
	context.View->CameraRef->GetViewProjection());
if (!frameBindGroup) {
	context.Diagnostics->push_back({
		RenderDiagnosticCode::MissingRhiDrawResources,
		Entity{},
		"Forward opaque pass skipped because frame bind group creation failed"
	});
	return;
}
```

如果 `Entity{}` 不能默认构造，改为使用 `0` 或当前 `Entity` 类型的空值写法。

- [ ] **Step 3: 替换 draw submission**

将 draw 路径替换为：

```cpp
auto objectBindGroup = CreateObjectBindGroup(RenderHardwareInterface::GetDevice(), item.Transform);
if (resolvedItem.PipelineStateRef && resolvedItem.VertexBufferViewRef && resolvedItem.MaterialBindGroupRef && objectBindGroup) {
	context.Commands->SetPipelineState(*resolvedItem.PipelineStateRef);
	context.Commands->SetBindGroup(0, *frameBindGroup);
	context.Commands->SetVertexBufferView(*resolvedItem.VertexBufferViewRef);
	context.Commands->SetBindGroup(1, *resolvedItem.MaterialBindGroupRef);
	context.Commands->SetBindGroup(2, *objectBindGroup);
	context.Commands->DrawIndexed(resolvedItem.VertexBufferViewRef->GetDesc().IndexCount);
}
```

- [ ] **Step 4: 残留搜索**

Run:

```powershell
rg -n -S "SetFrameBinding\(|SetObjectBinding\(|FrameObjectBinding" HuaEngine/src/HuaEngine/Rendering/RenderPipeline
```

Expected:
- 无输出。

- [ ] **Step 5: 运行相关 smoke**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke ApplicationOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 两个 smoke 均输出 passed。

---

### Task 3: 删除 CommandList 兼容接口和旧 binding 类型

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/CommandList.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
- Modify: `Tests/RHICommandListBindingSmoke.cpp`
- Delete if unused: `HuaEngine/src/HuaEngine/Rendering/RHI/FrameObjectBinding.h`
- Delete if unused: `HuaEngine/src/HuaEngine/Rendering/Material/MaterialBinding.h`

- [ ] **Step 1: 清理 CommandList.h**

删除：

```cpp
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
class MaterialBinding;
class ShaderProgram;
virtual void SetShaderProgram(ShaderProgram& shaderProgram) = 0;
virtual void SetFrameBinding(const FrameBinding& binding) = 0;
virtual void SetMaterialBinding(const MaterialBinding& binding) = 0;
virtual void SetObjectBinding(const ObjectBinding& binding) = 0;
```

保留：

```cpp
virtual void SetPipelineState(PipelineState& pipelineState) = 0;
virtual void SetVertexBufferView(VertexBufferView& vertexBufferView) = 0;
virtual void SetBindGroup(uint32_t slot, BindGroup& bindGroup) = 0;
virtual void DrawIndexed(uint32_t indexCount) = 0;
```

- [ ] **Step 2: 清理 OpenGLCommandList 声明和成员**

在 `OpenGLRenderDevice.h` 删除：

```cpp
class MaterialBinding;
void SetShaderProgram(ShaderProgram& shaderProgram) override;
void SetFrameBinding(const FrameBinding& binding) override;
void SetMaterialBinding(const MaterialBinding& binding) override;
void SetObjectBinding(const ObjectBinding& binding) override;
FrameBinding m_CurrentFrameBinding;
ObjectBinding m_CurrentObjectBinding;
```

- [ ] **Step 3: 清理 OpenGLCommandList 实现**

在 `OpenGLRenderDevice.cpp` 删除 include：

```cpp
#include "HuaEngine/Rendering/Material/MaterialBinding.h"
```

删除方法实现：

```cpp
void OpenGLCommandList::SetShaderProgram(...)
void OpenGLCommandList::SetFrameBinding(...)
void OpenGLCommandList::SetMaterialBinding(...)
void OpenGLCommandList::SetObjectBinding(...)
```

将 `SetPipelineState` 改为只绑定 pipeline shader：

```cpp
void OpenGLCommandList::SetPipelineState(PipelineState& pipelineState) {
	m_CurrentPipelineState = &pipelineState;
	auto& shaderProgram = static_cast<OpenGLPipelineState&>(pipelineState).GetShaderProgram();
	m_CurrentShaderProgram = &shaderProgram;
	static_cast<OpenGLShaderProgram&>(shaderProgram).BindForCommandList();
}
```

将 `EndFrame` 中旧 binding reset 删除：

```cpp
m_CurrentFrameBinding = {};
m_CurrentObjectBinding = {};
```

- [ ] **Step 4: 删除测试中的旧 include/helper**

在 `Tests/RHICommandListBindingSmoke.cpp` 删除：

```cpp
#include "HuaEngine/Rendering/Material/MaterialBinding.h"
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
HE::Rendering::MaterialBinding MakeColorBinding(...)
```

- [ ] **Step 5: 删除无人引用的旧类型文件**

Run:

```powershell
rg -n -S "FrameBinding|ObjectBinding|MaterialBinding" HuaEngine/src Tests
```

Expected:
- 如果只有两个旧 header 自身残留，删除：

```powershell
Remove-Item 'HuaEngine\src\HuaEngine\Rendering\RHI\FrameObjectBinding.h'
Remove-Item 'HuaEngine\src\HuaEngine\Rendering\Material\MaterialBinding.h'
```

- [ ] **Step 6: 编译验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RenderingOperationsSmoke ApplicationOperationsSmoke
```

Expected:
- 编译通过。

---

### Task 4: 删除 ShaderProgram public uniform setter

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RHI/ShaderProgram.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- Modify: `HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`

- [ ] **Step 1: 清理 ShaderProgram.h**

删除 includes：

```cpp
#include <cstdint>
#include "glm/glm.hpp"
```

删除 public uniform setter：

```cpp
virtual void SetInt(...) = 0;
virtual void SetIntArray(...) = 0;
virtual void SetFloat(...) = 0;
virtual void SetFloat2(...) = 0;
virtual void SetFloat3(...) = 0;
virtual void SetFloat4(...) = 0;
virtual void SetMat3(...) = 0;
virtual void SetMat4(...) = 0;
```

保留：

```cpp
virtual const ShaderProgramDesc& GetDesc() const = 0;
```

- [ ] **Step 2: OpenGLShaderProgram setter 去 override**

在 `OpenGLRenderDevice.h` 中将：

```cpp
void SetInt(const std::string& name, int value) override;
...
void SetMat4(const std::string& name, const glm::mat4 value) override;
```

改为：

```cpp
void SetInt(const std::string& name, int value);
...
void SetMat4(const std::string& name, const glm::mat4 value);
```

- [ ] **Step 3: SetBindGroup 内使用 OpenGLShaderProgram**

在 `OpenGLCommandList::SetBindGroup` 中添加 backend 引用：

```cpp
auto& shaderProgram = static_cast<OpenGLShaderProgram&>(*m_CurrentShaderProgram);
```

并将所有：

```cpp
m_CurrentShaderProgram->SetInt(...)
m_CurrentShaderProgram->SetFloat(...)
m_CurrentShaderProgram->SetMat4(...)
```

替换为：

```cpp
shaderProgram.SetInt(...)
shaderProgram.SetFloat(...)
shaderProgram.SetMat4(...)
```

- [ ] **Step 4: uniform setter 残留搜索**

Run:

```powershell
rg -n -S "virtual void SetInt|virtual void SetFloat|virtual void SetMat|override;" HuaEngine/src/HuaEngine/Rendering/RHI/ShaderProgram.h HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h
rg -n -S "->SetInt|->SetFloat|->SetMat|SetShaderProgram\(" HuaEngine/src Editor/src Tests
```

Expected:
- `ShaderProgram.h` 无 uniform setter。
- `OpenGLRenderDevice.h` 的 OpenGL backend setter 不带 `override`。
- `SetShaderProgram(` 只允许作为 `Material::SetShaderProgram(...)` 资源语义存在。

- [ ] **Step 5: 编译验证**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke RenderingOperationsSmoke
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected:
- 三个 smoke 均输出 passed。

---

### Task 5: 最终残留审计和提交

**Files:**
- Modify: `.workspace/superpower/specs/2026-07-14-rendering-rhi-compatibility-interface-retirement-design.md`

- [ ] **Step 1: 运行全量相关构建**

Run:

```powershell
cmake --build build --config Debug --target RHICommandListBindingSmoke RHIResourceCreationSmoke MaterialSerializationSmoke RenderingOperationsSmoke AssetServiceSmoke ApplicationOperationsSmoke Editor
```

Expected:
- 所有 target 编译通过。

- [ ] **Step 2: 运行 smoke**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\RHICommandListBindingSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RHIResourceCreationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

Expected:
- 所有 smoke 输出 passed。

- [ ] **Step 3: 最终残留搜索**

Run:

```powershell
rg -n -S "SetShaderProgram\(|SetFrameBinding\(|SetMaterialBinding\(|SetObjectBinding\(|FrameBinding|ObjectBinding|MaterialBinding|virtual void SetInt|virtual void SetFloat|virtual void SetMat|GetRenderID\(|GetColorAttachment\(" HuaEngine/src/HuaEngine/Rendering/RHI HuaEngine/src/HuaEngine/Rendering/RenderPipeline HuaEngine/src/Platform/OpenGL/RHI Tests
```

Expected:
- 不再出现 public command 兼容接口。
- 允许 `Material::SetShaderProgram(...)` 不在本搜索范围内出现。
- 允许 OpenGL backend 内部 `GetRenderID/GetColorAttachment` storage bridge 残留。

- [ ] **Step 4: 更新 spec 状态**

将 `2026-07-14-rendering-rhi-compatibility-interface-retirement-design.md` 顶部状态从 `草案` 改为：

```text
状态：已实现
```

并添加实现摘要：

```markdown
## 10. 实现结果

- 已新增 `RenderBindGroupBuilder`，frame/material/object 均通过 bind group 构建。
- Forward draw 路径已改为 `SetPipelineState` + `SetBindGroup(0/1/2)` + `SetVertexBufferView` + `DrawIndexed`。
- `CommandList` 已移除 OpenGL 迁移期兼容接口。
- `ShaderProgram` public uniform setter 已移除，OpenGL uniform 写入收敛为 backend 细节。
- 旧 `FrameObjectBinding` / `MaterialBinding` 类型已删除或确认不再被 command path 依赖。
```

- [ ] **Step 5: diff 和提交**

Run:

```powershell
git diff --check
git status --short
git add HuaEngine/src Tests .workspace/superpower/specs/2026-07-14-rendering-rhi-compatibility-interface-retirement-design.md .workspace/superpower/plans/2026-07-14-rendering-rhi-compatibility-interface-retirement-plan.md
git commit -m "refactor(rendering): retire rhi compatibility bindings"
```

Expected:
- `git diff --check` 无输出。
- commit 成功。

---

## 自检

- Spec 覆盖：R27-1 到 R27-4 均有对应任务；非目标不纳入实现。
- 占位符扫描：本 plan 不使用 TBD/TODO/implement later。
- 类型一致性：builder API 与 spec 推荐签名一致，`CommandList` 最终只保留现代提交接口。
