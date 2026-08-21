#include "enginepch.h"
#include "RenderGraph.h"

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
			std::vector<RenderGraphDiagnostic>& diagnostics,
			RenderGraphDiagnosticCode code,
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

	RenderGraphPassHandle RenderGraph::AddPass(RenderGraphPassDesc pass) {
		m_Passes.push_back(std::move(pass));
		m_Compiled = false;
		return { static_cast<uint32_t>(m_Passes.size() - 1) };
	}

	void RenderGraph::AddOutputResource(RenderGraphResourceHandle resource) {
		m_DeclaredOutputs.push_back(resource);
		m_Compiled = false;
	}

	void RenderGraph::SetBarrierExecutor(RenderGraphBarrierExecutor executor) {
		m_BarrierExecutor = std::move(executor);
	}

	RenderGraphResourceHandle RenderGraph::AddImportedResource(RenderGraphResourceDesc desc) {
		const auto handle = m_ResourceAllocator.AddImportedResource(std::move(desc));
		m_Compiled = false;
		return handle;
	}

	RenderGraphResourceHandle RenderGraph::AddTransientResource(RenderGraphResourceDesc desc) {
		const auto handle = m_ResourceAllocator.AddTransientResource(std::move(desc));
		m_Compiled = false;
		return handle;
	}

	bool RenderGraph::Compile() {
		m_Diagnostics.clear();
		m_BarrierPlan.clear();
		m_ExecutionOrder.clear();
		m_QueueBatches.clear();
		m_Stats = {};
		m_ResourceAllocator.ClearLifetimes();
		if (m_Passes.empty()) {
			AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::EmptyGraph, {},
						  "RenderGraph must contain at least one pass");
			m_Compiled = false;
			return false;
		}

		std::unordered_set<std::string> passNames, resources, readResources;
		std::unordered_map<std::string, uint32_t> writers, firstUses, lastUses;
		std::unordered_map<std::string, std::vector<uint32_t>> readers;
		std::unordered_map<std::string, ResourceState> declarationStates;
		std::vector<std::vector<uint32_t>> dependencyEdges(m_Passes.size());
		uint32_t accessEdgeCount = 0;
		for (const auto &desc : m_ResourceAllocator.GetResources()) {
			if (!IsResourceDescriptionValid(desc))
				AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::InvalidResourceDescription, {},
							  "Render graph resource description is invalid");
		}

		for (uint32_t passIndex = 0; passIndex < m_Passes.size(); ++passIndex) {
			const auto &pass = m_Passes[passIndex];
			if (pass.Name.empty())
				AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::EmptyPassName, {},
							  "Render pass name must not be empty");
			else if (!passNames.insert(pass.Name).second)
				AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::DuplicatePassName, pass.Name,
							  "Render pass name must be unique");
			if (!pass.Execute)
				AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::MissingExecuteCallback, pass.Name,
							  "Render pass must provide an execute callback");
			if (pass.Type != RenderGraphPassType::Graphics && !pass.RenderPassAttachments.empty())
				AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::InvalidPassType, pass.Name,
							  "Only graphics passes may declare render-pass attachments");
			for (const auto dependency : pass.Dependencies) {
				if (!dependency.IsValid() || dependency.Index >= m_Passes.size() || dependency.Index == passIndex)
					AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::InvalidResourceHandle, pass.Name,
								  "Render pass declares an invalid explicit dependency");
				else
					dependencyEdges[dependency.Index].push_back(passIndex);
			}

			auto usages = pass.ResourceUsages;
			for (const auto &attachment : pass.RenderPassAttachments)
				usages.push_back({attachment.Resource, RenderGraphResourceUsage::Access::Write,
								  attachment.Kind == RenderGraphRenderPassAttachmentKind::Color
									  ? ResourceState::RenderTarget
									  : ResourceState::DepthStencilWrite});
			std::unordered_set<std::string> passResources;
			for (const auto &usage : usages) {
				const auto *resource = m_ResourceAllocator.GetDesc(usage.Resource);
				if (!resource) {
					AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::InvalidResourceHandle, pass.Name,
								  "Render pass references an invalid resource handle");
					continue;
				}
				const bool textureState =
					usage.State == ResourceState::ShaderRead || usage.State == ResourceState::RenderTarget ||
					usage.State == ResourceState::DepthStencilWrite || usage.State == ResourceState::CopySrc ||
					usage.State == ResourceState::CopyDst;
				const bool bufferState =
					usage.State == ResourceState::CopySrc || usage.State == ResourceState::CopyDst ||
					usage.State == ResourceState::VertexBuffer || usage.State == ResourceState::IndexBuffer;
				if (usage.State == ResourceState::Undefined ||
					(resource->Kind == RenderGraphResourceKind::Texture ? !textureState : !bufferState)) {
					AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::InvalidResourceUsage, pass.Name,
								  "Render pass resource usage state is not supported");
					continue;
				}
				const auto &name = resource->Name;
				if (!passResources.insert(name).second) {
					AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::DuplicateResourceAccess, pass.Name,
								  "Render pass declares the same resource more than once");
					continue;
				}
				resources.insert(name);
				if (!firstUses.contains(name))
					firstUses.emplace(name, passIndex);
				lastUses[name] = passIndex;
				const auto before =
					declarationStates.contains(name) ? declarationStates[name] : ResourceState::Undefined;
				if (before != usage.State) {
					m_BarrierPlan.push_back({pass.Name, name, passIndex, before, usage.State});
					declarationStates[name] = usage.State;
				}
				if (usage.AccessMode == RenderGraphResourceUsage::Access::Read) {
					readers[name].push_back(passIndex);
					readResources.insert(name);
					++accessEdgeCount;
				} else if (!writers.emplace(name, passIndex).second)
					AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::DuplicateResourceWriter, pass.Name,
								  "Render graph resource must not have multiple writers");
			}
		}
		for (const auto &[name, resourceReaders] : readers) {
			const auto writer = writers.find(name);
			if (writer == writers.end()) {
				const auto *desc = m_ResourceAllocator.GetDesc(m_ResourceAllocator.FindByName(name));
				if (!desc || desc->Storage != RenderGraphResourceStorage::Imported)
					for (const auto reader : resourceReaders)
						AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::MissingResourceProducer,
									  m_Passes[reader].Name,
									  "Render pass reads a resource that has no producer or imported source");
			} else
				for (const auto reader : resourceReaders)
					dependencyEdges[writer->second].push_back(reader);
		}
		if (!m_Diagnostics.empty()) {
			m_Compiled = false;
			return false;
		}

		std::vector<uint32_t> incoming(m_Passes.size());
		for (auto &edges : dependencyEdges) {
			std::sort(edges.begin(), edges.end());
			edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
			for (const auto dependent : edges)
				++incoming[dependent];
		}
		std::deque<uint32_t> ready;
		for (uint32_t index = 0; index < m_Passes.size(); ++index)
			if (incoming[index] == 0)
				ready.push_back(index);
		while (!ready.empty()) {
			const auto index = ready.front();
			ready.pop_front();
			m_ExecutionOrder.push_back(index);
			for (const auto dependent : dependencyEdges[index])
				if (--incoming[dependent] == 0)
					ready.push_back(dependent);
		}
		if (m_ExecutionOrder.size() != m_Passes.size()) {
			AddDiagnostic(m_Diagnostics, RenderGraphDiagnosticCode::CyclicDependency, {},
						  "Render graph contains a cyclic pass dependency");
			m_Compiled = false;
			m_ExecutionOrder.clear();
			return false;
		}

		std::vector<std::vector<uint32_t>> reverse(m_Passes.size());
		for (uint32_t producer = 0; producer < dependencyEdges.size(); ++producer)
			for (const auto consumer : dependencyEdges[producer])
				reverse[consumer].push_back(producer);
		if (!m_DeclaredOutputs.empty()) {
			std::vector<bool> required(m_Passes.size());
			std::deque<uint32_t> pending;
			for (uint32_t index = 0; index < m_Passes.size(); ++index)
				if (m_Passes[index].HasSideEffects) {
					required[index] = true;
					pending.push_back(index);
				}
			for (const auto output : m_DeclaredOutputs) {
				const auto *desc = m_ResourceAllocator.GetDesc(output);
				const auto writer = desc ? writers.find(desc->Name) : writers.end();
				if (!desc || writer == writers.end()) {
					m_Compiled = false;
					return false;
				}
				if (!required[writer->second]) {
					required[writer->second] = true;
					pending.push_back(writer->second);
				}
				++m_Stats.OutputCount;
			}
			while (!pending.empty()) {
				const auto pass = pending.front();
				pending.pop_front();
				for (const auto dependency : reverse[pass])
					if (!required[dependency]) {
						required[dependency] = true;
						pending.push_back(dependency);
					}
			}
			std::erase_if(m_ExecutionOrder, [&required](uint32_t pass) { return !required[pass]; });
			m_Stats.CulledPassCount = static_cast<uint32_t>(m_Passes.size() - m_ExecutionOrder.size());
		} else
			for (const auto &[name, writer] : writers)
				if (!readResources.contains(name))
					++m_Stats.OutputCount;

		const auto planned = std::move(m_BarrierPlan);
		m_BarrierPlan.clear();
		std::unordered_map<std::string, ResourceState> executionStates;
		for (const auto passIndex : m_ExecutionOrder)
			for (const auto &barrier : planned)
				if (barrier.PassIndex == passIndex) {
					const auto before = executionStates.contains(barrier.ResourceName)
											? executionStates[barrier.ResourceName]
											: ResourceState::Undefined;
					if (before != barrier.After) {
						m_BarrierPlan.push_back(
							{barrier.PassName, barrier.ResourceName, passIndex, before, barrier.After});
						executionStates[barrier.ResourceName] = barrier.After;
					}
				}
		const auto queueFor = [this](uint32_t index) {
			return m_Passes[index].Type == RenderGraphPassType::Compute ? RenderQueueType::Compute
				   : m_Passes[index].Type == RenderGraphPassType::Copy  ? RenderQueueType::Copy
																	  : RenderQueueType::Graphics;
		};
		std::vector<uint32_t> batchIndices(m_Passes.size(), std::numeric_limits<uint32_t>::max());
		for (const auto passIndex : m_ExecutionOrder) {
			const auto queue = queueFor(passIndex);
			if (m_QueueBatches.empty() || m_QueueBatches.back().Queue != queue)
				m_QueueBatches.push_back({.Queue = queue});
			batchIndices[passIndex] = static_cast<uint32_t>(m_QueueBatches.size() - 1);
			m_QueueBatches.back().PassIndices.push_back(passIndex);
		}
		for (uint32_t producer = 0; producer < dependencyEdges.size(); ++producer)
			for (const auto consumer : dependencyEdges[producer])
				if (batchIndices[producer] != std::numeric_limits<uint32_t>::max() &&
					batchIndices[consumer] != std::numeric_limits<uint32_t>::max() &&
					batchIndices[producer] != batchIndices[consumer]) {
					auto &waits = m_QueueBatches[batchIndices[consumer]].WaitBatchIndices;
					if (std::find(waits.begin(), waits.end(), batchIndices[producer]) == waits.end())
						waits.push_back(batchIndices[producer]);
				}
		m_Stats.ResourceCount = static_cast<uint32_t>(resources.size());
		m_Stats.EdgeCount = accessEdgeCount + m_Stats.OutputCount;
		for (const auto &desc : m_ResourceAllocator.GetResources()) {
			if (desc.Storage == RenderGraphResourceStorage::Imported)
				++m_Stats.ImportedResourceCount;
			else
				++m_Stats.TransientResourceCount;
			const auto first = firstUses.find(desc.Name);
			const auto last = lastUses.find(desc.Name);
			if (first != firstUses.end() && last != lastUses.end())
				m_ResourceAllocator.SetLifetime(m_ResourceAllocator.FindByName(desc.Name), first->second, last->second);
		}
		m_Compiled = true;
		return true;
	}

	bool RenderGraph::Execute(RenderPassContext& context) {
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

				auto textureView = context.Device->CreateTextureView({
					.Texture = runtimeResource->Texture,
					.Aspect = attachment.Kind == RenderGraphRenderPassAttachmentKind::DepthStencil
						? TextureAspect::DepthStencil
						: TextureAspect::Color
				});
				if (!textureView) {
					succeeded = false;
					break;
				}

				if (attachment.Kind == RenderGraphRenderPassAttachmentKind::Color) {
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

	void RenderGraph::ReleaseTransientResources(uint64_t fenceValue) {
		m_ResourceAllocator.ReleaseTransientResources(fenceValue);
	}

	void RenderGraph::Reset() {
		m_Passes.clear();
		m_DeclaredOutputs.clear();
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
