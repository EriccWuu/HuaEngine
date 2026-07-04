#include "enginepch.h"
#include "CLICommandRunner.h"

#include <algorithm>
#include <span>

#include "CLIAssetCommands.h"
#include "CLICommandContext.h"
#include "CLIMetaCommands.h"
#include "CLIOptionParser.h"
#include "CLIProjectCommands.h"
#include "CLIReflectionCommands.h"
#include "CLISceneCommands.h"
#include "CLIValidationCommands.h"

namespace HE::CLI {
	CommandRunner::CommandRunner(ApplicationOperations& operations)
		: m_Operations(&operations)
	{
	}

	CLICommandResponse CommandRunner::Run(
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory) const {
		if (!m_Operations) {
			return { MakeHostFailure("cli.host", "Application operations are not available") };
		}

		if (arguments.empty()) {
			return { MakeUsageError("No command specified") };
		}

		const auto match = m_Catalog.Match(arguments);
		if (!match.Matched()) {
			return { MakeUsageError("Unknown command", arguments.front()) };
		}

		const auto optionStartIndex = match.MatchedTokenCount;
		const std::span<const std::string> optionTokens(
			arguments.data() + std::min(optionStartIndex, arguments.size()),
			arguments.size() > optionStartIndex ? arguments.size() - optionStartIndex : 0);

		CLIParsedOptions options;
		ResultEnvelope optionError;
		CLIOptionParser optionParser;
		if (!optionParser.Parse(*match.Command, optionTokens, options, optionError)) {
			return { std::move(optionError) };
		}

		CLICommandContext context{
			*m_Operations,
			NormalizePath(workingDirectory),
			m_Catalog
		};

		switch (match.Command->Domain) {
		case CLICommandDomain::CLI:
		case CLICommandDomain::Operations:
			return RunMetaCommand(*match.Command, options, context);
		case CLICommandDomain::Project:
			return RunProjectCommand(*match.Command, options, context);
		case CLICommandDomain::Scene:
			return RunSceneCommand(*match.Command, options, context);
		case CLICommandDomain::Asset:
			return RunAssetCommand(*match.Command, options, context);
		case CLICommandDomain::Reflection:
			return RunReflectionCommand(*match.Command, options, context);
		case CLICommandDomain::Validation:
			return RunValidationCommand(*match.Command, options, context);
		}

		return { MakeUsageError("Unknown command", arguments.front()) };
	}
}
