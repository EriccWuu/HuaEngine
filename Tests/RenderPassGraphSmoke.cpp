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

	std::cout << "RenderPassGraphSmoke passed" << std::endl;
	return 0;
}
