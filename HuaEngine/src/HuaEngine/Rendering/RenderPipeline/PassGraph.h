#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphResource.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

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

	struct RenderPassDesc {
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

	class PassGraph {
	public:
		void AddPass(RenderPassDesc pass);
		void AddExternalInput(std::string resourceName);
		RenderGraphResourceHandle AddImportedResource(RenderGraphResourceDesc desc);
		RenderGraphResourceHandle AddTransientResource(RenderGraphResourceDesc desc);
		[[nodiscard]] bool Compile();
		[[nodiscard]] bool Execute(RenderPassContext& context);
		void Reset();

		[[nodiscard]] const std::vector<RenderPassDesc>& GetPasses() const { return m_Passes; }
		[[nodiscard]] const std::vector<PassGraphDiagnostic>& GetDiagnostics() const { return m_Diagnostics; }
		[[nodiscard]] bool IsCompiled() const { return m_Compiled; }
		[[nodiscard]] const PassGraphStats& GetStats() const { return m_Stats; }
		[[nodiscard]] const std::vector<std::string>& GetExternalInputs() const { return m_ExternalInputs; }
		[[nodiscard]] const RenderGraphResourceAllocator& GetResourceAllocator() const { return m_ResourceAllocator; }

	private:
		std::vector<RenderPassDesc> m_Passes;
		std::vector<std::string> m_ExternalInputs;
		RenderGraphResourceAllocator m_ResourceAllocator;
		std::vector<PassGraphDiagnostic> m_Diagnostics;
		PassGraphStats m_Stats;
		bool m_Compiled = false;
	};
}
