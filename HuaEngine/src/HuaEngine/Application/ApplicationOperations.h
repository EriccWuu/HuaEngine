#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "OperationRegistry.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Asset/AssetRegistry.h"

namespace HE {
	class Application;
	class ApplicationServices;
	class Entity;
	class Scene;
	struct ProjectContext;
	struct ProjectStatusReport;
	struct SceneValidationReport;
	struct AssetValidationReport;
	struct ScriptStatusReport;
	struct ValidationReport;

	namespace Rendering {
		class Camera;
		class FrameBuffer;
		class Mesh;
		class Material;
		class Texture2D;
	}

	struct ApplicationValidationRequest {
		const ProjectContext* Project = nullptr;
		const Scene* SceneTarget = nullptr;
		bool IncludeAssets = false;
		Scene* ScriptScene = nullptr;
		bool IncludeScripts = false;
	};

	class ENGINE_API ApplicationOperations {
	public:
		[[nodiscard]] const OperationRegistry& GetRegistry() const { return m_Registry; }
		[[nodiscard]] bool Supports(std::string_view operationName) const { return m_Registry.Contains(operationName); }
		[[nodiscard]] const OperationDescriptor* FindOperation(std::string_view operationName) const { return m_Registry.Find(operationName); }

		[[nodiscard]] ResultEnvelope InitializeProject(
			const std::filesystem::path& rootPath,
			ProjectContext* outContext = nullptr,
			std::string_view projectName = {}) const;

		[[nodiscard]] ResultEnvelope ResolveProjectContext(
			const std::filesystem::path& startingPath,
			ProjectContext& outContext) const;

		[[nodiscard]] ResultEnvelope CheckProjectStatus(
			const ProjectContext& context,
			ProjectStatusReport* outReport = nullptr) const;

		[[nodiscard]] ResultEnvelope CreateScene(std::string_view sceneName, Ref<Scene>& outScene) const;
		[[nodiscard]] ResultEnvelope LoadScene(const std::filesystem::path& scenePath, Ref<Scene>& outScene) const;
		[[nodiscard]] ResultEnvelope SaveScene(const Scene& scene, const std::filesystem::path& scenePath) const;
		[[nodiscard]] ResultEnvelope ValidateScene(const Scene& scene, SceneValidationReport* outReport = nullptr) const;

		[[nodiscard]] ResultEnvelope CreateBuiltinMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			BuiltinMeshPrimitive primitive,
			std::string_view meshName = {},
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::Mesh>& mesh,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope LoadMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterMaterialAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::Material>& material,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope LoadMaterialAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterTextureAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::Texture2D>& texture = nullptr,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const;
		[[nodiscard]] ResultEnvelope ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const;
		[[nodiscard]] ResultEnvelope ResolveTextureAsset(AssetHandle handle, Ref<Rendering::Texture2D>& outTexture) const;
		[[nodiscard]] ResultEnvelope ValidateAssets(const ProjectContext& context, AssetValidationReport* outReport = nullptr) const;

		[[nodiscard]] ResultEnvelope UnbindNativeScript(Entity entity) const;
		[[nodiscard]] ResultEnvelope AttachScriptRuntime(Scene& scene) const;
		[[nodiscard]] ResultEnvelope InitializeSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope UpdateSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope ShutdownSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope CheckSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;

		[[nodiscard]] ResultEnvelope AttachSceneViewportRenderer(
			const Ref<Scene>& scene,
			const Ref<Rendering::FrameBuffer>& framebuffer) const;
		[[nodiscard]] ResultEnvelope RenderSceneViewport(
			Scene& scene,
			Rendering::Camera& camera) const;

		[[nodiscard]] ResultEnvelope Validate(const ApplicationValidationRequest& request, ValidationReport* outReport = nullptr) const;

	private:
		friend class Application;

		explicit ApplicationOperations(ApplicationServices& services);
		void RegisterDefaultOperations();

		ApplicationServices* m_Services = nullptr;
		OperationRegistry m_Registry;
	};
}
