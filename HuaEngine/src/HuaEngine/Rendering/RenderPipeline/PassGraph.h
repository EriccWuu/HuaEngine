#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphResource.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"

namespace HE::Rendering {
	enum class PassGraphDiagnosticCode {
		EmptyGraph,
		EmptyPassName,
		DuplicatePassName,
		MissingExecuteCallback,
		EmptyResourceName,
		InvalidResourceDescription,
		DuplicateResourceAccess,
		MissingResourceProducer,
		DuplicateResourceWriter
	};

	struct PassGraphDiagnostic {
		PassGraphDiagnosticCode Code;
		std::string PassName;
		std::string Message;
	};

	struct PassGraphPassDesc {
		std::string Name;
		std::vector<std::string> Inputs;
		std::vector<std::string> Outputs;
		std::function<void(RenderPassContext&)> Execute;
	};

	struct PassGraphStats {
		std::uint32_t ResourceCount = 0;
		std::uint32_t EdgeCount = 0;
		std::uint32_t ExternalInputCount = 0;
		std::uint32_t OutputCount = 0;
		std::uint32_t ImportedResourceCount = 0;
		std::uint32_t TransientResourceCount = 0;
	};

	struct PassGraphResourceBarrier {
		std::string PassName;
		std::string ResourceName;
		uint32_t PassIndex = 0;
		ResourceState Before = ResourceState::Undefined;
		ResourceState After = ResourceState::Undefined;
	};

	using PassGraphBarrierExecutor = std::function<void(const PassGraphResourceBarrier&, RenderPassContext&)>;

	class PassGraph {
	public:
		void AddPass(PassGraphPassDesc pass);
		void AddExternalInput(std::string resourceName);
		void SetBarrierExecutor(PassGraphBarrierExecutor executor);
		RenderGraphResourceHandle AddImportedResource(RenderGraphResourceDesc desc);
		RenderGraphResourceHandle AddTransientResource(RenderGraphResourceDesc desc);
		[[nodiscard]] bool Compile();
		[[nodiscard]] bool Execute(RenderPassContext& context);
		void Reset();

		[[nodiscard]] const std::vector<PassGraphPassDesc>& GetPasses() const { return m_Passes; }
		[[nodiscard]] const std::vector<PassGraphDiagnostic>& GetDiagnostics() const { return m_Diagnostics; }
		[[nodiscard]] bool IsCompiled() const { return m_Compiled; }
		[[nodiscard]] const PassGraphStats& GetStats() const { return m_Stats; }
		[[nodiscard]] const std::vector<std::string>& GetExternalInputs() const { return m_ExternalInputs; }
		[[nodiscard]] const RenderGraphResourceAllocator& GetResourceAllocator() const { return m_ResourceAllocator; }
		[[nodiscard]] const std::vector<PassGraphResourceBarrier>& GetBarrierPlan() const { return m_BarrierPlan; }

	private:
		std::vector<PassGraphPassDesc> m_Passes;
		std::vector<std::string> m_ExternalInputs;
		RenderGraphResourceAllocator m_ResourceAllocator;
		std::vector<PassGraphDiagnostic> m_Diagnostics;
		std::vector<PassGraphResourceBarrier> m_BarrierPlan;
		PassGraphBarrierExecutor m_BarrierExecutor;
		PassGraphStats m_Stats;
		bool m_Compiled = false;
	};
}
