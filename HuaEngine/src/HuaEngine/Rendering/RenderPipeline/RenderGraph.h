#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphResource.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/RenderPass.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"

namespace HE::Rendering {
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
		DuplicateResourceWriter,
		CyclicDependency
	};

	struct RenderGraphDiagnostic {
		RenderGraphDiagnosticCode Code;
		std::string PassName;
		std::string Message;
	};

	struct RenderGraphResourceUsage {
		RenderGraphResourceHandle Resource;
		enum class Access : uint8_t {
			Read = 0,
			Write
		};
		Access AccessMode = Access::Read;
		ResourceState State = ResourceState::Undefined;
	};

	struct RenderGraphPassHandle {
		uint32_t Index = std::numeric_limits<uint32_t>::max();

		[[nodiscard]] bool IsValid() const {
			return Index != std::numeric_limits<uint32_t>::max();
		}
	};

	enum class RenderGraphPassType : uint8_t {
		Graphics = 0,
		Compute,
		Copy
	};

	enum class RenderGraphRenderPassAttachmentKind : uint8_t {
		Color = 0,
		DepthStencil
	};

	struct RenderGraphRenderPassAttachment {
		RenderGraphResourceHandle Resource;
		RenderGraphRenderPassAttachmentKind Kind = RenderGraphRenderPassAttachmentKind::Color;
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		glm::vec4 ClearColor = glm::vec4(0.0f);
		float ClearDepth = 1.0f;
		uint32_t ClearStencil = 0;
	};

	struct RenderGraphPassDesc {
		std::string Name;
		RenderGraphPassType Type = RenderGraphPassType::Graphics;
		bool HasSideEffects = false;
		std::vector<RenderGraphPassHandle> Dependencies;
		std::vector<RenderGraphResourceUsage> ResourceUsages;
		std::vector<RenderGraphRenderPassAttachment> RenderPassAttachments;
		std::function<void(RenderPassContext&)> Execute;
	};

	struct RenderGraphStats {
		std::uint32_t ResourceCount = 0;
		std::uint32_t EdgeCount = 0;
		std::uint32_t OutputCount = 0;
		std::uint32_t ImportedResourceCount = 0;
		std::uint32_t TransientResourceCount = 0;
		std::uint32_t CulledPassCount = 0;
	};

	struct RenderGraphResourceBarrier {
		std::string PassName;
		std::string ResourceName;
		uint32_t PassIndex = 0;
		ResourceState Before = ResourceState::Undefined;
		ResourceState After = ResourceState::Undefined;
	};

	struct RenderGraphQueueBatch {
		RenderQueueType Queue = RenderQueueType::Graphics;
		std::vector<uint32_t> PassIndices;
		std::vector<uint32_t> WaitBatchIndices;
	};

	using RenderGraphBarrierExecutor = std::function<void(const RenderGraphResourceBarrier&, RenderPassContext&)>;

	class RenderGraphBuilder;

	class RenderGraph {
	public:
		void SetBarrierExecutor(RenderGraphBarrierExecutor executor);
		[[nodiscard]] bool Compile();
		[[nodiscard]] bool Execute(RenderPassContext& context);
		void ReleaseTransientResources(uint64_t fenceValue);

		[[nodiscard]] const std::vector<RenderGraphDiagnostic>& GetDiagnostics() const { return m_Diagnostics; }
		[[nodiscard]] bool IsCompiled() const { return m_Compiled; }
		[[nodiscard]] const RenderGraphStats& GetStats() const { return m_Stats; }
		[[nodiscard]] const RenderGraphResourceAllocator& GetResourceAllocator() const { return m_ResourceAllocator; }
		[[nodiscard]] const std::vector<RenderGraphResourceBarrier>& GetBarrierPlan() const { return m_BarrierPlan; }
		[[nodiscard]] const std::vector<uint32_t>& GetExecutionOrder() const { return m_ExecutionOrder; }
		[[nodiscard]] const std::vector<RenderGraphQueueBatch>& GetQueueBatches() const { return m_QueueBatches; }

	private:
		friend class RenderGraphBuilder;

		RenderGraphPassHandle AddPass(RenderGraphPassDesc pass);
		void AddOutputResource(RenderGraphResourceHandle resource);
		RenderGraphResourceHandle AddImportedResource(RenderGraphResourceDesc desc);
		RenderGraphResourceHandle AddTransientResource(RenderGraphResourceDesc desc);
		void Reset();

		std::vector<RenderGraphPassDesc> m_Passes;
		std::vector<RenderGraphResourceHandle> m_DeclaredOutputs;
		RenderGraphResourceAllocator m_ResourceAllocator;
		std::vector<RenderGraphDiagnostic> m_Diagnostics;
		std::vector<RenderGraphResourceBarrier> m_BarrierPlan;
		std::vector<uint32_t> m_ExecutionOrder;
		std::vector<RenderGraphQueueBatch> m_QueueBatches;
		RenderGraphBarrierExecutor m_BarrierExecutor;
		RenderGraphStats m_Stats;
		bool m_Compiled = false;
	};
}
