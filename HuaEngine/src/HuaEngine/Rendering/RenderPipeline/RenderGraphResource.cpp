#include "enginepch.h"
#include "RenderGraphResource.h"

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
		m_NameToIndex.clear();
	}

	void RenderGraphResourceAllocator::ClearLifetimes() {
		m_Lifetimes.clear();
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
