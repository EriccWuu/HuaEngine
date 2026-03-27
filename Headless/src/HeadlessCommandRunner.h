#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Application/ApplicationOperations.h"

namespace HE::Headless {
	struct HeadlessCommandResponse {
		ResultEnvelope Result;
		std::vector<OperationDescriptor> Operations;
	};

	class CommandRunner {
	public:
		explicit CommandRunner(ApplicationOperations& operations);

		[[nodiscard]] HeadlessCommandResponse Run(
			const std::vector<std::string>& arguments,
			const std::filesystem::path& workingDirectory) const;

	private:
		ApplicationOperations* m_Operations = nullptr;
	};
}
