#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "CLICommandCatalog.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE::CLI {
	struct CLIParsedOptions {
		std::unordered_map<std::string, std::string> Values;
		std::unordered_set<std::string> Flags;

		[[nodiscard]] bool HasFlag(std::string_view key) const;
		[[nodiscard]] std::optional<std::string> GetValue(std::string_view key) const;
	};

	class CLIOptionParser {
	public:
		[[nodiscard]] bool Parse(
			const CLICommandDefinition& command,
			std::span<const std::string> tokens,
			CLIParsedOptions& outOptions,
			ResultEnvelope& outError) const;
	};
}
