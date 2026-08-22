#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "AssetImporter.h"

namespace HE {
	struct AssetImporterMatch {
		AssetKind Kind = AssetKind::Unknown;
		const AssetImporter* Importer = nullptr;
	};

	class AssetImporterRegistry {
	public:
		[[nodiscard]] bool Register(std::unique_ptr<AssetImporter> importer);
		[[nodiscard]] const AssetImporter* Find(
			AssetKind kind,
			std::string_view extension) const;
		[[nodiscard]] std::optional<AssetImporterMatch> FindByExtension(std::string_view extension) const;

	private:
		std::vector<std::unique_ptr<AssetImporter>> m_Importers;
	};
}
