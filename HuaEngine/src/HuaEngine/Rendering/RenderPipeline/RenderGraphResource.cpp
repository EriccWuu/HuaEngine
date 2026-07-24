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

		for (uint32_t index = 0; index < m_Resources.size(); ++index) {
			const auto& desc = m_Resources[index];
			RenderGraphRuntimeResource runtimeResource{
				.Handle = { index },
				.Name = desc.Name
			};

			if (desc.Kind == RenderGraphResourceKind::Texture && desc.Storage == RenderGraphResourceStorage::Imported) {
				runtimeResource.Texture = desc.RuntimeTexture;
			}

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
		std::sort(allocations.begin(), allocations.end(), [](const auto& left, const auto& right) {
			return left.FirstPassIndex < right.FirstPassIndex;
		});

		std::vector<uint32_t> poolLastPassIndices(m_TransientTexturePool.size(), std::numeric_limits<uint32_t>::max());
		for (const auto& allocation : allocations) {
			const auto& desc = m_Resources[allocation.ResourceIndex];
			auto poolEntry = m_TransientTexturePool.size();
			for (uint32_t poolIndex = 0; poolIndex < m_TransientTexturePool.size(); ++poolIndex) {
				const auto& entry = m_TransientTexturePool[poolIndex];
				const bool hasMatchingDesc = entry.Desc.Width == desc.Texture.Width
					&& entry.Desc.Height == desc.Texture.Height
					&& entry.Desc.Format == desc.Texture.Format;
				const bool lifetimeDoesNotOverlap = poolLastPassIndices[poolIndex] == std::numeric_limits<uint32_t>::max()
					|| poolLastPassIndices[poolIndex] < allocation.FirstPassIndex;
				if (hasMatchingDesc && entry.AvailableAfterFenceValue <= completedFenceValue && lifetimeDoesNotOverlap) {
					poolEntry = poolIndex;
					break;
				}
			}

			if (poolEntry == m_TransientTexturePool.size()) {
				auto texture = device.CreateTexture({
					.Width = desc.Texture.Width,
					.Height = desc.Texture.Height,
					.Format = desc.Texture.Format,
					.Usage = TextureUsageSampled | TextureUsageColorAttachment,
					.MipLevels = 1,
					.Samples = 1
				});
				if (!texture) {
					return false;
				}

				poolEntry = m_TransientTexturePool.size();
				m_TransientTexturePool.push_back({ .Desc = desc.Texture, .Texture = std::move(texture) });
				poolLastPassIndices.push_back(std::numeric_limits<uint32_t>::max());
			}

			m_RuntimeResources[allocation.ResourceIndex].Texture = m_TransientTexturePool[poolEntry].Texture;
			poolLastPassIndices[poolEntry] = allocation.LastPassIndex;
			if (std::find(m_ActiveTransientTextureIndices.begin(), m_ActiveTransientTextureIndices.end(), poolEntry) == m_ActiveTransientTextureIndices.end()) {
				m_ActiveTransientTextureIndices.push_back(poolEntry);
			}
		}

		return true;
	}

	void RenderGraphResourceAllocator::ReleaseTransientResources(uint64_t fenceValue) {
		for (const auto poolIndex : m_ActiveTransientTextureIndices) {
			m_TransientTexturePool[poolIndex].AvailableAfterFenceValue = fenceValue;
		}
		m_ActiveTransientTextureIndices.clear();
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
