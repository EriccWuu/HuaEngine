#include "enginepch.h"
#include "Panels/ProjectAssetNaming.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace HE::Editor {
	ResultEnvelope ValidateAssetBaseName(std::string_view name) {
		if (name.empty() || name == "." || name == ".." || name.back() == ' ' || name.back() == '.') {
			return ResultEnvelope::Failure("asset.name.validate", std::string(name), "Asset name is invalid");
		}

		for (unsigned char character : name) {
			if (character < 32 || character == '<' || character == '>' || character == ':' || character == '"' ||
				character == '/' || character == '\\' || character == '|' || character == '?' || character == '*') {
				return ResultEnvelope::Failure("asset.name.validate", std::string(name), "Asset name contains invalid characters");
			}
		}

		std::string deviceName(name.substr(0, name.find('.')));
		std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(), [](unsigned char character) {
			return static_cast<char>(std::toupper(character));
		});
		static const std::unordered_set<std::string> reservedNames = {
			"CON", "PRN", "AUX", "NUL",
			"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
			"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
		};
		if (reservedNames.contains(deviceName)) {
			return ResultEnvelope::Failure("asset.name.validate", std::string(name), "Asset name is reserved by the operating system");
		}

		return ResultEnvelope::Success("asset.name.validate", std::string(name), "Asset name is valid");
	}

	std::filesystem::path GenerateUniqueAssetPath(
		const std::filesystem::path& directory,
		std::string_view defaultBaseName,
		std::string_view extension) {
		std::string normalizedExtension(extension);
		if (!normalizedExtension.empty() && normalizedExtension.front() != '.') {
			normalizedExtension.insert(normalizedExtension.begin(), '.');
		}

		for (uint32_t suffix = 0;; ++suffix) {
			std::string baseName(defaultBaseName);
			if (suffix > 0) {
				baseName += " " + std::to_string(suffix);
			}
			const auto candidate = directory / (baseName + normalizedExtension);
			std::error_code errorCode;
			const bool sourceExists = std::filesystem::exists(candidate, errorCode);
			if (errorCode) continue;
			const bool metaExists = std::filesystem::exists(GetAssetMetaPath(candidate), errorCode);
			if (!errorCode && !sourceExists && !metaExists) {
				return candidate;
			}
		}
	}
}
