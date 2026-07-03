#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CLICommandCatalog.h"
#include "HuaEngine/Application/ApplicationOperations.h"

namespace HE::CLI {
	struct CLICommandResponse {
		ResultEnvelope Result;
		std::vector<OperationDescriptor> Operations;
	};

	class CommandRunner {
	public:
		explicit CommandRunner(ApplicationOperations& operations);

		[[nodiscard]] CLICommandResponse Run(
			const std::vector<std::string>& arguments,
			const std::filesystem::path& workingDirectory) const;

	private:
		ApplicationOperations* m_Operations = nullptr;
		CLICommandCatalog m_Catalog;
	};
}
