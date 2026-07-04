#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HE::CLI {
	enum class CLICommandDomain {
		CLI,
		Operations,
		Project,
		Scene,
		Asset,
		Reflection,
		Validation
	};

	struct CLIOptionDefinition {
		std::string Name;
		bool RequiresValue = true;
		bool Required = false;
		std::string Summary;
	};

	struct CLICommandDefinition {
		std::vector<std::string> Path;
		CLICommandDomain Domain = CLICommandDomain::CLI;
		std::string FormalOperation;
		std::string Summary;
		std::string Usage;
		std::vector<CLIOptionDefinition> Options;
	};

	struct CLICommandMatch {
		const CLICommandDefinition* Command = nullptr;
		size_t MatchedTokenCount = 0;

		[[nodiscard]] bool Matched() const {
			return Command != nullptr;
		}
	};

	class CLICommandCatalog {
	public:
		CLICommandCatalog();

		[[nodiscard]] CLICommandMatch Match(std::span<const std::string> tokens) const;
		[[nodiscard]] const CLICommandDefinition* Find(std::span<const std::string_view> path) const;
		[[nodiscard]] const CLICommandDefinition* Find(std::initializer_list<std::string_view> path) const;
		[[nodiscard]] const std::vector<CLICommandDefinition>& Commands() const {
			return m_Commands;
		}

	private:
		void Register(CLICommandDefinition command);

		std::vector<CLICommandDefinition> m_Commands;
	};
}
