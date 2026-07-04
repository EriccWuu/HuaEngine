#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "HuaEngine/Core/Core.h"

namespace HE {
	enum class OperationDomain {
		Project,
		Scene,
		Asset,
		Rendering,
		Validation
	};

	struct OperationDescriptor {
		std::string Name;
		OperationDomain Domain = OperationDomain::Project;
		std::string Summary;
	};

	class ENGINE_API OperationRegistry {
	public:
		void Register(OperationDescriptor descriptor);

		[[nodiscard]] bool Contains(std::string_view name) const;
		[[nodiscard]] const OperationDescriptor* Find(std::string_view name) const;
		[[nodiscard]] const std::vector<OperationDescriptor>& List() const { return m_Descriptors; }
		[[nodiscard]] size_t Size() const { return m_Descriptors.size(); }

	private:
		std::vector<OperationDescriptor> m_Descriptors;
		std::unordered_map<std::string, size_t> m_DescriptorIndex;
	};

	[[nodiscard]] constexpr std::string_view ToString(OperationDomain domain) {
		switch (domain) {
		case OperationDomain::Project:
			return "project";
		case OperationDomain::Scene:
			return "scene";
		case OperationDomain::Asset:
			return "asset";
		case OperationDomain::Rendering:
			return "rendering";
		case OperationDomain::Validation:
			return "validation";
		}

		return "unknown";
	}
}
