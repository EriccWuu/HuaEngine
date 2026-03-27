#include "enginepch.h"
#include "OperationRegistry.h"

namespace HE {
	void OperationRegistry::Register(OperationDescriptor descriptor)
	{
		if (descriptor.Name.empty()) {
			return;
		}

		auto existing = m_DescriptorIndex.find(descriptor.Name);
		if (existing != m_DescriptorIndex.end()) {
			m_Descriptors[existing->second] = std::move(descriptor);
			return;
		}

		const auto index = m_Descriptors.size();
		m_DescriptorIndex.emplace(descriptor.Name, index);
		m_Descriptors.emplace_back(std::move(descriptor));
	}

	bool OperationRegistry::Contains(std::string_view name) const
	{
		return Find(name) != nullptr;
	}

	const OperationDescriptor* OperationRegistry::Find(std::string_view name) const
	{
		auto existing = m_DescriptorIndex.find(std::string(name));
		if (existing == m_DescriptorIndex.end()) {
			return nullptr;
		}

		return &m_Descriptors[existing->second];
	}
}
