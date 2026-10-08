#pragma once

#include <filesystem>
#include <string_view>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE::Editor {
	[[nodiscard]] ResultEnvelope ValidateAssetBaseName(std::string_view name);
	[[nodiscard]] std::filesystem::path GenerateUniqueAssetPath(
		const std::filesystem::path& directory,
		std::string_view defaultBaseName,
		std::string_view extension);
}
