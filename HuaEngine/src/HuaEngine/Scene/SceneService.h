#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE {
	struct SceneValidationReport {
		bool HasName = true;
		uint32_t EntityCount = 0;
		uint32_t EntitiesMissingTransform = 0;
		uint32_t RenderEntitiesMissingMaterial = 0;
		uint32_t RenderEntitiesMissingMesh = 0;
		uint32_t EntitiesUsingLegacyRenderer = 0;

		[[nodiscard]] bool IsOperational() const;
		[[nodiscard]] bool HasIssues() const;
	};

	class ENGINE_API SceneService {
	public:
		[[nodiscard]] ResultEnvelope CreateScene(std::string_view sceneName, Ref<Scene>& outScene) const;
		[[nodiscard]] ResultEnvelope LoadScene(const std::filesystem::path& scenePath, Ref<Scene>& outScene) const;
		[[nodiscard]] ResultEnvelope SaveScene(const Scene& scene, const std::filesystem::path& scenePath) const;
		[[nodiscard]] ResultEnvelope ValidateScene(const Scene& scene, SceneValidationReport* outReport = nullptr) const;
	};
}
