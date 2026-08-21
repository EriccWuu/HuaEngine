#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphBuilder.h"

namespace {
	using namespace HE::Rendering;

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RenderPassGraphSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasDiagnostic(const std::vector<PassGraphDiagnostic>& diagnostics, PassGraphDiagnosticCode code) {
		for (const auto& diagnostic : diagnostics) {
			if (diagnostic.Code == code) {
				return true;
			}
		}
		return false;
	}

	RenderGraphResourceHandle CreateTexture(RenderGraphBuilder& graph, const char* name) {
		return graph.CreateTexture(name, {
			.Width = 64,
			.Height = 64,
			.Format = RenderTargetTextureFormat::RGBA8
		});
	}

	class ObjectPass final : public RenderGraphPass {
	public:
		explicit ObjectPass(RenderGraphResourceHandle color) : m_Color(color) {}

		void Setup(RenderGraphPassBuilder& builder) override {
			builder.Write(m_Color, ResourceState::RenderTarget);
		}

		void Execute(RenderPassContext&) override {
			m_Executed = true;
		}

		[[nodiscard]] bool WasExecuted() const { return m_Executed; }

	private:
		RenderGraphResourceHandle m_Color;
		bool m_Executed = false;
	};
}

int main() {
	PassGraph emptyGraph;
	Require(!emptyGraph.Compile() && HasDiagnostic(emptyGraph.GetDiagnostics(), PassGraphDiagnosticCode::EmptyGraph), "Expected empty graph diagnostic");

	PassGraph duplicateGraph;
	RenderGraphBuilder duplicateBuilder(duplicateGraph);
	duplicateBuilder.AddGraphicsPass("Forward", [](RenderGraphPassBuilder& pass) { pass.SetExecute([](RenderPassContext&) {}); });
	duplicateBuilder.AddGraphicsPass("Forward", [](RenderGraphPassBuilder& pass) { pass.SetExecute([](RenderPassContext&) {}); });
	Require(!duplicateGraph.Compile() && HasDiagnostic(duplicateGraph.GetDiagnostics(), PassGraphDiagnosticCode::DuplicatePassName), "Expected duplicate pass diagnostic");

	PassGraph invalidUsageGraph;
	RenderGraphBuilder invalidUsageBuilder(invalidUsageGraph);
	const auto invalidUsage = CreateTexture(invalidUsageBuilder, "InvalidUsage");
	invalidUsageBuilder.AddGraphicsPass("Invalid", [invalidUsage](RenderGraphPassBuilder& pass) {
		pass.Read(invalidUsage, ResourceState::Undefined);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(!invalidUsageGraph.Compile() && HasDiagnostic(invalidUsageGraph.GetDiagnostics(), PassGraphDiagnosticCode::InvalidResourceUsage), "Expected invalid usage diagnostic");

	PassGraph explicitGraph;
	RenderGraphBuilder explicitBuilder(explicitGraph);
	const auto color = CreateTexture(explicitBuilder, "Color");
	explicitBuilder.AddGraphicsPass("Write", [color](RenderGraphPassBuilder& pass) {
		pass.Write(color, ResourceState::RenderTarget);
		pass.SetExecute([](RenderPassContext&) {});
	});
	explicitBuilder.AddGraphicsPass("Read", [color](RenderGraphPassBuilder& pass) {
		pass.Read(color, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(explicitGraph.Compile(), "Expected typed write/read graph to compile");
	const auto& barriers = explicitGraph.GetBarrierPlan();
	Require(barriers.size() == 2 && barriers[0].After == ResourceState::RenderTarget && barriers[1].Before == ResourceState::RenderTarget && barriers[1].After == ResourceState::ShaderRead, "Expected typed barrier sequence");

	PassGraph futureGraph;
	RenderGraphBuilder futureBuilder(futureGraph);
	const auto futureColor = CreateTexture(futureBuilder, "FutureColor");
	std::vector<std::string> execution;
	futureBuilder.AddGraphicsPass("Reader", [&](RenderGraphPassBuilder& pass) {
		pass.Read(futureColor, ResourceState::ShaderRead);
		pass.SetExecute([&](RenderPassContext&) { execution.push_back("reader"); });
	});
	futureBuilder.AddGraphicsPass("Writer", [&](RenderGraphPassBuilder& pass) {
		pass.Write(futureColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { execution.push_back("writer"); });
	});
	Require(futureGraph.Compile() && futureGraph.GetExecutionOrder() == std::vector<uint32_t>{ 1, 0 }, "Expected future writer to execute first");
	RenderPassContext emptyContext;
	Require(futureGraph.Execute(emptyContext) && execution == std::vector<std::string>{ "writer", "reader" }, "Expected typed future producer execution");

	PassGraph cycleGraph;
	RenderGraphBuilder cycleBuilder(cycleGraph);
	const auto resourceA = CreateTexture(cycleBuilder, "A");
	const auto resourceB = CreateTexture(cycleBuilder, "B");
	cycleBuilder.AddGraphicsPass("A", [resourceA, resourceB](RenderGraphPassBuilder& pass) {
		pass.Write(resourceA, ResourceState::RenderTarget);
		pass.Read(resourceB, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	cycleBuilder.AddGraphicsPass("B", [resourceA, resourceB](RenderGraphPassBuilder& pass) {
		pass.Write(resourceB, ResourceState::RenderTarget);
		pass.Read(resourceA, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(!cycleGraph.Compile() && HasDiagnostic(cycleGraph.GetDiagnostics(), PassGraphDiagnosticCode::CyclicDependency), "Expected typed cycle diagnostic");

	PassGraph queueGraph;
	RenderGraphBuilder queueBuilder(queueGraph);
	const auto sceneColor = CreateTexture(queueBuilder, "SceneColor");
	const auto luminance = CreateTexture(queueBuilder, "Luminance");
	queueBuilder.AddGraphicsPass("Graphics", [sceneColor](RenderGraphPassBuilder& pass) {
		pass.Write(sceneColor, ResourceState::RenderTarget);
		pass.SetExecute([](RenderPassContext&) {});
	});
	queueBuilder.AddComputePass("Compute", [sceneColor, luminance](RenderGraphPassBuilder& pass) {
		pass.Read(sceneColor, ResourceState::ShaderRead);
		pass.Write(luminance, ResourceState::CopyDst);
		pass.SetExecute([](RenderPassContext&) {});
	});
	queueBuilder.AddCopyPass("Copy", [luminance](RenderGraphPassBuilder& pass) {
		pass.Read(luminance, ResourceState::CopySrc);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(queueGraph.Compile(), "Expected typed queue graph to compile");
	const auto& batches = queueGraph.GetQueueBatches();
	Require(batches.size() == 3 && batches[1].WaitBatchIndices == std::vector<uint32_t>{ 0 } && batches[2].WaitBatchIndices == std::vector<uint32_t>{ 1 }, "Expected cross-queue waits");

	PassGraph dependencyGraph;
	RenderGraphBuilder dependencyBuilder(dependencyGraph);
	std::vector<std::string> dependencyExecution;
	const auto first = dependencyBuilder.AddGraphicsPass("First", [&](RenderGraphPassBuilder& pass) {
		pass.SetExecute([&](RenderPassContext&) { dependencyExecution.push_back("first"); });
	});
	dependencyBuilder.AddGraphicsPass("Second", [&](RenderGraphPassBuilder& pass) {
		pass.DependsOn(first);
		pass.SetExecute([&](RenderPassContext&) { dependencyExecution.push_back("second"); });
	});
	Require(dependencyGraph.Compile(), "Expected explicit dependency graph to compile");
	Require(dependencyGraph.Execute(emptyContext) && dependencyExecution == std::vector<std::string>{ "first", "second" }, "Expected explicit dependency execution");

	PassGraph objectPassGraph;
	RenderGraphBuilder objectPassBuilder(objectPassGraph);
	const auto objectPassColor = CreateTexture(objectPassBuilder, "ObjectPassColor");
	ObjectPass objectPass(objectPassColor);
	objectPassBuilder.AddGraphicsPass("ObjectPass", objectPass);
	objectPassBuilder.Export(objectPassColor);
	Require(objectPassGraph.Compile(), "Expected object pass graph to compile");
	Require(objectPassGraph.Execute(emptyContext) && objectPass.WasExecuted(), "Expected object pass setup and execute binding");

	std::vector<std::string> culledExecution;
	PassGraph cullingGraph;
	RenderGraphBuilder cullingBuilder(cullingGraph);
	const auto finalColor = CreateTexture(cullingBuilder, "Final");
	const auto unusedColor = CreateTexture(cullingBuilder, "Unused");
	cullingBuilder.AddGraphicsPass("Final", [&](RenderGraphPassBuilder& pass) {
		pass.Write(finalColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { culledExecution.push_back("final"); });
	});
	cullingBuilder.AddGraphicsPass("Unused", [&](RenderGraphPassBuilder& pass) {
		pass.Write(unusedColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { culledExecution.push_back("unused"); });
	});
	cullingBuilder.Export(finalColor);
	Require(cullingGraph.Compile() && cullingGraph.GetStats().CulledPassCount == 1, "Expected typed culling");
	Require(cullingGraph.Execute(emptyContext) && culledExecution == std::vector<std::string>{ "final" }, "Expected unused pass to be culled");

	std::cout << "RenderPassGraphSmoke passed" << std::endl;
	return 0;
}
