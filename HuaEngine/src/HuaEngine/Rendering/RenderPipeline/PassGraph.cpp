#include "enginepch.h"
#include "PassGraph.h"

#include <unordered_map>
#include <unordered_set>

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(
			std::vector<PassGraphDiagnostic>& diagnostics,
			PassGraphDiagnosticCode code,
			std::string passName,
			std::string message) {
			diagnostics.push_back({ code, std::move(passName), std::move(message) });
		}
	}

	void PassGraph::AddPass(RenderPassDesc pass) {
		m_Passes.push_back(std::move(pass));
		m_Compiled = false;
	}

	void PassGraph::AddExternalInput(std::string resourceName) {
		m_ExternalInputs.push_back(std::move(resourceName));
		m_Compiled = false;
	}

	bool PassGraph::Compile() {
		m_Diagnostics.clear();
		m_Stats = {};

		if (m_Passes.empty()) {
			AddDiagnostic(
				m_Diagnostics,
				PassGraphDiagnosticCode::EmptyGraph,
				{},
				"PassGraph must contain at least one pass");
			m_Compiled = false;
			return false;
		}

		std::unordered_set<std::string> passNames;
		std::unordered_set<std::string> externalInputs;
		std::unordered_set<std::string> resources;
		std::unordered_set<std::string> readResources;
		std::unordered_map<std::string, std::string> resourceWriters;
		std::uint32_t inputEdgeCount = 0;

		for (const auto& resourceName : m_ExternalInputs) {
			if (resourceName.empty()) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::EmptyResourceName,
					{},
					"External input resource name must not be empty");
				continue;
			}

			if (!externalInputs.insert(resourceName).second) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::DuplicateResourceAccess,
					{},
					"External input resource name must be unique");
			}

			resources.insert(resourceName);
		}

		for (const auto& pass : m_Passes) {
			if (pass.Name.empty()) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::EmptyPassName,
					{},
					"Render pass name must not be empty");
			} else if (!passNames.insert(pass.Name).second) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::DuplicatePassName,
					pass.Name,
					"Render pass name must be unique");
			}

			if (!pass.Execute) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::MissingExecuteCallback,
					pass.Name,
					"Render pass must provide an execute callback");
			}

			std::unordered_set<std::string> passResources;
			for (const auto& input : pass.Inputs) {
				if (input.empty()) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::EmptyResourceName,
						pass.Name,
						"Render pass input resource name must not be empty");
					continue;
				}

				if (!passResources.insert(input).second) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceAccess,
						pass.Name,
						"Render pass must not access the same resource more than once");
				}

				resources.insert(input);
				readResources.insert(input);
				++inputEdgeCount;
			}

			for (const auto& output : pass.Outputs) {
				if (output.empty()) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::EmptyResourceName,
						pass.Name,
						"Render pass output resource name must not be empty");
					continue;
				}

				if (!passResources.insert(output).second) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceAccess,
						pass.Name,
						"Render pass must not access the same resource more than once");
				}

				resources.insert(output);
				const auto [writer, inserted] = resourceWriters.emplace(output, pass.Name);
				if (!inserted) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceWriter,
						pass.Name,
						"Render pass output resource must have a single writer");
				}
			}
		}

		for (const auto& resource : readResources) {
			if (!externalInputs.contains(resource) && !resourceWriters.contains(resource)) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::MissingResourceProducer,
					{},
					"Render pass input resource must be produced by a pass or external input");
			}
		}

		m_Compiled = m_Diagnostics.empty();
		if (m_Compiled) {
			std::uint32_t outputCount = 0;
			for (const auto& [resource, passName] : resourceWriters) {
				if (!readResources.contains(resource)) {
					++outputCount;
				}
			}

			m_Stats.ResourceCount = static_cast<std::uint32_t>(resources.size());
			m_Stats.ExternalInputCount = static_cast<std::uint32_t>(externalInputs.size());
			m_Stats.OutputCount = outputCount;
			m_Stats.EdgeCount = inputEdgeCount + outputCount;
		}

		return m_Compiled;
	}

	bool PassGraph::Execute(RenderPassContext& context) {
		if (!m_Compiled && !Compile()) {
			return false;
		}

		for (const auto& pass : m_Passes) {
			pass.Execute(context);
		}

		return true;
	}

	void PassGraph::Reset() {
		m_Passes.clear();
		m_ExternalInputs.clear();
		m_Diagnostics.clear();
		m_Stats = {};
		m_Compiled = false;
	}
}
