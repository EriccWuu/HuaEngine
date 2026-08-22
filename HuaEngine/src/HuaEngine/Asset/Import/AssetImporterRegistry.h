#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "AssetImporter.h"

namespace HE {
	class AssetImporterRegistry {
	public:
		[[nodiscard]] bool Register(std::unique_ptr<AssetImporter> importer);
		[[nodiscard]] const AssetImporter* Find(
			AssetKind kind,
			std::string_view extension) const;

	private:
		std::vector<std::unique_ptr<AssetImporter>> m_Importers;
	};
}
