#include "enginepch.h"
#include "CLIMetaCommands.h"

#include <sstream>

namespace HE::CLI {
	namespace {
		std::string JoinPath(const std::vector<std::string>& path) {
			std::ostringstream output;
			for (size_t index = 0; index < path.size(); ++index) {
				if (index > 0) {
					output << ' ';
				}
				output << path[index];
			}
			return output.str();
		}
	}

	CLICommandResponse RunMetaCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions&,
		CLICommandContext& context) {
		if (command.Path == std::vector<std::string>{ "help" }) {
			auto result = ResultEnvelope::Success("cli.help", "command_line", "CLI command help");
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.summary", "Use 'ops list' to inspect the formal operation registry", {} });

			for (const auto& catalogCommand : context.Catalog.Commands()) {
				result.AddDetail({
					DiagnosticSeverity::Info,
					"cli.help.command",
					JoinPath(catalogCommand.Path) + " - " + catalogCommand.Summary,
					catalogCommand.Usage
				});
			}

			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "ops", "list" }) {
			auto result = ResultEnvelope::Success("cli.ops_list", "operation_registry", "Formal operation registry listed");
			result.SetPayloadValue("operation_count", std::to_string(context.Operations.GetOperationRegistry().Size()));
			return { std::move(result), context.Operations.GetOperationRegistry().List() };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
