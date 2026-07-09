#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RenderPassGraphSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasDiagnostic(
		const std::vector<HE::Rendering::PassGraphDiagnostic>& diagnostics,
		HE::Rendering::PassGraphDiagnosticCode code) {
		for (const auto& diagnostic : diagnostics) {
			if (diagnostic.Code == code) {
				return true;
			}
		}

		return false;
	}
}

int main() {
	HE::Rendering::PassGraph emptyGraph;
	Require(!emptyGraph.Compile(), "Expected empty graph compile to fail");
	Require(
		HasDiagnostic(emptyGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::EmptyGraph),
		"Expected empty graph diagnostic");

	HE::Rendering::PassGraph duplicateGraph;
	duplicateGraph.AddPass({ .Name = "ForwardOpaque", .Execute = [](HE::Rendering::RenderPassContext&) {} });
	duplicateGraph.AddPass({ .Name = "ForwardOpaque", .Execute = [](HE::Rendering::RenderPassContext&) {} });
	Require(!duplicateGraph.Compile(), "Expected duplicate pass names to fail compile");
	Require(
		HasDiagnostic(duplicateGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::DuplicatePassName),
		"Expected duplicate pass diagnostic");

	HE::Rendering::PassGraph missingCallbackGraph;
	missingCallbackGraph.AddPass({ .Name = "MissingCallback" });
	Require(!missingCallbackGraph.Compile(), "Expected missing callback to fail compile");
	Require(
		HasDiagnostic(missingCallbackGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::MissingExecuteCallback),
		"Expected missing callback diagnostic");

	HE::Rendering::PassGraph emptyResourceGraph;
	emptyResourceGraph.AddPass({
		.Name = "EmptyResource",
		.Outputs = { "" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!emptyResourceGraph.Compile(), "Expected empty resource name to fail compile");
	Require(
		HasDiagnostic(emptyResourceGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::EmptyResourceName),
		"Expected empty resource name diagnostic");

	HE::Rendering::PassGraph invalidTypedResourceGraph;
	invalidTypedResourceGraph.AddTransientResource({
		.Name = "BadSceneColor",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = 0,
			.Height = 64,
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	});
	invalidTypedResourceGraph.AddPass({
		.Name = "BadTypedResource",
		.Outputs = { "BadSceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!invalidTypedResourceGraph.Compile(), "Expected invalid typed resource description to fail compile");
	Require(
		HasDiagnostic(invalidTypedResourceGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::InvalidResourceDescription),
		"Expected invalid typed resource description diagnostic");

	HE::Rendering::PassGraph duplicateAccessGraph;
	duplicateAccessGraph.AddPass({
		.Name = "DuplicateAccess",
		.Outputs = { "SceneColor", "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!duplicateAccessGraph.Compile(), "Expected duplicate resource access to fail compile");
	Require(
		HasDiagnostic(duplicateAccessGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::DuplicateResourceAccess),
		"Expected duplicate resource access diagnostic");

	HE::Rendering::PassGraph missingProducerGraph;
	missingProducerGraph.AddPass({
		.Name = "MissingProducer",
		.Inputs = { "SceneItems" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!missingProducerGraph.Compile(), "Expected missing resource producer to fail compile");
	Require(
		HasDiagnostic(missingProducerGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::MissingResourceProducer),
		"Expected missing resource producer diagnostic");

	HE::Rendering::PassGraph duplicateWriterGraph;
	duplicateWriterGraph.AddPass({
		.Name = "SceneExtract",
		.Outputs = { "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	duplicateWriterGraph.AddPass({
		.Name = "ForwardOpaque",
		.Outputs = { "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!duplicateWriterGraph.Compile(), "Expected duplicate resource writer to fail compile");
	Require(
		HasDiagnostic(duplicateWriterGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::DuplicateResourceWriter),
		"Expected duplicate resource writer diagnostic");

	HE::Rendering::PassGraph futureProducerGraph;
	futureProducerGraph.AddPass({
		.Name = "ReadsFutureColor",
		.Inputs = { "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	futureProducerGraph.AddPass({
		.Name = "WritesFutureColor",
		.Outputs = { "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(!futureProducerGraph.Compile(), "Expected future resource producer to fail compile");
	Require(
		HasDiagnostic(futureProducerGraph.GetDiagnostics(), HE::Rendering::PassGraphDiagnosticCode::MissingResourceProducer),
		"Expected future resource producer diagnostic");

	std::vector<std::string> executionOrder;
	HE::Rendering::PassGraph graph;
	graph.AddExternalInput("CameraView");
	graph.AddPass({
		.Name = "ExtractedSceneInput",
		.Outputs = { "SceneItems" },
		.Execute = [&](HE::Rendering::RenderPassContext&) {
			executionOrder.push_back("ExtractedSceneInput");
		}
	});
	graph.AddPass({
		.Name = "ForwardOpaque",
		.Inputs = { "SceneItems", "CameraView" },
		.Outputs = { "SceneColor" },
		.Execute = [&](HE::Rendering::RenderPassContext&) {
			executionOrder.push_back("ForwardOpaque");
		}
	});

	Require(graph.Compile(), "Expected valid graph compile to succeed");
	Require(graph.IsCompiled(), "Expected graph to report compiled state");
	const auto& stats = graph.GetStats();
	Require(stats.ResourceCount == 3, "Expected three graph resources");
	Require(stats.EdgeCount == 3, "Expected three graph edges");
	Require(stats.ExternalInputCount == 1, "Expected one external graph input");
	Require(stats.OutputCount == 1, "Expected one graph output");
	Require(graph.GetExternalInputs().size() == 1, "Expected one external input entry");
	Require(graph.GetExternalInputs()[0] == "CameraView", "Expected CameraView external input");
	HE::Rendering::RenderPassContext context;
	Require(graph.Execute(context), "Expected valid graph execute to succeed");
	Require(executionOrder.size() == 2, "Expected two passes to execute");
	Require(executionOrder[0] == "ExtractedSceneInput", "Expected first pass execution order to be stable");
	Require(executionOrder[1] == "ForwardOpaque", "Expected second pass execution order to be stable");

	HE::Rendering::PassGraph typedGraph;
	const auto importedTarget = typedGraph.AddImportedResource({
		.Name = "RenderTarget",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = 64,
			.Height = 64,
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	});
	const auto transientSceneColor = typedGraph.AddTransientResource({
		.Name = "SceneColor",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = 64,
			.Height = 64,
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	});
	Require(importedTarget.IsValid(), "Expected imported render target handle to be valid");
	Require(transientSceneColor.IsValid(), "Expected transient scene color handle to be valid");
	typedGraph.AddPass({
		.Name = "ForwardOpaqueTyped",
		.Inputs = { "RenderTarget" },
		.Outputs = { "SceneColor" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(typedGraph.Compile(), "Expected typed graph compile to succeed");
	const auto& typedStats = typedGraph.GetStats();
	Require(typedStats.ImportedResourceCount == 1, "Expected one imported resource");
	Require(typedStats.TransientResourceCount == 1, "Expected one transient resource");
	const auto& lifetimes = typedGraph.GetResourceAllocator().GetLifetimes();
	Require(lifetimes.size() == 2, "Expected lifetimes for imported and transient resources");
	Require(lifetimes[0].FirstPassIndex == 0 && lifetimes[0].LastPassIndex == 0, "Expected imported resource lifetime to cover typed pass");
	Require(lifetimes[1].FirstPassIndex == 0 && lifetimes[1].LastPassIndex == 0, "Expected transient resource lifetime to cover typed pass");

	std::cout << "RenderPassGraphSmoke passed" << std::endl;
	return 0;
}
