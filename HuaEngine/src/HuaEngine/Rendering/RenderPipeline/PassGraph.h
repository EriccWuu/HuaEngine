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
	enum class PassGraphDiagnosticCode {
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

	struct PassGraphDiagnostic {
		PassGraphDiagnosticCode Code;
		std::string PassName;
		std::string Message;
	};

	struct PassGraphResourceUsage {
		RenderGraphResourceHandle Resource;
		enum class Access : uint8_t {
			Read = 0,
			Write
		};
		Access AccessMode = Access::Read;
		ResourceState State = ResourceState::Undefined;
	};

	struct PassGraphPassHandle {
		uint32_t Index = std::numeric_limits<uint32_t>::max();

		[[nodiscard]] bool IsValid() const {
			return Index != std::numeric_limits<uint32_t>::max();
		}
	};

	enum class PassGraphPassType : uint8_t {
		Graphics = 0,
		Compute,
		Copy
	};

	enum class PassGraphRenderPassAttachmentKind : uint8_t {
		Color = 0,
		DepthStencil
	};

	struct PassGraphRenderPassAttachment {
		RenderGraphResourceHandle Resource;
		PassGraphRenderPassAttachmentKind Kind = PassGraphRenderPassAttachmentKind::Color;
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		glm::vec4 ClearColor = glm::vec4(0.0f);
		float ClearDepth = 1.0f;
		uint32_t ClearStencil = 0;
	};

	struct PassGraphPassDesc {
		std::string Name;
		PassGraphPassType Type = PassGraphPassType::Graphics;
		bool HasSideEffects = false;
		std::vector<PassGraphPassHandle> Dependencies;
		std::vector<PassGraphResourceUsage> ResourceUsages;
		std::vector<PassGraphRenderPassAttachment> RenderPassAttachments;
		std::function<void(RenderPassContext&)> Execute;
	};

	struct PassGraphStats {
		std::uint32_t ResourceCount = 0;
		std::uint32_t EdgeCount = 0;
		std::uint32_t OutputCount = 0;
		std::uint32_t ImportedResourceCount = 0;
		std::uint32_t TransientResourceCount = 0;
		std::uint32_t CulledPassCount = 0;
	};

	struct PassGraphResourceBarrier {
		std::string PassName;
		std::string ResourceName;
		uint32_t PassIndex = 0;
		ResourceState Before = ResourceState::Undefined;
		ResourceState After = ResourceState::Undefined;
	};

	struct PassGraphQueueBatch {
		RenderQueueType Queue = RenderQueueType::Graphics;
		std::vector<uint32_t> PassIndices;
		std::vector<uint32_t> WaitBatchIndices;
	};

	using PassGraphBarrierExecutor = std::function<void(const PassGraphResourceBarrier&, RenderPassContext&)>;

	class RenderGraphBuilder;

	class PassGraph {
	public:
		void SetBarrierExecutor(PassGraphBarrierExecutor executor);
		[[nodiscard]] bool Compile();
		[[nodiscard]] bool Execute(RenderPassContext& context);
		void ReleaseTransientResources(uint64_t fenceValue);

		[[nodiscard]] const std::vector<PassGraphDiagnostic>& GetDiagnostics() const { return m_Diagnostics; }
		[[nodiscard]] bool IsCompiled() const { return m_Compiled; }
		[[nodiscard]] const PassGraphStats& GetStats() const { return m_Stats; }
		[[nodiscard]] const RenderGraphResourceAllocator& GetResourceAllocator() const { return m_ResourceAllocator; }
		[[nodiscard]] const std::vector<PassGraphResourceBarrier>& GetBarrierPlan() const { return m_BarrierPlan; }
		[[nodiscard]] const std::vector<uint32_t>& GetExecutionOrder() const { return m_ExecutionOrder; }
		[[nodiscard]] const std::vector<PassGraphQueueBatch>& GetQueueBatches() const { return m_QueueBatches; }

	private:
		friend class RenderGraphBuilder;

		PassGraphPassHandle AddPass(PassGraphPassDesc pass);
		void AddOutputResource(RenderGraphResourceHandle resource);
		RenderGraphResourceHandle AddImportedResource(RenderGraphResourceDesc desc);
		RenderGraphResourceHandle AddTransientResource(RenderGraphResourceDesc desc);
		void Reset();

		std::vector<PassGraphPassDesc> m_Passes;
		std::vector<RenderGraphResourceHandle> m_DeclaredOutputs;
		RenderGraphResourceAllocator m_ResourceAllocator;
		std::vector<PassGraphDiagnostic> m_Diagnostics;
		std::vector<PassGraphResourceBarrier> m_BarrierPlan;
		std::vector<uint32_t> m_ExecutionOrder;
		std::vector<PassGraphQueueBatch> m_QueueBatches;
		PassGraphBarrierExecutor m_BarrierExecutor;
		PassGraphStats m_Stats;
		bool m_Compiled = false;
	};
}
