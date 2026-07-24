#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/PipelineState.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/VertexInputBinding.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	class CommandList;
	class RenderDevice;
	class RenderGraphResourceAllocator;
	class RenderResourceResolver;
	class ResourceStateTracker;
	class CommandBuffer;
	struct RenderPassDesc;

	struct RenderView {
		Ref<Camera> CameraRef;
		Ref<RenderTarget> Target;
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
		MissingShader,
		MissingPipelineState,
		FallbackResourceUsed,
		MissingRhiDrawResources
	};

	struct RenderDiagnostic {
		RenderDiagnosticCode Code;
		Entity SourceEntity;
		std::string Message;
	};

	enum class RenderGraphDiagnosticCode {
		EmptyGraph,
		EmptyPassName,
		DuplicatePassName,
		MissingExecuteCallback,
		EmptyResourceName,
		InvalidResourceDescription,
		InvalidResourceHandle,
		InvalidResourceUsage,
		InvalidPassType,
		DuplicateResourceAccess,
		MissingResourceProducer,
		DuplicateResourceWriter
	};

	struct RenderGraphDiagnostic {
		RenderGraphDiagnosticCode Code;
		std::string PassName;
		std::string Message;
	};

	struct RenderGraphStats {
		uint32_t ResourceCount = 0;
		uint32_t EdgeCount = 0;
		uint32_t ExternalInputCount = 0;
		uint32_t OutputCount = 0;
	};

	struct ResolvedRenderItem {
		const RenderItem* Source = nullptr;
		Ref<VertexBufferView> VertexBufferViewRef;
		VertexBufferBinding VertexBinding;
		IndexBufferBinding IndexBinding;
		Ref<PipelineState> PipelineStateRef;
		Ref<ShaderProgram> ShaderProgramRef;
		Ref<MaterialInstance> MaterialInstanceRef;
		Ref<BindGroup> MaterialBindGroupRef;
	};

	struct RenderStats {
		uint32_t RenderItems = 0;
		uint32_t SubmittedItems = 0;
		uint32_t SkippedItems = 0;
		uint32_t DrawCalls = 0;
		uint32_t VisibleItems = 0;
		uint32_t PassCount = 0;
		uint32_t FallbackItems = 0;
		uint32_t BindGroupLayoutCacheHits = 0;
		uint32_t BindGroupLayoutCacheMisses = 0;
		uint32_t PipelineStateCacheHits = 0;
		uint32_t PipelineStateCacheMisses = 0;
		uint64_t GraphicsQueueSignalValue = 0;
		uint64_t GraphicsQueueCompletedValue = 0;
		uint32_t FramesInFlight = 0;
	};

	struct RenderPassContext {
		const RenderView* View = nullptr;
		const std::vector<RenderItem>* RenderItems = nullptr;
		const RenderResourceResolver* ResourceResolver = nullptr;
		CommandList* Commands = nullptr;
		CommandBuffer* RecordingCommandBuffer = nullptr;
		uint64_t CompletedGraphicsFenceValue = std::numeric_limits<uint64_t>::max();
		RenderDevice* Device = nullptr;
		ResourceStateTracker* ResourceStates = nullptr;
		const RenderGraphResourceAllocator* GraphResources = nullptr;
		const RenderPassDesc* GraphRenderPass = nullptr;
		RenderStats* Stats = nullptr;
		std::vector<RenderDiagnostic>* Diagnostics = nullptr;
	};

	struct RenderResult {
		bool Succeeded = false;
		RenderStats Stats;
		RenderGraphStats GraphStats;
		std::vector<RenderDiagnostic> Diagnostics;
		std::vector<RenderGraphDiagnostic> GraphDiagnostics;
	};
}
