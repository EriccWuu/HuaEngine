#include "enginepch.h"
#include "AssetImportFingerprint.h"

#include <algorithm>
#include <set>

#include "HuaEngine/Asset/Library/AssetBinaryIO.h"
#include "HuaEngine/Core/Sha256.h"

namespace {
	bool IsDigest(std::string_view value) {
		return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char character) {
			return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
		});
	}

	bool IsSafeNormalizedPath(std::string_view value) {
		if (value.empty() || value.starts_with('/') || value.find('\\') != std::string_view::npos) return false;
		const std::filesystem::path path(value);
		if (path.is_absolute() || path.has_root_name() || path.lexically_normal().generic_string() != value) return false;
		return std::none_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; });
	}
}

namespace HE {
	ResultEnvelope ComputeAssetImportFingerprint(const AssetImportFingerprintInput& input, std::string& outFingerprint) {
		outFingerprint.clear();
		if (input.ImporterId.empty() || input.ImporterVersion == 0 || input.ArtifactVersion == 0 || input.Sources.empty()) {
			return ResultEnvelope::Failure("asset.import_fingerprint", input.ImporterId, "Fingerprint identity is incomplete");
		}

		auto sources = input.Sources;
		auto dependencies = input.Dependencies;
		auto options = input.Options;
		std::sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) { return a.NormalizedPath < b.NormalizedPath; });
		std::sort(dependencies.begin(), dependencies.end(), [](const auto& a, const auto& b) { return a.Guid < b.Guid; });
		std::sort(options.begin(), options.end());
		std::set<std::string> identities;
		for (const auto& source : sources) {
			if (!IsSafeNormalizedPath(source.NormalizedPath) || !IsDigest(source.ContentHash) || !identities.emplace(source.NormalizedPath).second) {
				return ResultEnvelope::Failure("asset.import_fingerprint", input.ImporterId, "Fingerprint source input is invalid");
			}
		}
		identities.clear();
		for (const auto& dependency : dependencies) {
			if (dependency.Guid.empty() || !IsDigest(dependency.ArtifactDigest) || !identities.emplace(dependency.Guid).second) {
				return ResultEnvelope::Failure("asset.import_fingerprint", input.ImporterId, "Fingerprint dependency input is invalid");
			}
		}
		identities.clear();
		for (const auto& [name, value] : options) {
			if (name.empty() || !identities.emplace(name).second) return ResultEnvelope::Failure("asset.import_fingerprint", input.ImporterId, "Fingerprint option is invalid");
		}

		AssetBinaryWriter writer;
		writer.WriteU32(1);
		writer.WriteString(input.ImporterId);
		writer.WriteU32(input.ImporterVersion);
		writer.WriteU32(input.ArtifactVersion);
		writer.WriteU32(static_cast<uint32_t>(sources.size()));
		for (const auto& source : sources) { writer.WriteString(source.NormalizedPath); writer.WriteString(source.ContentHash); }
		writer.WriteU32(static_cast<uint32_t>(dependencies.size()));
		for (const auto& dependency : dependencies) { writer.WriteString(dependency.Guid); writer.WriteString(dependency.ArtifactDigest); }
		writer.WriteU32(static_cast<uint32_t>(options.size()));
		for (const auto& [name, value] : options) { writer.WriteString(name); writer.WriteString(value); }
		outFingerprint = Sha256ToHex(ComputeSha256(writer.GetData()));
		auto result = ResultEnvelope::Success("asset.import_fingerprint", input.ImporterId, "Import fingerprint computed");
		result.SetPayloadValue("sha256", outFingerprint);
		return result;
	}
}
