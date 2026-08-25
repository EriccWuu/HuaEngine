#include "AssetPickerModel.h"

#include <algorithm>
#include <cctype>

namespace HE::Editor {
	namespace {
		char ToLowerAscii(char value) {
			return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
		}

		bool CaseInsensitiveLess(std::string_view left, std::string_view right) {
			return std::lexicographical_compare(
				left.begin(),
				left.end(),
				right.begin(),
				right.end(),
				[](char leftCharacter, char rightCharacter) {
					return ToLowerAscii(leftCharacter) < ToLowerAscii(rightCharacter);
				});
		}

		bool ContainsCaseInsensitive(std::string_view text, std::string_view filter) {
			if (filter.empty()) return true;
			return std::search(
				text.begin(),
				text.end(),
				filter.begin(),
				filter.end(),
				[](char textCharacter, char filterCharacter) {
					return ToLowerAscii(textCharacter) == ToLowerAscii(filterCharacter);
				}) != text.end();
		}
	}

	std::vector<AssetPickerOption> BuildAssetPickerOptions(
		std::span<const AssetRecord> records,
		AssetKind kind) {
		std::vector<AssetPickerOption> options;
		options.reserve(records.size());
		for (const AssetRecord& record : records) {
			if (record.Kind == kind && !record.Guid.empty() && !record.AssetId.empty()) {
				options.push_back({ record.Guid, record.AssetId });
			}
		}

		std::sort(options.begin(), options.end(), [](const AssetPickerOption& left, const AssetPickerOption& right) {
			if (CaseInsensitiveLess(left.DisplayName, right.DisplayName)) return true;
			if (CaseInsensitiveLess(right.DisplayName, left.DisplayName)) return false;
			return left.Guid < right.Guid;
		});
		return options;
	}

	AssetPickerPreview GetAssetPickerPreview(
		std::span<const AssetPickerOption> options,
		const AssetGuid& guid) {
		if (guid.empty()) return { "None", false };

		const auto iterator = std::find_if(options.begin(), options.end(), [&](const AssetPickerOption& option) {
			return option.Guid == guid;
		});
		if (iterator != options.end()) return { iterator->DisplayName, false };
		return { "Missing: " + guid, true };
	}

	bool AssetPickerOptionMatches(const AssetPickerOption& option, std::string_view filter) {
		return ContainsCaseInsensitive(option.DisplayName, filter);
	}
}
