#include "enginepch.h"
#include "RenderGraphResource.h"

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

	bool RenderGraphResourceAllocator::PrepareRuntimeResources(RenderDevice& device) {
		m_RuntimeResources.clear();
		m_RuntimeResources.reserve(m_Resources.size());

		for (uint32_t index = 0; index < m_Resources.size(); ++index) {
			const auto& desc = m_Resources[index];
			RenderGraphRuntimeResource runtimeResource{
				.Handle = { index },
				.Name = desc.Name
			};

			if (desc.Kind == RenderGraphResourceKind::Texture) {
				if (desc.Storage == RenderGraphResourceStorage::Imported) {
					runtimeResource.Texture = desc.RuntimeTexture;
				}
				else {
					runtimeResource.Texture = device.CreateTexture({
						.Width = desc.Texture.Width,
						.Height = desc.Texture.Height,
						.Format = desc.Texture.Format,
						.Usage = TextureUsageSampled | TextureUsageColorAttachment,
						.MipLevels = 1,
						.Samples = 1
					});
					if (!runtimeResource.Texture) {
						return false;
					}
				}
			}

			m_RuntimeResources.push_back(std::move(runtimeResource));
		}

		return true;
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
