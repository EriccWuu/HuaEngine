#pragma once

#include <filesystem>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	struct ReflectionToolRequest {
		std::filesystem::path RootPath;
		std::filesystem::path ManifestPath;
		std::filesystem::path OutputDirectory;
	};

	class ENGINE_API ReflectionToolService {
	public:
		[[nodiscard]] ResultEnvelope Scan(const ReflectionToolRequest& request) const;
		[[nodiscard]] ResultEnvelope Generate(const ReflectionToolRequest& request) const;
		[[nodiscard]] ResultEnvelope Validate(const ReflectionToolRequest& request) const;

	private:
		[[nodiscard]] ReflectionToolRequest ResolveRequestDefaults(const ReflectionToolRequest& request) const;
	};
}
