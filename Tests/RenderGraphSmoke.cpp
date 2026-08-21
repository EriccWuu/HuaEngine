#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"

namespace {
	using namespace HE::Rendering;

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RenderGraphSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasDiagnostic(const std::vector<RenderGraphDiagnostic>& diagnostics, RenderGraphDiagnosticCode code) {
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

		[[nodiscard]] const char* GetName() const override { return "ObjectPass"; }
		[[nodiscard]] RenderGraphPassType GetType() const override { return RenderGraphPassType::Graphics; }

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
	RenderGraph emptyGraph;
	Require(!emptyGraph.Compile() && HasDiagnostic(emptyGraph.GetDiagnostics(), RenderGraphDiagnosticCode::EmptyGraph), "Expected empty graph diagnostic");

	RenderGraph duplicateGraph;
	RenderGraphBuilder duplicateBuilder(duplicateGraph);
	duplicateBuilder.AddPass("Forward", RenderGraphPassType::Graphics, [](RenderGraphPassBuilder& pass) { pass.SetExecute([](RenderPassContext&) {}); });
	duplicateBuilder.AddPass("Forward", RenderGraphPassType::Graphics, [](RenderGraphPassBuilder& pass) { pass.SetExecute([](RenderPassContext&) {}); });
	Require(!duplicateGraph.Compile() && HasDiagnostic(duplicateGraph.GetDiagnostics(), RenderGraphDiagnosticCode::DuplicatePassName), "Expected duplicate pass diagnostic");

	RenderGraph invalidUsageGraph;
	RenderGraphBuilder invalidUsageBuilder(invalidUsageGraph);
	const auto invalidUsage = CreateTexture(invalidUsageBuilder, "InvalidUsage");
	invalidUsageBuilder.AddPass("Invalid", RenderGraphPassType::Graphics, [invalidUsage](RenderGraphPassBuilder& pass) {
		pass.Read(invalidUsage, ResourceState::Undefined);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(!invalidUsageGraph.Compile() && HasDiagnostic(invalidUsageGraph.GetDiagnostics(), RenderGraphDiagnosticCode::InvalidResourceUsage), "Expected invalid usage diagnostic");

	RenderGraph explicitGraph;
	RenderGraphBuilder explicitBuilder(explicitGraph);
	const auto color = CreateTexture(explicitBuilder, "Color");
	explicitBuilder.AddPass("Write", RenderGraphPassType::Graphics, [color](RenderGraphPassBuilder& pass) {
		pass.Write(color, ResourceState::RenderTarget);
		pass.SetExecute([](RenderPassContext&) {});
	});
	explicitBuilder.AddPass("Read", RenderGraphPassType::Graphics, [color](RenderGraphPassBuilder& pass) {
		pass.Read(color, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(explicitGraph.Compile(), "Expected typed write/read graph to compile");
	const auto& barriers = explicitGraph.GetBarrierPlan();
	Require(barriers.size() == 2 && barriers[0].After == ResourceState::RenderTarget && barriers[1].Before == ResourceState::RenderTarget && barriers[1].After == ResourceState::ShaderRead, "Expected typed barrier sequence");

	RenderGraph futureGraph;
	RenderGraphBuilder futureBuilder(futureGraph);
	const auto futureColor = CreateTexture(futureBuilder, "FutureColor");
	std::vector<std::string> execution;
	futureBuilder.AddPass("Reader", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.Read(futureColor, ResourceState::ShaderRead);
		pass.SetExecute([&](RenderPassContext&) { execution.push_back("reader"); });
	});
	futureBuilder.AddPass("Writer", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.Write(futureColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { execution.push_back("writer"); });
	});
	Require(futureGraph.Compile() && futureGraph.GetExecutionOrder() == std::vector<uint32_t>{ 1, 0 }, "Expected future writer to execute first");
	RenderPassContext emptyContext;
	Require(futureGraph.Execute(emptyContext) && execution == std::vector<std::string>{ "writer", "reader" }, "Expected typed future producer execution");

	RenderGraph sequentialAttachmentGraph;
	RenderGraphBuilder sequentialAttachmentBuilder(sequentialAttachmentGraph);
	const auto sequentialColor = CreateTexture(sequentialAttachmentBuilder, "SequentialColor");
	const auto sequentialDepth = sequentialAttachmentBuilder.CreateTexture("SequentialDepth", {
		.Width = 64,
		.Height = 64,
		.Format = RenderTargetTextureFormat::DEPTH24_STENCIL8
	});
	sequentialAttachmentBuilder.AddPass("Grid", RenderGraphPassType::Graphics, [sequentialColor, sequentialDepth](RenderGraphPassBuilder& pass) {
		pass.WriteColor(sequentialColor, LoadOp::Clear);
		pass.WriteDepth(sequentialDepth, LoadOp::Clear);
		pass.SetExecute([](RenderPassContext&) {});
	});
	sequentialAttachmentBuilder.AddPass("Opaque", RenderGraphPassType::Graphics, [sequentialColor, sequentialDepth](RenderGraphPassBuilder& pass) {
		pass.WriteColor(sequentialColor, LoadOp::Load);
		pass.WriteDepth(sequentialDepth, LoadOp::Load);
		pass.SetExecute([](RenderPassContext&) {});
	});
	sequentialAttachmentBuilder.Export(sequentialColor);
	Require(sequentialAttachmentGraph.Compile(), "Expected sequential attachment writers to compile");
	Require(sequentialAttachmentGraph.GetExecutionOrder() == std::vector<uint32_t>{ 0, 1 }, "Expected attachment writers to preserve declaration order");

	RenderGraph invalidSequentialAttachmentGraph;
	RenderGraphBuilder invalidSequentialAttachmentBuilder(invalidSequentialAttachmentGraph);
	const auto invalidSequentialColor = CreateTexture(invalidSequentialAttachmentBuilder, "InvalidSequentialColor");
	invalidSequentialAttachmentBuilder.AddPass("First", RenderGraphPassType::Graphics, [invalidSequentialColor](RenderGraphPassBuilder& pass) {
		pass.WriteColor(invalidSequentialColor, LoadOp::Clear);
		pass.SetExecute([](RenderPassContext&) {});
	});
	invalidSequentialAttachmentBuilder.AddPass("Second", RenderGraphPassType::Graphics, [invalidSequentialColor](RenderGraphPassBuilder& pass) {
		pass.WriteColor(invalidSequentialColor, LoadOp::Clear);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(!invalidSequentialAttachmentGraph.Compile() && HasDiagnostic(invalidSequentialAttachmentGraph.GetDiagnostics(), RenderGraphDiagnosticCode::DuplicateResourceWriter), "Expected unordered attachment writer diagnostic");

	RenderGraph cycleGraph;
	RenderGraphBuilder cycleBuilder(cycleGraph);
	const auto resourceA = CreateTexture(cycleBuilder, "A");
	const auto resourceB = CreateTexture(cycleBuilder, "B");
	cycleBuilder.AddPass("A", RenderGraphPassType::Graphics, [resourceA, resourceB](RenderGraphPassBuilder& pass) {
		pass.Write(resourceA, ResourceState::RenderTarget);
		pass.Read(resourceB, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	cycleBuilder.AddPass("B", RenderGraphPassType::Graphics, [resourceA, resourceB](RenderGraphPassBuilder& pass) {
		pass.Write(resourceB, ResourceState::RenderTarget);
		pass.Read(resourceA, ResourceState::ShaderRead);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(!cycleGraph.Compile() && HasDiagnostic(cycleGraph.GetDiagnostics(), RenderGraphDiagnosticCode::CyclicDependency), "Expected typed cycle diagnostic");

	RenderGraph queueGraph;
	RenderGraphBuilder queueBuilder(queueGraph);
	const auto sceneColor = CreateTexture(queueBuilder, "SceneColor");
	const auto luminance = CreateTexture(queueBuilder, "Luminance");
	queueBuilder.AddPass("Graphics", RenderGraphPassType::Graphics, [sceneColor](RenderGraphPassBuilder& pass) {
		pass.Write(sceneColor, ResourceState::RenderTarget);
		pass.SetExecute([](RenderPassContext&) {});
	});
	queueBuilder.AddPass("Compute", RenderGraphPassType::Compute, [sceneColor, luminance](RenderGraphPassBuilder& pass) {
		pass.Read(sceneColor, ResourceState::ShaderRead);
		pass.Write(luminance, ResourceState::CopyDst);
		pass.SetExecute([](RenderPassContext&) {});
	});
	queueBuilder.AddPass("Copy", RenderGraphPassType::Copy, [luminance](RenderGraphPassBuilder& pass) {
		pass.Read(luminance, ResourceState::CopySrc);
		pass.SetExecute([](RenderPassContext&) {});
	});
	Require(queueGraph.Compile(), "Expected typed queue graph to compile");
	const auto& batches = queueGraph.GetQueueBatches();
	Require(batches.size() == 3 && batches[1].WaitBatchIndices == std::vector<uint32_t>{ 0 } && batches[2].WaitBatchIndices == std::vector<uint32_t>{ 1 }, "Expected cross-queue waits");

	RenderGraph dependencyGraph;
	RenderGraphBuilder dependencyBuilder(dependencyGraph);
	std::vector<std::string> dependencyExecution;
	const auto first = dependencyBuilder.AddPass("First", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.SetExecute([&](RenderPassContext&) { dependencyExecution.push_back("first"); });
	});
	dependencyBuilder.AddPass("Second", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.DependsOn(first);
		pass.SetExecute([&](RenderPassContext&) { dependencyExecution.push_back("second"); });
	});
	Require(dependencyGraph.Compile(), "Expected explicit dependency graph to compile");
	Require(dependencyGraph.Execute(emptyContext) && dependencyExecution == std::vector<std::string>{ "first", "second" }, "Expected explicit dependency execution");

	RenderGraph objectRenderGraph;
	RenderGraphBuilder objectPassBuilder(objectRenderGraph);
	const auto objectPassColor = CreateTexture(objectPassBuilder, "ObjectPassColor");
	ObjectPass objectPass(objectPassColor);
	objectPassBuilder.AddPass(objectPass);
	objectPassBuilder.Export(objectPassColor);
	Require(objectRenderGraph.Compile(), "Expected object pass graph to compile");
	Require(objectRenderGraph.Execute(emptyContext) && objectPass.WasExecuted(), "Expected object pass setup and execute binding");

	std::vector<std::string> culledExecution;
	RenderGraph cullingGraph;
	RenderGraphBuilder cullingBuilder(cullingGraph);
	const auto finalColor = CreateTexture(cullingBuilder, "Final");
	const auto unusedColor = CreateTexture(cullingBuilder, "Unused");
	cullingBuilder.AddPass("Final", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.Write(finalColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { culledExecution.push_back("final"); });
	});
	cullingBuilder.AddPass("Unused", RenderGraphPassType::Graphics, [&](RenderGraphPassBuilder& pass) {
		pass.Write(unusedColor, ResourceState::RenderTarget);
		pass.SetExecute([&](RenderPassContext&) { culledExecution.push_back("unused"); });
	});
	cullingBuilder.Export(finalColor);
	Require(cullingGraph.Compile() && cullingGraph.GetStats().CulledPassCount == 1, "Expected typed culling");
	Require(cullingGraph.Execute(emptyContext) && culledExecution == std::vector<std::string>{ "final" }, "Expected unused pass to be culled");

	std::cout << "RenderGraphSmoke passed" << std::endl;
	return 0;
}
