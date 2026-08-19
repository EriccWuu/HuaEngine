#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"

namespace {
	using namespace HE::Rendering;
	using Access = PassGraphResourceUsage::Access;

	void Require(bool condition, const std::string& message) {
		if (!condition) { std::cerr << "[RenderPassGraphSmoke] " << message << std::endl; std::exit(1); }
	}

	bool HasDiagnostic(const std::vector<PassGraphDiagnostic>& diagnostics, PassGraphDiagnosticCode code) {
		for (const auto& diagnostic : diagnostics) if (diagnostic.Code == code) return true;
		return false;
	}

	RenderGraphResourceHandle AddTexture(PassGraph& graph, const char* name, RenderGraphResourceStorage storage = RenderGraphResourceStorage::Transient) {
		RenderGraphResourceDesc desc;
		desc.Name = name;
		desc.Kind = RenderGraphResourceKind::Texture;
		desc.Texture = { .Width = 64, .Height = 64, .Format = RenderTargetTextureFormat::RGBA8 };
		return storage == RenderGraphResourceStorage::Imported ? graph.AddImportedResource(std::move(desc)) : graph.AddTransientResource(std::move(desc));
	}
}

int main() {
	PassGraph emptyGraph;
	Require(!emptyGraph.Compile() && HasDiagnostic(emptyGraph.GetDiagnostics(), PassGraphDiagnosticCode::EmptyGraph), "Expected empty graph diagnostic");

	PassGraph duplicateGraph;
	duplicateGraph.AddPass({ .Name = "Forward", .Execute = [](RenderPassContext&) {} });
	duplicateGraph.AddPass({ .Name = "Forward", .Execute = [](RenderPassContext&) {} });
	Require(!duplicateGraph.Compile() && HasDiagnostic(duplicateGraph.GetDiagnostics(), PassGraphDiagnosticCode::DuplicatePassName), "Expected duplicate pass diagnostic");

	PassGraph invalidUsageGraph;
	const auto invalidUsage = AddTexture(invalidUsageGraph, "InvalidUsage");
	invalidUsageGraph.AddPass({ .Name = "Invalid", .ResourceUsages = { { invalidUsage, Access::Read, ResourceState::Undefined } }, .Execute = [](RenderPassContext&) {} });
	Require(!invalidUsageGraph.Compile() && HasDiagnostic(invalidUsageGraph.GetDiagnostics(), PassGraphDiagnosticCode::InvalidResourceUsage), "Expected invalid usage diagnostic");

	PassGraph explicitGraph;
	const auto color = AddTexture(explicitGraph, "Color", RenderGraphResourceStorage::Imported);
	explicitGraph.AddPass({ .Name = "Write", .ResourceUsages = { { color, Access::Write, ResourceState::RenderTarget } }, .Execute = [](RenderPassContext&) {} });
	explicitGraph.AddPass({ .Name = "Read", .ResourceUsages = { { color, Access::Read, ResourceState::ShaderRead } }, .Execute = [](RenderPassContext&) {} });
	Require(explicitGraph.Compile(), "Expected typed write/read graph to compile");
	const auto& barriers = explicitGraph.GetBarrierPlan();
	Require(barriers.size() == 2 && barriers[0].After == ResourceState::RenderTarget && barriers[1].Before == ResourceState::RenderTarget && barriers[1].After == ResourceState::ShaderRead, "Expected typed barrier sequence");

	PassGraph futureGraph;
	const auto futureColor = AddTexture(futureGraph, "FutureColor");
	std::vector<std::string> execution;
	futureGraph.AddPass({ .Name = "Reader", .ResourceUsages = { { futureColor, Access::Read, ResourceState::ShaderRead } }, .Execute = [&](RenderPassContext&) { execution.push_back("reader"); } });
	futureGraph.AddPass({ .Name = "Writer", .ResourceUsages = { { futureColor, Access::Write, ResourceState::RenderTarget } }, .Execute = [&](RenderPassContext&) { execution.push_back("writer"); } });
	Require(futureGraph.Compile() && futureGraph.GetExecutionOrder() == std::vector<uint32_t>{ 1, 0 }, "Expected future writer to execute first");
	RenderPassContext emptyContext;
	Require(futureGraph.Execute(emptyContext) && execution == std::vector<std::string>{ "writer", "reader" }, "Expected typed future producer execution");

	PassGraph cycleGraph;
	const auto resourceA = AddTexture(cycleGraph, "A");
	const auto resourceB = AddTexture(cycleGraph, "B");
	cycleGraph.AddPass({ .Name = "A", .ResourceUsages = { { resourceA, Access::Write, ResourceState::RenderTarget }, { resourceB, Access::Read, ResourceState::ShaderRead } }, .Execute = [](RenderPassContext&) {} });
	cycleGraph.AddPass({ .Name = "B", .ResourceUsages = { { resourceB, Access::Write, ResourceState::RenderTarget }, { resourceA, Access::Read, ResourceState::ShaderRead } }, .Execute = [](RenderPassContext&) {} });
	Require(!cycleGraph.Compile() && HasDiagnostic(cycleGraph.GetDiagnostics(), PassGraphDiagnosticCode::CyclicDependency), "Expected typed cycle diagnostic");

	PassGraph queueGraph;
	const auto sceneColor = AddTexture(queueGraph, "SceneColor");
	const auto luminance = AddTexture(queueGraph, "Luminance");
	queueGraph.AddPass({ .Name = "Graphics", .ResourceUsages = { { sceneColor, Access::Write, ResourceState::RenderTarget } }, .Execute = [](RenderPassContext&) {} });
	queueGraph.AddPass({ .Name = "Compute", .Type = PassGraphPassType::Compute, .ResourceUsages = { { sceneColor, Access::Read, ResourceState::ShaderRead }, { luminance, Access::Write, ResourceState::CopyDst } }, .Execute = [](RenderPassContext&) {} });
	queueGraph.AddPass({ .Name = "Copy", .Type = PassGraphPassType::Copy, .ResourceUsages = { { luminance, Access::Read, ResourceState::CopySrc } }, .Execute = [](RenderPassContext&) {} });
	Require(queueGraph.Compile(), "Expected typed queue graph to compile");
	const auto& batches = queueGraph.GetQueueBatches();
	Require(batches.size() == 3 && batches[1].WaitBatchIndices == std::vector<uint32_t>{ 0 } && batches[2].WaitBatchIndices == std::vector<uint32_t>{ 1 }, "Expected cross-queue waits");

	PassGraph dependencyGraph;
	std::vector<std::string> dependencyExecution;
	const auto first = dependencyGraph.AddPass({ .Name = "First", .Execute = [&](RenderPassContext&) { dependencyExecution.push_back("first"); } });
	dependencyGraph.AddPass({ .Name = "Second", .Dependencies = { first }, .Execute = [&](RenderPassContext&) { dependencyExecution.push_back("second"); } });
	Require(dependencyGraph.Compile(), "Expected explicit dependency graph to compile");
	Require(dependencyGraph.Execute(emptyContext) && dependencyExecution == std::vector<std::string>{ "first", "second" }, "Expected explicit dependency execution");

	std::vector<std::string> culledExecution;
	PassGraph cullingGraph;
	const auto finalColor = AddTexture(cullingGraph, "Final");
	const auto unusedColor = AddTexture(cullingGraph, "Unused");
	cullingGraph.AddPass({ .Name = "Final", .ResourceUsages = { { finalColor, Access::Write, ResourceState::RenderTarget } }, .Execute = [&](RenderPassContext&) { culledExecution.push_back("final"); } });
	cullingGraph.AddPass({ .Name = "Unused", .ResourceUsages = { { unusedColor, Access::Write, ResourceState::RenderTarget } }, .Execute = [&](RenderPassContext&) { culledExecution.push_back("unused"); } });
	cullingGraph.AddOutputResource(finalColor);
	Require(cullingGraph.Compile() && cullingGraph.GetStats().CulledPassCount == 1, "Expected typed culling");
	Require(cullingGraph.Execute(emptyContext) && culledExecution == std::vector<std::string>{ "final" }, "Expected unused pass to be culled");

	std::cout << "RenderPassGraphSmoke passed" << std::endl;
	return 0;
}
