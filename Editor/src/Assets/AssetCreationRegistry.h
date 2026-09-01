#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE::Editor {
	struct AssetCreationDescriptor {
		std::string TypeId;
		AssetKind Kind = AssetKind::Unknown;
		std::string MenuLabel;
		std::string DefaultBaseName;
		std::string Extension;
	};

	class AssetCreationRegistry {
	public:
		[[nodiscard]] ResultEnvelope Register(AssetCreationDescriptor descriptor);
		[[nodiscard]] const AssetCreationDescriptor* Find(std::string_view typeId) const;
		[[nodiscard]] std::span<const AssetCreationDescriptor> GetAll() const { return m_Descriptors; }

	private:
		std::vector<AssetCreationDescriptor> m_Descriptors;
		std::unordered_map<std::string, size_t> m_DescriptorIndex;
	};
}
