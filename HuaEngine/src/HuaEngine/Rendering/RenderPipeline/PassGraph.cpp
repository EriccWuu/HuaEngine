#include "enginepch.h"
#include "PassGraph.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(
			std::vector<PassGraphDiagnostic>& diagnostics,
			PassGraphDiagnosticCode code,
			std::string passName,
			std::string message) {
			diagnostics.push_back({ code, std::move(passName), std::move(message) });
		}

		bool IsResourceDescriptionValid(const RenderGraphResourceDesc& desc) {
			if (desc.Name.empty()) {
				return false;
			}

			if (desc.Kind == RenderGraphResourceKind::Texture) {
				return desc.Texture.Width > 0 && desc.Texture.Height > 0 && desc.Texture.Format != RenderTargetTextureFormat::None;
			}

			return desc.Buffer.Size > 0;
		}
	}

	void PassGraph::AddPass(PassGraphPassDesc pass) {
		m_Passes.push_back(std::move(pass));
		m_Compiled = false;
	}

	void PassGraph::AddExternalInput(std::string resourceName) {
		m_ExternalInputs.push_back(std::move(resourceName));
		m_Compiled = false;
	}

	void PassGraph::SetBarrierExecutor(PassGraphBarrierExecutor executor) {
		m_BarrierExecutor = std::move(executor);
	}

	RenderGraphResourceHandle PassGraph::AddImportedResource(RenderGraphResourceDesc desc) {
		AddExternalInput(desc.Name);
		const auto handle = m_ResourceAllocator.AddImportedResource(std::move(desc));
		m_Compiled = false;
		return handle;
	}

	RenderGraphResourceHandle PassGraph::AddTransientResource(RenderGraphResourceDesc desc) {
		const auto handle = m_ResourceAllocator.AddTransientResource(std::move(desc));
		m_Compiled = false;
		return handle;
	}

	bool PassGraph::Compile() {
		m_Diagnostics.clear();
		m_BarrierPlan.clear();
		m_Stats = {};
		m_ResourceAllocator.ClearLifetimes();

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
		std::unordered_set<std::string> availableResources;
		std::unordered_set<std::string> resources;
		std::unordered_set<std::string> readResources;
		std::unordered_map<std::string, std::string> resourceWriters;
		std::unordered_set<std::string> typedResourceWriters;
		std::unordered_map<std::string, uint32_t> firstUsePass;
		std::unordered_map<std::string, uint32_t> lastUsePass;
		std::unordered_map<std::string, ResourceState> resourceStates;
		std::uint32_t inputEdgeCount = 0;
		const auto resolveResourceNames = [this](
			const std::vector<std::string>& legacyNames,
			const std::vector<RenderGraphResourceHandle>& handles,
			const std::string& passName) {
			std::vector<std::string> names = legacyNames;
			names.reserve(names.size() + handles.size());
			for (const auto handle : handles) {
				const auto* resource = m_ResourceAllocator.GetDesc(handle);
				if (!resource) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::InvalidResourceHandle,
						passName,
						"Render pass references an invalid typed resource handle");
					continue;
				}

				names.push_back(resource->Name);
			}

			return names;
		};

		for (const auto& desc : m_ResourceAllocator.GetResources()) {
			if (!IsResourceDescriptionValid(desc)) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::InvalidResourceDescription,
					{},
					"Render graph resource description is invalid");
			}
		}

		for (const auto& resourceName : m_ExternalInputs) {
			if (resourceName.empty()) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::EmptyResourceName,
					{},
					"Render graph resource name must not be empty");
				continue;
			}

			if (!externalInputs.insert(resourceName).second) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::DuplicateResourceAccess,
					{},
					"Render pass declares the same resource more than once");
			}

			availableResources.insert(resourceName);
			resources.insert(resourceName);
		}

		for (uint32_t passIndex = 0; passIndex < m_Passes.size(); ++passIndex) {
			const auto& pass = m_Passes[passIndex];
			const auto inputResources = resolveResourceNames(pass.Inputs, pass.InputResources, pass.Name);
			const auto outputResources = resolveResourceNames(pass.Outputs, pass.OutputResources, pass.Name);
			std::unordered_set<std::string> typedOutputs;
			for (const auto handle : pass.OutputResources) {
				if (const auto* resource = m_ResourceAllocator.GetDesc(handle)) {
					typedOutputs.insert(resource->Name);
				}
			}
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
			for (const auto& input : inputResources) {
				if (input.empty()) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::EmptyResourceName,
						pass.Name,
						"Render graph resource name must not be empty");
					continue;
				}

				if (!passResources.insert(input).second) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceAccess,
						pass.Name,
						"Render pass declares the same resource more than once");
				}

				if (!availableResources.contains(input)) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::MissingResourceProducer,
						pass.Name,
						"Render pass reads a resource that has no producer or external input");
				}

				resources.insert(input);
				readResources.insert(input);
				if (!firstUsePass.contains(input)) {
					firstUsePass.emplace(input, passIndex);
				}
				lastUsePass[input] = passIndex;
				const auto before = resourceStates.contains(input) ? resourceStates[input] : ResourceState::Undefined;
				if (before != ResourceState::ShaderRead) {
					m_BarrierPlan.push_back({
						.PassName = pass.Name,
						.ResourceName = input,
						.PassIndex = passIndex,
						.Before = before,
						.After = ResourceState::ShaderRead
					});
					resourceStates[input] = ResourceState::ShaderRead;
				}
				++inputEdgeCount;
			}

			for (const auto& output : outputResources) {
				if (output.empty()) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::EmptyResourceName,
						pass.Name,
						"Render graph resource name must not be empty");
					continue;
				}

				if (!passResources.insert(output).second) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceAccess,
						pass.Name,
						"Render pass declares the same resource more than once");
				}

				resources.insert(output);
				if (!firstUsePass.contains(output)) {
					firstUsePass.emplace(output, passIndex);
				}
				lastUsePass[output] = passIndex;
				const auto before = resourceStates.contains(output) ? resourceStates[output] : ResourceState::Undefined;
				if (before != ResourceState::RenderTarget) {
					m_BarrierPlan.push_back({
						.PassName = pass.Name,
						.ResourceName = output,
						.PassIndex = passIndex,
						.Before = before,
						.After = ResourceState::RenderTarget
					});
					resourceStates[output] = ResourceState::RenderTarget;
				}
				const auto [writer, inserted] = resourceWriters.emplace(output, pass.Name);
				const bool isTypedOutput = typedOutputs.contains(output);
				if (!inserted && (!isTypedOutput || !typedResourceWriters.contains(output))) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceWriter,
						pass.Name,
						"Render graph resource must not have multiple writers");
				}
				if (isTypedOutput) {
					typedResourceWriters.insert(output);
				}
			}

			for (const auto& output : outputResources) {
				if (!output.empty()) {
					availableResources.insert(output);
				}
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
			for (const auto& desc : m_ResourceAllocator.GetResources()) {
				if (desc.Storage == RenderGraphResourceStorage::Imported) {
					++m_Stats.ImportedResourceCount;
				}
				else {
					++m_Stats.TransientResourceCount;
				}

				const auto firstUse = firstUsePass.find(desc.Name);
				const auto lastUse = lastUsePass.find(desc.Name);
				if (firstUse != firstUsePass.end() && lastUse != lastUsePass.end()) {
					m_ResourceAllocator.SetLifetime(
						m_ResourceAllocator.FindByName(desc.Name),
						firstUse->second,
						lastUse->second);
				}
			}
		}

		return m_Compiled;
	}

	bool PassGraph::Execute(RenderPassContext& context) {
		if (!m_Compiled && !Compile()) {
			return false;
		}

		if (context.Device && !m_ResourceAllocator.PrepareRuntimeResources(*context.Device)) {
			return false;
		}

		const auto* const previousGraphResources = context.GraphResources;
		context.GraphResources = &m_ResourceAllocator;

		for (uint32_t passIndex = 0; passIndex < m_Passes.size(); ++passIndex) {
			for (const auto& barrier : m_BarrierPlan) {
				if (barrier.PassIndex == passIndex) {
					if (context.Commands && context.ResourceStates) {
						const auto handle = m_ResourceAllocator.FindByName(barrier.ResourceName);
						const auto* runtimeResource = m_ResourceAllocator.GetRuntimeResource(handle);
						if (runtimeResource && runtimeResource->Texture) {
							ResourceBarrier emittedBarrier;
							if (context.ResourceStates->Transition(runtimeResource->Texture, barrier.After, emittedBarrier)) {
								context.Commands->ResourceBarrier(emittedBarrier);
							}
						}
					}

					if (m_BarrierExecutor) {
						m_BarrierExecutor(barrier, context);
					}
				}
			}

			m_Passes[passIndex].Execute(context);
		}

		context.GraphResources = previousGraphResources;

		return true;
	}

	void PassGraph::Reset() {
		m_Passes.clear();
		m_ExternalInputs.clear();
		m_ResourceAllocator.Reset();
		m_Diagnostics.clear();
		m_BarrierPlan.clear();
		m_BarrierExecutor = nullptr;
		m_Stats = {};
		m_Compiled = false;
	}
}
