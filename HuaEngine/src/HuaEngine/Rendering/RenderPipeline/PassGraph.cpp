#include "enginepch.h"
#include "PassGraph.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"
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

	void PassGraph::AddOutputResource(RenderGraphResourceHandle resource) {
		m_OutputResources.push_back(resource);
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
		m_ExecutionOrder.clear();
		m_QueueBatches.clear();
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
		std::unordered_map<std::string, uint32_t> passIndices;
		std::unordered_map<std::string, std::vector<uint32_t>> resourceReaders;
		std::vector<std::vector<uint32_t>> dependencyEdges(m_Passes.size());
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
			auto resourceUsages = pass.ResourceUsages;
			resourceUsages.reserve(resourceUsages.size() + pass.RenderPassAttachments.size());
			for (const auto& attachment : pass.RenderPassAttachments) {
				resourceUsages.push_back({
					.Resource = attachment.Resource,
					.State = attachment.Kind == PassGraphRenderPassAttachmentKind::Color
						? ResourceState::RenderTarget
						: ResourceState::DepthStencilWrite
				});
			}
			if (pass.Type != PassGraphPassType::Graphics && !pass.RenderPassAttachments.empty()) {
				AddDiagnostic(
					m_Diagnostics,
					PassGraphDiagnosticCode::InvalidPassType,
					pass.Name,
					"Only graphics passes may declare render-pass attachments");
			}
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
			} else {
				passIndices.emplace(pass.Name, passIndex);
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

				resourceReaders[input].push_back(passIndex);

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

			for (const auto& usage : resourceUsages) {
				const auto* resource = m_ResourceAllocator.GetDesc(usage.Resource);
				if (!resource) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::InvalidResourceHandle,
						pass.Name,
						"Render pass references an invalid explicit resource usage handle");
					continue;
				}

				const bool isTextureUsage = usage.State == ResourceState::ShaderRead
					|| usage.State == ResourceState::RenderTarget
					|| usage.State == ResourceState::DepthStencilWrite
					|| usage.State == ResourceState::CopySrc
					|| usage.State == ResourceState::CopyDst;
				if (!isTextureUsage || resource->Kind != RenderGraphResourceKind::Texture) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::InvalidResourceUsage,
						pass.Name,
						"Render pass explicit resource usage state is not supported");
					continue;
				}

				const auto& resourceName = resource->Name;
				if (!passResources.insert(resourceName).second) {
					AddDiagnostic(
						m_Diagnostics,
						PassGraphDiagnosticCode::DuplicateResourceAccess,
						pass.Name,
						"Render pass declares the same resource more than once");
				}

				const bool requiresProducer = usage.State == ResourceState::ShaderRead || usage.State == ResourceState::CopySrc;
				if (requiresProducer) {
					resourceReaders[resourceName].push_back(passIndex);
				}

				resources.insert(resourceName);
				if (!firstUsePass.contains(resourceName)) {
					firstUsePass.emplace(resourceName, passIndex);
				}
				lastUsePass[resourceName] = passIndex;
				const auto before = resourceStates.contains(resourceName) ? resourceStates[resourceName] : ResourceState::Undefined;
				if (before != usage.State) {
					m_BarrierPlan.push_back({
						.PassName = pass.Name,
						.ResourceName = resourceName,
						.PassIndex = passIndex,
						.Before = before,
						.After = usage.State
					});
					resourceStates[resourceName] = usage.State;
				}

				if (requiresProducer) {
					readResources.insert(resourceName);
					++inputEdgeCount;
				} else {
					resourceWriters[resourceName] = pass.Name;
					availableResources.insert(resourceName);
				}
			}
		}

		for (const auto& [resource, readers] : resourceReaders) {
			const auto writer = resourceWriters.find(resource);
			if (writer == resourceWriters.end()) {
				if (!externalInputs.contains(resource)) {
					for (const auto reader : readers) {
						AddDiagnostic(m_Diagnostics, PassGraphDiagnosticCode::MissingResourceProducer, m_Passes[reader].Name, "Render pass reads a resource that has no producer or external input");
					}
				}
				continue;
			}

			const auto writerIndex = passIndices.find(writer->second);
			if (writerIndex == passIndices.end()) continue;
			for (const auto reader : readers) dependencyEdges[writerIndex->second].push_back(reader);
		}

		m_Compiled = m_Diagnostics.empty();
		if (m_Compiled) {
			std::vector<uint32_t> incomingEdgeCounts(m_Passes.size(), 0);
			for (auto& edges : dependencyEdges) {
				std::sort(edges.begin(), edges.end());
				edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
				for (const auto dependentPass : edges) {
					++incomingEdgeCounts[dependentPass];
				}
			}
			std::deque<uint32_t> readyPasses;
			for (uint32_t passIndex = 0; passIndex < m_Passes.size(); ++passIndex) {
				if (incomingEdgeCounts[passIndex] == 0) {
					readyPasses.push_back(passIndex);
				}
			}
			while (!readyPasses.empty()) {
				const auto passIndex = readyPasses.front();
				readyPasses.pop_front();
				m_ExecutionOrder.push_back(passIndex);
				for (const auto dependentPass : dependencyEdges[passIndex]) {
					if (--incomingEdgeCounts[dependentPass] == 0) {
						readyPasses.push_back(dependentPass);
					}
				}
			}
			if (m_ExecutionOrder.size() != m_Passes.size()) {
				AddDiagnostic(m_Diagnostics, PassGraphDiagnosticCode::CyclicDependency, {}, "Render graph contains a cyclic pass dependency");
				m_Compiled = false;
				m_ExecutionOrder.clear();
				return false;
			}
			std::uint32_t outputCount = 0;
			if (!m_OutputResources.empty()) {
				std::vector<std::vector<uint32_t>> reverseDependencies(m_Passes.size());
				for (uint32_t producer = 0; producer < dependencyEdges.size(); ++producer) {
					for (const auto consumer : dependencyEdges[producer]) reverseDependencies[consumer].push_back(producer);
				}
				std::vector<bool> required(m_Passes.size(), false);
				std::deque<uint32_t> pending;
				for (uint32_t index = 0; index < m_Passes.size(); ++index) {
					if (m_Passes[index].HasSideEffects) { required[index] = true; pending.push_back(index); }
				}
				for (const auto output : m_OutputResources) {
					const auto* desc = m_ResourceAllocator.GetDesc(output);
					const auto writer = desc ? resourceWriters.find(desc->Name) : resourceWriters.end();
					if (!desc || writer == resourceWriters.end()) { m_Compiled = false; break; }
					const auto pass = passIndices.find(writer->second);
					if (pass != passIndices.end() && !required[pass->second]) { required[pass->second] = true; pending.push_back(pass->second); }
					++outputCount;
				}
				while (m_Compiled && !pending.empty()) {
					const auto pass = pending.front(); pending.pop_front();
					for (const auto dependency : reverseDependencies[pass]) {
						if (!required[dependency]) { required[dependency] = true; pending.push_back(dependency); }
					}
				}
				if (!m_Compiled) return false;
				std::erase_if(m_ExecutionOrder, [&required](uint32_t pass) { return !required[pass]; });
				m_Stats.CulledPassCount = static_cast<uint32_t>(m_Passes.size() - m_ExecutionOrder.size());
			} else {
				for (const auto& [resource, passName] : resourceWriters) if (!readResources.contains(resource)) ++outputCount;
			}

			const auto plannedBarriers = std::move(m_BarrierPlan);
			m_BarrierPlan.clear();
			std::unordered_map<std::string, ResourceState> executionStates;
			for (const auto passIndex : m_ExecutionOrder) {
				for (const auto& planned : plannedBarriers) {
					if (planned.PassIndex != passIndex) continue;
					const auto before = executionStates.contains(planned.ResourceName)
						? executionStates[planned.ResourceName]
						: ResourceState::Undefined;
					if (before != planned.After) {
						m_BarrierPlan.push_back({ planned.PassName, planned.ResourceName, passIndex, before, planned.After });
						executionStates[planned.ResourceName] = planned.After;
					}
				}
			}

			const auto queueForPass = [this](uint32_t passIndex) {
				switch (m_Passes[passIndex].Type) {
					case PassGraphPassType::Compute: return RenderQueueType::Compute;
					case PassGraphPassType::Copy: return RenderQueueType::Copy;
					case PassGraphPassType::Graphics: return RenderQueueType::Graphics;
				}
				return RenderQueueType::Graphics;
			};
			std::vector<uint32_t> passBatchIndices(m_Passes.size(), std::numeric_limits<uint32_t>::max());
			for (const auto passIndex : m_ExecutionOrder) {
				const auto queue = queueForPass(passIndex);
				if (m_QueueBatches.empty() || m_QueueBatches.back().Queue != queue) {
					m_QueueBatches.push_back({ .Queue = queue });
				}
				const auto batchIndex = static_cast<uint32_t>(m_QueueBatches.size() - 1);
				m_QueueBatches.back().PassIndices.push_back(passIndex);
				passBatchIndices[passIndex] = batchIndex;
			}
			for (uint32_t producer = 0; producer < dependencyEdges.size(); ++producer) {
				for (const auto consumer : dependencyEdges[producer]) {
					const auto producerBatch = passBatchIndices[producer];
					const auto consumerBatch = passBatchIndices[consumer];
					if (producerBatch != std::numeric_limits<uint32_t>::max()
						&& consumerBatch != std::numeric_limits<uint32_t>::max()
						&& producerBatch != consumerBatch) {
						auto& waits = m_QueueBatches[consumerBatch].WaitBatchIndices;
						if (std::find(waits.begin(), waits.end(), producerBatch) == waits.end()) waits.push_back(producerBatch);
					}
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

		if (context.Device && !m_ResourceAllocator.PrepareRuntimeResources(*context.Device, context.CompletedGraphicsFenceValue)) {
			return false;
		}

		const auto* const previousGraphResources = context.GraphResources;
		const auto* const previousGraphRenderPass = context.GraphRenderPass;
		context.GraphResources = &m_ResourceAllocator;
		bool succeeded = true;

		for (const auto passIndex : m_ExecutionOrder) {
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

			RenderPassDesc graphRenderPass;
			context.GraphRenderPass = nullptr;
			for (const auto& attachment : m_Passes[passIndex].RenderPassAttachments) {
				if (!context.Device) {
					succeeded = false;
					break;
				}

				const auto* runtimeResource = m_ResourceAllocator.GetRuntimeResource(attachment.Resource);
				if (!runtimeResource || !runtimeResource->Texture) {
					succeeded = false;
					break;
				}

				auto textureView = context.Device->CreateTextureView({ .Texture = runtimeResource->Texture });
				if (!textureView) {
					succeeded = false;
					break;
				}

				if (attachment.Kind == PassGraphRenderPassAttachmentKind::Color) {
					graphRenderPass.ColorAttachments.push_back({
						.View = textureView,
						.Load = attachment.Load,
						.Store = attachment.Store,
						.ClearColor = attachment.ClearColor
					});
				} else if (graphRenderPass.DepthStencilAttachment) {
					succeeded = false;
					break;
				} else {
					graphRenderPass.DepthStencilAttachment = RenderPassDepthStencilAttachment{
						.View = textureView,
						.DepthLoad = attachment.Load,
						.DepthStore = attachment.Store,
						.ClearDepth = attachment.ClearDepth,
						.ClearStencil = attachment.ClearStencil
					};
				}
			}

			if (!succeeded) {
				break;
			}

			if (!m_Passes[passIndex].RenderPassAttachments.empty()) {
				context.GraphRenderPass = &graphRenderPass;
				if (context.Commands) {
					context.Commands->BeginRenderPass(graphRenderPass);
				}
			}
			m_Passes[passIndex].Execute(context);
			if (!m_Passes[passIndex].RenderPassAttachments.empty() && context.Commands) {
				context.Commands->EndRenderPass();
			}
			context.GraphRenderPass = previousGraphRenderPass;
		}

		context.GraphResources = previousGraphResources;
		context.GraphRenderPass = previousGraphRenderPass;

		return succeeded;
	}

	void PassGraph::ReleaseTransientResources(uint64_t fenceValue) {
		m_ResourceAllocator.ReleaseTransientResources(fenceValue);
	}

	void PassGraph::Reset() {
		m_Passes.clear();
		m_ExternalInputs.clear();
		m_OutputResources.clear();
		m_ResourceAllocator.Reset();
		m_Diagnostics.clear();
		m_BarrierPlan.clear();
		m_ExecutionOrder.clear();
		m_QueueBatches.clear();
		m_BarrierExecutor = nullptr;
		m_Stats = {};
		m_Compiled = false;
	}
}
