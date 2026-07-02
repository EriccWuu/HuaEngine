# Render Refactor P0-P3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a reliable, verifiable render baseline and refactor the render path through P0-P3 without introducing a full RHI.

**Architecture:** P0 adds framebuffer readback, depth reliability, and null-safety. P1 introduces `RenderView`, `RenderItem`, `SceneRenderExtractor`, and a pipeline entry so editor/runtime rendering share one path. P2 centralizes mesh/material resolution and diagnostics. P3 adds a lightweight forward pass, render stats, and observable submission results.

**Tech Stack:** C++20, CMake, OpenGL/GLAD, GLFW, EnTT-backed HuaEngine ECS, existing smoke executables.

---

## File Structure

- Modify: `HuaEngine/src/HuaEngine/Rendering/FrameBuffer.h`
  - Add a minimal RGBA8 pixel readback result type and virtual readback method.
- Modify: `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.h`
  - Declare the OpenGL readback implementation.
- Modify: `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.cpp`
  - Fix depth attachment creation and implement RGBA8 readback.
- Modify: `HuaEngine/src/HuaEngine/Rendering/Material/MaterialCore.h`
  - Make `MaterialInstance::GetShader()` null-safe.
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
  - Add P0 framebuffer guard, then migrate to extractor/pipeline in P1.
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.h`
  - Add pipeline ownership/accessors needed by P1-P3.
- Modify: `Editor/src/EditorLayer.cpp`
  - Add depth attachment to the Game framebuffer specification.
- Modify: `Tests/RenderingOperationsSmoke.cpp`
  - Add P0 pixel verification and later stats/diagnostics assertions.
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
  - Define `RenderView`, `RenderItem`, `RenderStats`, `RenderDiagnostic`, and result structs.
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
  - Extract render items from `World`.
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp`
  - Replace the legacy scene/camera interface with `Render(RenderView, items)`.
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
  - Resolve mesh/material runtime resources and diagnostics.
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`
  - Implement the default forward opaque pass with stats.
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
  - Use the shared render path and expose stats in result payload where useful.

---

### Task 1: P0 Rendering Reliability Baseline

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/FrameBuffer.h`
- Modify: `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.h`
- Modify: `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/Material/MaterialCore.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: Write the failing smoke assertions**

In `Tests/RenderingOperationsSmoke.cpp`, add helpers before `main()`:

```cpp
bool IsClearColor(const HE::Rendering::FrameBufferPixelRGBA8& pixel) {
	return pixel.R == 26 && pixel.G == 26 && pixel.B == 26 && pixel.A == 255;
}

bool HasRenderedPixel(const HE::Ref<HE::Rendering::FrameBuffer>& framebuffer) {
	const auto& spec = framebuffer->GetSpecification();
	const uint32_t sampleX[] = { spec.Width / 2, spec.Width / 3, (spec.Width * 2) / 3 };
	const uint32_t sampleY[] = { spec.Height / 2, spec.Height / 3, (spec.Height * 2) / 3 };

	for (uint32_t y : sampleY) {
		for (uint32_t x : sampleX) {
			const auto pixel = framebuffer->ReadPixelRGBA8(0, x, y);
			if (!IsClearColor(pixel)) {
				return true;
			}
		}
	}

	return false;
}
```

Update framebuffer creation in the smoke:

```cpp
specification.Attachments = {
	HE::FrameBufferTextureFormat::RGBA8,
	HE::FrameBufferTextureFormat::DEPTH24_STENCIL8
};
```

After `renderLoadedScene` succeeds, add:

```cpp
Require(HasRenderedPixel(framebuffer), "Expected loaded sandbox scene render to write a non-clear pixel");
```

- [ ] **Step 2: Run the smoke build and confirm RED**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
```

Expected: compile fails because `FrameBufferPixelRGBA8` and `ReadPixelRGBA8` do not exist.

- [ ] **Step 3: Add minimal framebuffer readback API**

In `HuaEngine/src/HuaEngine/Rendering/FrameBuffer.h`, add:

```cpp
struct FrameBufferPixelRGBA8 {
	uint8_t R = 0;
	uint8_t G = 0;
	uint8_t B = 0;
	uint8_t A = 0;
};
```

Inside `class FrameBuffer`, add:

```cpp
virtual FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const = 0;
```

In `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.h`, add:

```cpp
virtual FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const override;
```

In `HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.cpp`, implement:

```cpp
FrameBufferPixelRGBA8 OpenGLFrameBuffer::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
	HE_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Color attachment index out of range");
	HE_CORE_ASSERT(x < m_Specification.Width && y < m_Specification.Height, "Framebuffer readback coordinates out of range");
	HE_CORE_ASSERT(m_ColorAttachmentSpecifications[attachmentIndex].Format == FrameBufferTextureFormat::RGBA8, "ReadPixelRGBA8 only supports RGBA8 attachments");

	FrameBufferPixelRGBA8 pixel;
	const GLint previousFramebuffer = [] {
		GLint current = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &current);
		return current;
	}();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RenderID);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
	glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, previousFramebuffer);

	return pixel;
}
```

- [ ] **Step 4: Fix depth attachment creation**

In `OpenGLFrameBuffer::Invalidate()`, change the depth switch case to:

```cpp
case FrameBufferTextureFormat::DEPTH24_STENCIL8:
	Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
	break;
```

- [ ] **Step 5: Add default depth attachment to Editor Game framebuffer**

In `Editor/src/EditorLayer.cpp`, update `InitializeWorkbenchShell()`:

```cpp
spec.Attachments = {
	FrameBufferTextureFormat::RGBA8,
	FrameBufferTextureFormat::DEPTH24_STENCIL8
};
```

- [ ] **Step 6: Add null-safety and framebuffer guard**

In `MaterialCore.h`, replace `MaterialInstance::GetShader()` with:

```cpp
Ref<Shader> GetShader() const { return m_BaseMaterial ? m_BaseMaterial->GetShader() : nullptr; }
```

At the start of `RenderSystem::RenderSingleCamera` in `RenderSystem.cpp`, add:

```cpp
if (!m_Framebuffer) {
	HE_CORE_WARN("RenderSystem cannot render without a framebuffer");
	return;
}
```

- [ ] **Step 7: Run P0 verification**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
cmake --build build --config Debug --target MaterialSerializationSmoke
& .\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
```

Expected: both executables exit 0 and print their pass messages.

- [ ] **Step 8: Commit P0**

```powershell
git add -- HuaEngine/src/HuaEngine/Rendering/FrameBuffer.h HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.h HuaEngine/src/Platform/OpenGL/OpenGLFrameBuffer.cpp HuaEngine/src/HuaEngine/Rendering/Material/MaterialCore.h HuaEngine/src/Module/Rendering/RenderSystem.cpp Editor/src/EditorLayer.cpp Tests/RenderingOperationsSmoke.cpp
git commit -m "fix(render): add verifiable framebuffer baseline"
```

---

### Task 2: P1 Render View, Item Extraction, and Shared Pipeline Path

**Files:**
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: Write failing payload/assertion expectations**

In `Tests/RenderingOperationsSmoke.cpp`, after each successful viewport render, assert the shared pipeline payload:

```cpp
Require(renderViewport.Payload.contains("render_items"), "Expected render operation to report extracted render item count");
Require(renderViewport.Payload.contains("submitted_items"), "Expected render operation to report submitted item count");
```

After `renderLoadedScene`, add:

```cpp
Require(renderLoadedScene.Payload.contains("render_items"), "Expected loaded scene render to report extracted render item count");
Require(renderLoadedScene.Payload.at("render_items") == "4", "Expected loaded sandbox scene to extract four render items");
```

- [ ] **Step 2: Run RED**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: executable fails because render payload does not contain `render_items` or `submitted_items`.

- [ ] **Step 3: Define render data contracts**

Create `RenderTypes.h`:

```cpp
#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	struct RenderView {
		Ref<Camera> CameraRef;
		Ref<FrameBuffer> Target;
		glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
		bool ClearColorBuffer = true;
	};

	struct RenderItem {
		Entity SourceEntity;
		glm::mat4 Transform = glm::mat4(1.0f);
		Ref<VertexArray> VertexArrayRef;
		Ref<MaterialInstance> MaterialInstanceRef;
	};

	struct RenderStats {
		uint32_t RenderItems = 0;
		uint32_t SubmittedItems = 0;
		uint32_t SkippedItems = 0;
		uint32_t DrawCalls = 0;
		uint32_t VisibleItems = 0;
		uint32_t PassCount = 0;
	};

	struct RenderResult {
		bool Succeeded = false;
		RenderStats Stats;
	};
}
```

- [ ] **Step 4: Implement extractor**

Create `SceneRenderExtractor.h`:

```cpp
#pragma once

#include "RenderTypes.h"
#include "HuaEngine/ECS/World.h"
#include <vector>

namespace HE::Rendering {
	class SceneRenderExtractor {
	public:
		std::vector<RenderItem> Extract(World& world) const;
	};
}
```

Create `SceneRenderExtractor.cpp`:

```cpp
#include "enginepch.h"
#include "SceneRenderExtractor.h"

#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	std::vector<RenderItem> SceneRenderExtractor::Extract(World& world) const {
		std::vector<RenderItem> items;
		auto query = world.Query<TransformComponent, MeshComponent, MaterialComponent>();
		query.ForEach([&](Entity entity, TransformComponent& transform, MeshComponent& mesh, MaterialComponent& material) {
			RenderItem item;
			item.SourceEntity = entity;
			item.Transform = transform.GetTransformMat();
			item.VertexArrayRef = mesh.GetVertexArray();
			item.MaterialInstanceRef = material.MaterialInstance;
			items.push_back(item);
		});
		return items;
	}
}
```

- [ ] **Step 5: Replace pipeline legacy interface**

Update `RenderPipeline.h`:

```cpp
#pragma once

#include "RenderTypes.h"
#include <vector>

namespace HE::Rendering {
	class RenderPipeline {
	public:
		virtual ~RenderPipeline();
		virtual RenderResult Render(const RenderView& view, const std::vector<RenderItem>& items);
	};
}
```

Update `RenderPipeline.cpp`:

```cpp
#include "enginepch.h"
#include "RenderPipeline.h"

#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/Renderer.h"

namespace HE::Rendering {
	RenderPipeline::~RenderPipeline() = default;

	RenderResult RenderPipeline::Render(const RenderView& view, const std::vector<RenderItem>& items) {
		RenderResult result;
		result.Stats.RenderItems = static_cast<uint32_t>(items.size());
		result.Stats.VisibleItems = result.Stats.RenderItems;

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		view.Target->Bind();
		if (view.ClearColorBuffer) {
			RenderCommand::SetClearColor(view.ClearColor);
			RenderCommand::Clear();
		}

		Renderer::Begin(view.CameraRef);
		for (const auto& item : items) {
			if (item.MaterialInstanceRef && item.VertexArrayRef) {
				Renderer::Submit(item.MaterialInstanceRef, item.VertexArrayRef, item.Transform);
				++result.Stats.SubmittedItems;
				++result.Stats.DrawCalls;
			}
			else {
				++result.Stats.SkippedItems;
			}
		}
		Renderer::End();
		view.Target->Unbind();

		result.Succeeded = true;
		return result;
	}
}
```

- [ ] **Step 6: Route RenderSystem and ApplicationOperations through pipeline**

In `RenderSystem.h`, include `SceneRenderExtractor.h`, add members:

```cpp
Rendering::SceneRenderExtractor m_Extractor;
Scope<Rendering::RenderPipeline> m_Pipeline;
Rendering::RenderResult m_LastRenderResult;
```

Add public accessor:

```cpp
const Rendering::RenderResult& GetLastRenderResult() const { return m_LastRenderResult; }
```

Initialize `m_Pipeline` in constructor:

```cpp
RenderSystem(Ref<Scene> scene)
	: m_Scene(scene), m_Pipeline(CreateScope<Rendering::RenderPipeline>()) {}
```

In `RenderSystem::RenderSingleCamera`, replace direct entity submit loop with:

```cpp
if (!m_Framebuffer) {
	HE_CORE_WARN("RenderSystem cannot render without a framebuffer");
	m_LastRenderResult = {};
	return;
}

Rendering::RenderView view;
view.CameraRef = CreateRef<Rendering::Camera>(camera);
view.Target = m_Framebuffer;
view.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

auto items = m_Extractor.Extract(world);
m_LastRenderResult = m_Pipeline->Render(view, items);
```

In `ApplicationOperations::RenderSceneViewport`, after rendering, add payload:

```cpp
const auto& renderResult = renderSystem->GetLastRenderResult();
if (!renderResult.Succeeded) {
	auto result = ResultEnvelope::Failure("rendering.render_scene_viewport", scene.GetName(), "Scene viewport render failed");
	result.AddDetail({ DiagnosticSeverity::Error, "rendering.render_scene_viewport.failed", "Render pipeline failed to render the scene viewport", {} });
	return result;
}

auto result = ResultEnvelope::Success("rendering.render_scene_viewport", scene.GetName(), "Scene viewport rendered");
result.SetPayloadValue("scene_name", scene.GetName());
result.SetPayloadValue("render_items", std::to_string(renderResult.Stats.RenderItems));
result.SetPayloadValue("submitted_items", std::to_string(renderResult.Stats.SubmittedItems));
return result;
```

- [ ] **Step 7: Run P1 verification**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: build succeeds; smoke exits 0; loaded scene reports four extracted render items.

- [ ] **Step 8: Commit P1**

```powershell
git add -- HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp HuaEngine/src/Module/Rendering/RenderSystem.h HuaEngine/src/Module/Rendering/RenderSystem.cpp HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp Tests/RenderingOperationsSmoke.cpp
git commit -m "refactor(render): route scene rendering through pipeline"
```

---

### Task 3: P2 Resource Resolver and Render Diagnostics

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: Add failing diagnostics expectations**

In `Tests/RenderingOperationsSmoke.cpp`, after `renderLoadedScene`, add:

```cpp
Require(renderLoadedScene.Payload.contains("skipped_items"), "Expected render operation to report skipped item count");
Require(renderLoadedScene.Payload.at("submitted_items") == "4", "Expected loaded sandbox scene to submit four render items");
Require(renderLoadedScene.Payload.at("skipped_items") == "0", "Expected loaded sandbox scene to skip no render items");
```

- [ ] **Step 2: Run RED**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: executable fails because `skipped_items` is not in the payload yet.

- [ ] **Step 3: Extend render contracts for stable references and diagnostics**

In `RenderTypes.h`, update `RenderItem`:

```cpp
std::string MeshAssetName;
Ref<MaterialInstance> MaterialInstanceRef;
```

Remove `VertexArrayRef` from `RenderItem`.

Add:

```cpp
enum class RenderDiagnosticCode {
	MissingMeshAsset,
	MissingVertexArray,
	MissingMaterialInstance,
	MissingBaseMaterial,
	MissingShader
};

struct RenderDiagnostic {
	RenderDiagnosticCode Code = RenderDiagnosticCode::MissingMeshAsset;
	Entity SourceEntity;
	std::string Message;
};

struct ResolvedRenderItem {
	const RenderItem* Source = nullptr;
	Ref<VertexArray> VertexArrayRef;
	Ref<MaterialInstance> MaterialInstanceRef;
};
```

Add to `RenderResult`:

```cpp
std::vector<RenderDiagnostic> Diagnostics;
```

- [ ] **Step 4: Make extractor stop resolving GPU mesh resources**

In `SceneRenderExtractor.cpp`, set:

```cpp
item.MeshAssetName = mesh.MeshAssetName;
item.MaterialInstanceRef = material.MaterialInstance;
```

Do not call `mesh.GetVertexArray()` in the extractor.

- [ ] **Step 5: Implement resource resolver**

Create `RenderResourceResolver.h`:

```cpp
#pragma once

#include "RenderTypes.h"

namespace HE::Rendering {
	class RenderResourceResolver {
	public:
		bool Resolve(const RenderItem& item, ResolvedRenderItem& resolved, std::vector<RenderDiagnostic>& diagnostics) const;
	};
}
```

Create `RenderResourceResolver.cpp`:

```cpp
#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Rendering/Mesh/MeshManager.h"

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(std::vector<RenderDiagnostic>& diagnostics, RenderDiagnosticCode code, Entity entity, const std::string& message) {
			RenderDiagnostic diagnostic;
			diagnostic.Code = code;
			diagnostic.SourceEntity = entity;
			diagnostic.Message = message;
			diagnostics.push_back(std::move(diagnostic));
		}
	}

	bool RenderResourceResolver::Resolve(const RenderItem& item, ResolvedRenderItem& resolved, std::vector<RenderDiagnostic>& diagnostics) const {
		resolved = {};
		resolved.Source = &item;

		if (item.MeshAssetName.empty()) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingMeshAsset, item.SourceEntity, "Render item has no mesh asset name");
			return false;
		}

		auto mesh = MeshManager::Instance().GetMesh(item.MeshAssetName);
		if (!mesh) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingMeshAsset, item.SourceEntity, "Mesh asset is not registered: " + item.MeshAssetName);
			return false;
		}

		resolved.VertexArrayRef = mesh->GetVertexArray();
		if (!resolved.VertexArrayRef) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingVertexArray, item.SourceEntity, "Mesh asset has no vertex array: " + item.MeshAssetName);
			return false;
		}

		resolved.MaterialInstanceRef = item.MaterialInstanceRef;
		if (!resolved.MaterialInstanceRef) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingMaterialInstance, item.SourceEntity, "Render item has no material instance");
			return false;
		}

		if (!resolved.MaterialInstanceRef->GetBaseMaterial()) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingBaseMaterial, item.SourceEntity, "Material instance has no base material");
			return false;
		}

		if (!resolved.MaterialInstanceRef->GetShader()) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingShader, item.SourceEntity, "Material instance has no shader");
			return false;
		}

		return true;
	}
}
```

- [ ] **Step 6: Use resolver in pipeline**

In `RenderPipeline.h`, include `RenderResourceResolver.h` and change render signature:

```cpp
virtual RenderResult Render(const RenderView& view, const std::vector<RenderItem>& items, const RenderResourceResolver& resolver);
```

In `RenderPipeline.cpp`, for each item:

```cpp
ResolvedRenderItem resolved;
if (resolver.Resolve(item, resolved, result.Diagnostics)) {
	Renderer::Submit(resolved.MaterialInstanceRef, resolved.VertexArrayRef, item.Transform);
	++result.Stats.SubmittedItems;
	++result.Stats.DrawCalls;
}
else {
	++result.Stats.SkippedItems;
}
```

Update `RenderSystem` to own:

```cpp
Rendering::RenderResourceResolver m_ResourceResolver;
```

Call:

```cpp
m_LastRenderResult = m_Pipeline->Render(view, items, m_ResourceResolver);
```

- [ ] **Step 7: Expose skipped/diagnostic counts**

In `ApplicationOperations::RenderSceneViewport`, add:

```cpp
result.SetPayloadValue("skipped_items", std::to_string(renderResult.Stats.SkippedItems));
result.SetPayloadValue("diagnostics", std::to_string(renderResult.Diagnostics.size()));
```

- [ ] **Step 8: Run P2 verification**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
cmake --build build --config Debug --target MaterialSerializationSmoke
& .\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
```

Expected: both executables exit 0; sandbox scene submits four items and skips zero.

- [ ] **Step 9: Commit P2**

```powershell
git add -- HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp HuaEngine/src/Module/Rendering/RenderSystem.h HuaEngine/src/Module/Rendering/RenderSystem.cpp HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp Tests/RenderingOperationsSmoke.cpp
git commit -m "refactor(render): centralize resource resolution"
```

---

### Task 4: P3 Forward Pipeline Pass and Render Stats

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h`
- Create: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h`
- Modify: `HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`

- [ ] **Step 1: Add failing pass/stat expectations**

In `Tests/RenderingOperationsSmoke.cpp`, after `renderLoadedScene`, add:

```cpp
Require(renderLoadedScene.Payload.contains("draw_calls"), "Expected render operation to report draw calls");
Require(renderLoadedScene.Payload.contains("pass_count"), "Expected render operation to report pass count");
Require(renderLoadedScene.Payload.at("draw_calls") == "4", "Expected loaded sandbox scene to issue four draw calls");
Require(renderLoadedScene.Payload.at("pass_count") == "1", "Expected default forward pipeline to run one pass");
```

- [ ] **Step 2: Run RED**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
```

Expected: executable fails because `draw_calls` and `pass_count` are not exposed or pass count remains zero.

- [ ] **Step 3: Define pass context**

In `RenderTypes.h`, add:

```cpp
struct RenderPassContext {
	const RenderView* View = nullptr;
	const std::vector<RenderItem>* Items = nullptr;
	const RenderResourceResolver* Resolver = nullptr;
	RenderStats* Stats = nullptr;
	std::vector<RenderDiagnostic>* Diagnostics = nullptr;
};
```

Forward declare `RenderResourceResolver` before this struct:

```cpp
class RenderResourceResolver;
```

- [ ] **Step 4: Add forward pipeline**

Create `ForwardRenderPipeline.h`:

```cpp
#pragma once

#include "RenderPipeline.h"

namespace HE::Rendering {
	class ForwardOpaquePass {
	public:
		void Execute(RenderPassContext& context) const;
	};

	class ForwardRenderPipeline final : public RenderPipeline {
	public:
		RenderResult Render(const RenderView& view, const std::vector<RenderItem>& items, const RenderResourceResolver& resolver) override;

	private:
		ForwardOpaquePass m_OpaquePass;
	};
}
```

Create `ForwardRenderPipeline.cpp`:

```cpp
#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/Renderer.h"

namespace HE::Rendering {
	void ForwardOpaquePass::Execute(RenderPassContext& context) const {
		++context.Stats->PassCount;

		for (const auto& item : *context.Items) {
			ResolvedRenderItem resolved;
			if (context.Resolver->Resolve(item, resolved, *context.Diagnostics)) {
				Renderer::Submit(resolved.MaterialInstanceRef, resolved.VertexArrayRef, item.Transform);
				++context.Stats->SubmittedItems;
				++context.Stats->DrawCalls;
			}
			else {
				++context.Stats->SkippedItems;
			}
		}
	}

	RenderResult ForwardRenderPipeline::Render(const RenderView& view, const std::vector<RenderItem>& items, const RenderResourceResolver& resolver) {
		RenderResult result;
		result.Stats.RenderItems = static_cast<uint32_t>(items.size());
		result.Stats.VisibleItems = result.Stats.RenderItems;

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		view.Target->Bind();
		if (view.ClearColorBuffer) {
			RenderCommand::SetClearColor(view.ClearColor);
			RenderCommand::Clear();
		}

		Renderer::Begin(view.CameraRef);
		RenderPassContext context;
		context.View = &view;
		context.Items = &items;
		context.Resolver = &resolver;
		context.Stats = &result.Stats;
		context.Diagnostics = &result.Diagnostics;
		m_OpaquePass.Execute(context);
		Renderer::End();
		view.Target->Unbind();

		result.Succeeded = true;
		return result;
	}
}
```

- [ ] **Step 5: Make base pipeline abstract fallback**

In `RenderPipeline.h`, keep the virtual method but make it pure:

```cpp
virtual RenderResult Render(const RenderView& view, const std::vector<RenderItem>& items, const RenderResourceResolver& resolver) = 0;
```

In `RenderPipeline.cpp`, keep only:

```cpp
#include "enginepch.h"
#include "RenderPipeline.h"

namespace HE::Rendering {
	RenderPipeline::~RenderPipeline() = default;
}
```

- [ ] **Step 6: Use ForwardRenderPipeline in RenderSystem**

In `RenderSystem.h`, include `ForwardRenderPipeline.h`.

Initialize:

```cpp
RenderSystem(Ref<Scene> scene)
	: m_Scene(scene), m_Pipeline(CreateScope<Rendering::ForwardRenderPipeline>()) {}
```

- [ ] **Step 7: Expose P3 stats**

In `ApplicationOperations::RenderSceneViewport`, add:

```cpp
result.SetPayloadValue("draw_calls", std::to_string(renderResult.Stats.DrawCalls));
result.SetPayloadValue("pass_count", std::to_string(renderResult.Stats.PassCount));
result.SetPayloadValue("visible_items", std::to_string(renderResult.Stats.VisibleItems));
```

- [ ] **Step 8: Run P3 verification**

Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
cmake --build build --config Debug --target MaterialSerializationSmoke
& .\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
cmake --build build --config Debug --target Editor
```

Expected: both smoke executables exit 0, and Editor target builds successfully.

- [ ] **Step 9: Commit P3**

```powershell
git add -- HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderTypes.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.cpp HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.h HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderPipeline.cpp HuaEngine/src/Module/Rendering/RenderSystem.h HuaEngine/src/Module/Rendering/RenderSystem.cpp HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp Tests/RenderingOperationsSmoke.cpp
git commit -m "refactor(render): add forward pipeline stats"
```

---

## Final Verification

- [ ] Run:

```powershell
cmake --build build --config Debug --target RenderingOperationsSmoke
& .\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
cmake --build build --config Debug --target MaterialSerializationSmoke
& .\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
cmake --build build --config Debug --target Editor
```

- [ ] If scene/resource behavior changed unexpectedly, also run:

```powershell
& .\build\bin\Debug-Windows-x64\smoke\SceneServiceSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
```

- [ ] Confirm `git status --short` only contains expected user-owned untracked files such as `.workspace/render-refactor-handoff.md`.
