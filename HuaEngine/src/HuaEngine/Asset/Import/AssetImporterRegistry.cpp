#include "enginepch.h"
#include "AssetImporterRegistry.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {
	std::string NormalizeExtension(std::string_view extension) {
		std::string normalized(extension);
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) {
			return static_cast<char>(std::tolower(value));
		});
		return normalized;
	}
}

namespace HE {
	bool AssetImporterRegistry::Register(std::unique_ptr<AssetImporter> importer) {
		if (!importer || importer->GetId().empty() || importer->GetVersion() == 0 || importer->GetArtifactVersion() == 0) {
			return false;
		}

		const auto duplicate = std::find_if(m_Importers.begin(), m_Importers.end(), [&](const auto& existing) {
			return existing->GetId() == importer->GetId();
		});
		if (duplicate != m_Importers.end()) {
			return false;
		}

		m_Importers.push_back(std::move(importer));
		return true;
	}

	const AssetImporter* AssetImporterRegistry::Find(
		AssetKind kind,
		std::string_view extension) const {
		const auto normalizedExtension = NormalizeExtension(extension);
		const auto it = std::find_if(m_Importers.begin(), m_Importers.end(), [&](const auto& importer) {
			return importer->CanImport(kind, normalizedExtension);
		});
		return it != m_Importers.end() ? it->get() : nullptr;
	}

	std::optional<AssetImporterMatch> AssetImporterRegistry::FindByExtension(std::string_view extension) const {
		constexpr std::array kinds = {
			AssetKind::Mesh,
			AssetKind::Material,
			AssetKind::Texture2D,
			AssetKind::Shader
		};
		for (const AssetKind kind : kinds) {
			if (const AssetImporter* importer = Find(kind, extension)) {
				return AssetImporterMatch{ kind, importer };
			}
		}
		return std::nullopt;
	}
}
