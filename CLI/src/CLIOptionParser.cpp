#include "enginepch.h"
#include "CLIOptionParser.h"

#include "CLICommandContext.h"

namespace HE::CLI {
	namespace {
		const CLIOptionDefinition* FindOption(const CLICommandDefinition& command, const std::string& name) {
			for (const auto& option : command.Options) {
				if (option.Name == name) {
					return &option;
				}
			}

			return nullptr;
		}
	}

	bool CLIParsedOptions::HasFlag(std::string_view key) const {
		return Flags.contains(std::string(key));
	}

	std::optional<std::string> CLIParsedOptions::GetValue(std::string_view key) const {
		auto existing = Values.find(std::string(key));
		if (existing == Values.end()) {
			return std::nullopt;
		}

		return existing->second;
	}

	bool CLIOptionParser::Parse(
		const CLICommandDefinition& command,
		std::span<const std::string> tokens,
		CLIParsedOptions& outOptions,
		ResultEnvelope& outError) const {
		CLIParsedOptions parsedOptions;

		for (size_t index = 0; index < tokens.size(); ++index) {
			const auto& token = tokens[index];
			if (!token.starts_with("--")) {
				outError = MakeUsageError(command, "Unexpected positional argument", token);
				return false;
			}

			const auto* option = FindOption(command, token);
			if (!option) {
				outError = MakeUsageError(command, "Unknown option", token);
				return false;
			}

			if (!option->RequiresValue) {
				parsedOptions.Flags.insert(token);
				continue;
			}

			if (index + 1 >= tokens.size()) {
				outError = MakeUsageError(command, "Option requires a value", token);
				return false;
			}

			const auto& value = tokens[++index];
			if (value.starts_with("--")) {
				outError = MakeUsageError(command, "Option requires a value", token);
				return false;
			}

			parsedOptions.Values[token] = value;
		}

		for (const auto& option : command.Options) {
			if (!option.Required) {
				continue;
			}

			const bool isPresent = option.RequiresValue
				? parsedOptions.Values.contains(option.Name)
				: parsedOptions.Flags.contains(option.Name);
			if (!isPresent) {
				outError = MakeUsageError(command, "Missing required option", option.Name);
				return false;
			}
		}

		outOptions = std::move(parsedOptions);
		return true;
	}
}
