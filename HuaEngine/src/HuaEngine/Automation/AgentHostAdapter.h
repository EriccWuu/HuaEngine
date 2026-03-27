#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Application/ApplicationOperations.h"

namespace HE {
	struct AgentOperationRequest {
		std::string Operation;
		ResultPayload Arguments;
		std::filesystem::path WorkingDirectory;
	};

	struct AgentOperationResponse {
		ResultEnvelope Result;
		std::vector<OperationDescriptor> Operations;
	};

	class ENGINE_API AgentHostAdapter {
	public:
		explicit AgentHostAdapter(ApplicationOperations& operations);

		[[nodiscard]] AgentOperationResponse Invoke(const AgentOperationRequest& request) const;

	private:
		ApplicationOperations* m_Operations = nullptr;
	};
}
