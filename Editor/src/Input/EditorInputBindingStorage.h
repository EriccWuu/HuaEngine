#pragma once

#include <filesystem>
#include <vector>

#include "EditorInputBindingRegistry.h"

namespace HE::Editor {
	class EditorInputBindingStorage {
	public:
		[[nodiscard]] static std::filesystem::path GetDefaultPath();
		static ResultEnvelope Save(const std::filesystem::path& path, const std::vector<EditorInputBindingOverride>& overrides);
		static ResultEnvelope Load(const std::filesystem::path& path, std::vector<EditorInputBindingOverride>& overrides);
	};
}
