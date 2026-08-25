#pragma once

#include <string>
#include <string_view>

namespace HE {
	class AssetImportSettings {
	public:
		virtual ~AssetImportSettings() = default;
		[[nodiscard]] virtual std::string_view GetImporterId() const = 0;
	};

	class EmptyAssetImportSettings final : public AssetImportSettings {
	public:
		explicit EmptyAssetImportSettings(std::string importerId)
			: m_ImporterId(std::move(importerId)) {}

		[[nodiscard]] std::string_view GetImporterId() const override { return m_ImporterId; }

	private:
		std::string m_ImporterId;
	};
}
