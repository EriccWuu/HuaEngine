#include "enginepch.h"
#include "Assets/AssetCreationRegistry.h"

namespace HE::Editor {
	ResultEnvelope AssetCreationRegistry::Register(AssetCreationDescriptor descriptor) {
		if (descriptor.TypeId.empty() || descriptor.Kind == AssetKind::Unknown || descriptor.MenuLabel.empty() ||
			descriptor.DefaultBaseName.empty() || descriptor.Extension.empty() || descriptor.Extension.front() != '.') {
			return ResultEnvelope::Failure("asset.creation.register", descriptor.TypeId, "Asset creation descriptor is incomplete");
		}
		if (m_DescriptorIndex.contains(descriptor.TypeId)) {
			return ResultEnvelope::Failure("asset.creation.register", descriptor.TypeId, "Asset creation type is already registered");
		}

		m_DescriptorIndex.emplace(descriptor.TypeId, m_Descriptors.size());
		m_Descriptors.push_back(std::move(descriptor));
		return ResultEnvelope::Success("asset.creation.register", m_Descriptors.back().TypeId, "Asset creation type registered");
	}

	const AssetCreationDescriptor* AssetCreationRegistry::Find(std::string_view typeId) const {
		const auto found = m_DescriptorIndex.find(std::string(typeId));
		return found != m_DescriptorIndex.end() ? &m_Descriptors[found->second] : nullptr;
	}
}
