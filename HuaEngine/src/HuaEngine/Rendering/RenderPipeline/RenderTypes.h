#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	class RenderResourceResolver;

	struct RenderView {
		Ref<Camera> CameraRef;
		Ref<FrameBuffer> Target;
		glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
		bool ClearColorBuffer = true;
	};

	struct RenderItem {
		Entity SourceEntity;
		glm::mat4 Transform = glm::mat4(1.0f);
		MeshAssetRef Mesh;
		MaterialAssetRef Material;
		MaterialOverrideSet MaterialOverrides;
	};

	enum class RenderDiagnosticCode {
		MissingMeshAsset,
		MissingVertexArray,
		MissingMaterialInstance,
		MissingBaseMaterial,
		MissingShader
	};

	struct RenderDiagnostic {
		RenderDiagnosticCode Code;
		Entity SourceEntity;
		std::string Message;
	};

	struct ResolvedRenderItem {
		const RenderItem* Source = nullptr;
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

	struct RenderPassContext {
		const RenderView* View = nullptr;
		const std::vector<RenderItem>* RenderItems = nullptr;
		const RenderResourceResolver* ResourceResolver = nullptr;
		RenderStats* Stats = nullptr;
		std::vector<RenderDiagnostic>* Diagnostics = nullptr;
	};

	struct RenderResult {
		bool Succeeded = false;
		RenderStats Stats;
		std::vector<RenderDiagnostic> Diagnostics;
	};
}
