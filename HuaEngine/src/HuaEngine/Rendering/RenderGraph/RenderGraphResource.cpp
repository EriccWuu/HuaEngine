#include "enginepch.h"
#include "RenderGraphResource.h"

#include <algorithm>

#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	RenderGraphResourceHandle RenderGraphResourceAllocator::AddImportedResource(RenderGraphResourceDesc desc) {
		return AddResource(std::move(desc), RenderGraphResourceStorage::Imported);
	}

	RenderGraphResourceHandle RenderGraphResourceAllocator::AddTransientResource(RenderGraphResourceDesc desc) {
		return AddResource(std::move(desc), RenderGraphResourceStorage::Transient);
	}

	void RenderGraphResourceAllocator::Reset() {
		m_Resources.clear();
		m_Lifetimes.clear();
		m_RuntimeResources.clear();
		m_NameToIndex.clear();
	}

	void RenderGraphResourceAllocator::ClearLifetimes() {
		m_Lifetimes.clear();
	}

	bool RenderGraphResourceAllocator::PrepareRuntimeResources(RenderDevice& device, uint64_t completedFenceValue) {
		m_RuntimeResources.clear();
		m_RuntimeResources.reserve(m_Resources.size());
		m_ActiveTransientTextureIndices.clear();
		m_ActiveTransientBufferIndices.clear();

		for (uint32_t index = 0; index < m_Resources.size(); ++index) {
			const auto& desc = m_Resources[index];
			RenderGraphRuntimeResource runtimeResource{
				.Handle = { index },
				.Name = desc.Name
			};

			if (desc.Kind == RenderGraphResourceKind::Texture && desc.Storage == RenderGraphResourceStorage::Imported) {
				runtimeResource.Texture = desc.RuntimeTexture;
			}
			if (desc.Kind == RenderGraphResourceKind::Buffer && desc.Storage == RenderGraphResourceStorage::Imported) runtimeResource.Buffer = desc.RuntimeBuffer;

			m_RuntimeResources.push_back(std::move(runtimeResource));
		}

		struct TransientTextureAllocation {
			uint32_t ResourceIndex = 0;
			uint32_t FirstPassIndex = 0;
			uint32_t LastPassIndex = std::numeric_limits<uint32_t>::max();
		};
		std::vector<TransientTextureAllocation> allocations;
		for (const auto& lifetime : m_Lifetimes) {
			const auto* desc = GetDesc(lifetime.Handle);
			if (desc && desc->Storage == RenderGraphResourceStorage::Transient && desc->Kind == RenderGraphResourceKind::Texture) {
				allocations.push_back({ lifetime.Handle.Index, lifetime.FirstPassIndex, lifetime.LastPassIndex });
			}
		}
		std::vector<TransientTextureAllocation> bufferAllocations;
		for (const auto& lifetime : m_Lifetimes) {
			const auto* desc = GetDesc(lifetime.Handle);
			if (desc && desc->Storage == RenderGraphResourceStorage::Transient && desc->Kind == RenderGraphResourceKind::Buffer) bufferAllocations.push_back({ lifetime.Handle.Index, lifetime.FirstPassIndex, lifetime.LastPassIndex });
		}
		std::sort(allocations.begin(), allocations.end(), [](const auto& left, const auto& right) {
			return left.FirstPassIndex != right.FirstPassIndex
				? left.FirstPassIndex < right.FirstPassIndex
				: left.ResourceIndex < right.ResourceIndex;
		});

		std::vector<uint32_t> poolLastPassIndices(m_TransientTexturePool.size(), std::numeric_limits<uint32_t>::max());
		for (const auto& allocation : allocations) {
			const auto& desc = m_Resources[allocation.ResourceIndex];
			auto poolEntry = m_TransientTexturePool.size();
			const bool isDepthStencil = desc.Texture.Format == RenderTargetTextureFormat::DEPTH24_STENCIL8;
			for (uint32_t poolIndex = 0; poolIndex < m_TransientTexturePool.size(); ++poolIndex) {
				const auto& entry = m_TransientTexturePool[poolIndex];
				const bool hasMatchingDesc = entry.Desc.Width == desc.Texture.Width
					&& entry.Desc.Height == desc.Texture.Height
					&& (isDepthStencil
						? !desc.Texture.AttachmentGroup.empty() && entry.Desc.AttachmentGroup == desc.Texture.AttachmentGroup && entry.DepthStencilTexture
						: entry.Desc.Format == desc.Texture.Format && entry.Desc.AttachmentGroup == desc.Texture.AttachmentGroup);
				const bool lifetimeDoesNotOverlap = poolLastPassIndices[poolIndex] == std::numeric_limits<uint32_t>::max()
					|| poolLastPassIndices[poolIndex] < allocation.FirstPassIndex;
				if (hasMatchingDesc && entry.AvailableAfterFenceValue <= completedFenceValue && (isDepthStencil || lifetimeDoesNotOverlap)) {
					poolEntry = poolIndex;
					break;
				}
			}

			if (poolEntry == m_TransientTexturePool.size()) {
				if (isDepthStencil) {
					return false;
				}
				RenderTargetAttachmentSpecification attachments{ desc.Texture.Format };
				if (!desc.Texture.AttachmentGroup.empty()) {
					const auto depthResource = std::find_if(m_Resources.begin(), m_Resources.end(), [&desc](const auto& candidate) {
						return candidate.Storage == RenderGraphResourceStorage::Transient
							&& candidate.Kind == RenderGraphResourceKind::Texture
							&& candidate.Texture.AttachmentGroup == desc.Texture.AttachmentGroup
							&& candidate.Texture.Format == RenderTargetTextureFormat::DEPTH24_STENCIL8;
					});
					if (depthResource != m_Resources.end()) {
						attachments = { desc.Texture.Format, RenderTargetTextureFormat::DEPTH24_STENCIL8 };
					}
				}
				auto renderTarget = device.CreateRenderTarget({
					.Specification = {
						.Width = desc.Texture.Width,
						.Height = desc.Texture.Height,
						.Attachments = attachments
					}
				});
				if (!renderTarget || !renderTarget->GetColorAttachmentTexture()) {
					return false;
				}

				poolEntry = m_TransientTexturePool.size();
				m_TransientTexturePool.push_back({
					.Desc = desc.Texture,
					.Texture = renderTarget->GetColorAttachmentTexture(),
					.DepthStencilTexture = renderTarget->GetDepthStencilAttachmentTexture()
				});
				poolLastPassIndices.push_back(std::numeric_limits<uint32_t>::max());
			}

			m_RuntimeResources[allocation.ResourceIndex].Texture = isDepthStencil
				? m_TransientTexturePool[poolEntry].DepthStencilTexture
				: m_TransientTexturePool[poolEntry].Texture;
			poolLastPassIndices[poolEntry] = allocation.LastPassIndex;
			if (std::find(m_ActiveTransientTextureIndices.begin(), m_ActiveTransientTextureIndices.end(), poolEntry) == m_ActiveTransientTextureIndices.end()) {
				m_ActiveTransientTextureIndices.push_back(poolEntry);
			}
		}

		std::sort(bufferAllocations.begin(), bufferAllocations.end(), [](const auto& left, const auto& right) {
			return left.FirstPassIndex != right.FirstPassIndex ? left.FirstPassIndex < right.FirstPassIndex : left.ResourceIndex < right.ResourceIndex;
		});
		std::vector<uint32_t> bufferPoolLastPassIndices(m_TransientBufferPool.size(), std::numeric_limits<uint32_t>::max());
		for (const auto& allocation : bufferAllocations) {
			const auto& desc = m_Resources[allocation.ResourceIndex];
			auto poolEntry = m_TransientBufferPool.size();
			for (uint32_t poolIndex = 0; poolIndex < m_TransientBufferPool.size(); ++poolIndex) {
				const auto& entry = m_TransientBufferPool[poolIndex];
				const bool matching = entry.Desc.Size == desc.Buffer.Size && entry.Desc.Stride == desc.Buffer.Stride && entry.Desc.Usage == desc.Buffer.Usage;
				const bool nonOverlapping = bufferPoolLastPassIndices[poolIndex] == std::numeric_limits<uint32_t>::max() || bufferPoolLastPassIndices[poolIndex] < allocation.FirstPassIndex;
				if (matching && nonOverlapping && entry.AvailableAfterFenceValue <= completedFenceValue) { poolEntry = poolIndex; break; }
			}
			if (poolEntry == m_TransientBufferPool.size()) {
				auto buffer = device.CreateBuffer({ .Usage = desc.Buffer.Usage, .Size = desc.Buffer.Size, .Stride = desc.Buffer.Stride }, nullptr);
				if (!buffer) return false;
				poolEntry = m_TransientBufferPool.size();
				m_TransientBufferPool.push_back({ .Desc = desc.Buffer, .Buffer = std::move(buffer) });
				bufferPoolLastPassIndices.push_back(std::numeric_limits<uint32_t>::max());
			}
			m_RuntimeResources[allocation.ResourceIndex].Buffer = m_TransientBufferPool[poolEntry].Buffer;
			bufferPoolLastPassIndices[poolEntry] = allocation.LastPassIndex;
			if (std::find(m_ActiveTransientBufferIndices.begin(), m_ActiveTransientBufferIndices.end(), poolEntry) == m_ActiveTransientBufferIndices.end()) m_ActiveTransientBufferIndices.push_back(poolEntry);
		}

		return true;
	}

	void RenderGraphResourceAllocator::ReleaseTransientResources(uint64_t fenceValue) {
		for (const auto poolIndex : m_ActiveTransientTextureIndices) {
			m_TransientTexturePool[poolIndex].AvailableAfterFenceValue = fenceValue;
		}
		m_ActiveTransientTextureIndices.clear();
		for (const auto poolIndex : m_ActiveTransientBufferIndices) m_TransientBufferPool[poolIndex].AvailableAfterFenceValue = fenceValue;
		m_ActiveTransientBufferIndices.clear();
	}

	void RenderGraphResourceAllocator::SetLifetime(RenderGraphResourceHandle handle, uint32_t firstPassIndex, uint32_t lastPassIndex) {
		const auto* desc = GetDesc(handle);
		if (!desc) {
			return;
		}

		m_Lifetimes.push_back({
			.Handle = handle,
			.Name = desc->Name,
			.FirstPassIndex = firstPassIndex,
			.LastPassIndex = lastPassIndex,
			.Storage = desc->Storage
		});
	}

	const RenderGraphResourceDesc* RenderGraphResourceAllocator::GetDesc(RenderGraphResourceHandle handle) const {
		if (!handle.IsValid() || handle.Index >= m_Resources.size()) {
			return nullptr;
		}

		return &m_Resources[handle.Index];
	}

	const RenderGraphRuntimeResource* RenderGraphResourceAllocator::GetRuntimeResource(RenderGraphResourceHandle handle) const {
		if (!handle.IsValid() || handle.Index >= m_RuntimeResources.size()) {
			return nullptr;
		}

		return &m_RuntimeResources[handle.Index];
	}

	RenderGraphResourceHandle RenderGraphResourceAllocator::FindByName(const std::string& name) const {
		const auto found = m_NameToIndex.find(name);
		if (found == m_NameToIndex.end()) {
			return {};
		}

		return { found->second };
	}

	RenderGraphResourceHandle RenderGraphResourceAllocator::AddResource(RenderGraphResourceDesc desc, RenderGraphResourceStorage storage) {
		if (desc.Name.empty()) {
			return {};
		}

		const auto existing = m_NameToIndex.find(desc.Name);
		if (existing != m_NameToIndex.end()) {
			return { existing->second };
		}

		desc.Storage = storage;
		const auto index = static_cast<uint32_t>(m_Resources.size());
		m_NameToIndex.emplace(desc.Name, index);
		m_Resources.push_back(std::move(desc));
		return { index };
	}
}
