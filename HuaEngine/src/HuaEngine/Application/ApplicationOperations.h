#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "OperationRegistry.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Asset/AssetRegistry.h"
#include "HuaEngine/Reflection/ReflectionToolService.h"

namespace HE {
	class Application;
	class ApplicationServices;
	class Entity;
	class Scene;
	struct ProjectContext;
	struct ProjectStatusReport;
	struct SceneValidationReport;
	struct AssetValidationReport;
	struct TransformComponent;
	struct ValidationReport;

	namespace Rendering {
		class RenderCamera;
		class RenderTarget;
		class Mesh;
		class Material;
		class TextureResource;
		class RenderGraphExtension;
		struct CameraComponent;
		struct MaterialComponent;
		struct MeshComponent;
	}

	enum class SceneComponentKind {
		Camera,
		Mesh,
		Material
	};

	[[nodiscard]] constexpr std::string_view ToString(SceneComponentKind componentKind) {
		switch (componentKind) {
		case SceneComponentKind::Camera:
			return "camera";
		case SceneComponentKind::Mesh:
			return "mesh";
		case SceneComponentKind::Material:
			return "material";
		}

		return "unknown";
	}

	struct ApplicationValidationRequest {
		const ProjectContext* Project = nullptr;
		const Scene* SceneTarget = nullptr;
		bool IncludeAssets = false;
	};

	class ENGINE_API ApplicationOperations {
	public:
		[[nodiscard]] const OperationRegistry& GetOperationRegistry() const { return m_Registry; }
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
		[[nodiscard]] ResultEnvelope CreateSceneEntity(
			Scene& scene,
			std::string_view entityName,
			uint32_t* outEntityId = nullptr) const;
		[[nodiscard]] ResultEnvelope DeleteSceneEntities(
			Scene& scene,
			std::span<const uint32_t> entityIds,
			uint32_t* outDeletedCount = nullptr) const;
		[[nodiscard]] ResultEnvelope UpsertSceneEntityName(
			Scene& scene,
			uint32_t entityId,
			std::string_view name) const;
		[[nodiscard]] ResultEnvelope UpsertSceneEntityTransform(
			Scene& scene,
			uint32_t entityId,
			const TransformComponent& component) const;
		[[nodiscard]] ResultEnvelope AddSceneComponent(
			Scene& scene,
			uint32_t entityId,
			SceneComponentKind componentKind) const;
		[[nodiscard]] ResultEnvelope RemoveSceneComponent(
			Scene& scene,
			uint32_t entityId,
			SceneComponentKind componentKind) const;
		[[nodiscard]] ResultEnvelope UpsertSceneCameraComponent(
			Scene& scene,
			uint32_t entityId,
			const Rendering::CameraComponent& component) const;
		[[nodiscard]] ResultEnvelope UpsertSceneMeshComponent(
			Scene& scene,
			uint32_t entityId,
			const Rendering::MeshComponent& component) const;
		[[nodiscard]] ResultEnvelope UpsertSceneMaterialComponent(
			Scene& scene,
			uint32_t entityId,
			const Rendering::MaterialComponent& component) const;

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
			const Ref<Rendering::TextureResource>& texture = nullptr,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope InitializeAssetManifest(const ProjectContext& context) const;
		[[nodiscard]] ResultEnvelope ImportAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetKind kind,
			AssetGuid* outGuid = nullptr) const;
		[[nodiscard]] ResultEnvelope ListAssets(
			const ProjectContext& context,
			std::vector<AssetRecord>& outRecords) const;

		[[nodiscard]] ResultEnvelope ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAssetByGuid(
			const ProjectContext& context,
			const AssetGuid& guid,
			AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const;
		[[nodiscard]] ResultEnvelope ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const;
		[[nodiscard]] ResultEnvelope ResolveTextureAsset(AssetHandle handle, Ref<Rendering::TextureResource>& outTexture) const;
		[[nodiscard]] ResultEnvelope ValidateAssets(const ProjectContext& context, AssetValidationReport* outReport = nullptr) const;

		[[nodiscard]] ResultEnvelope AttachSceneViewportRenderer(
			const Ref<Scene>& scene,
			const Ref<Rendering::RenderTarget>& renderTarget) const;
		[[nodiscard]] ResultEnvelope RenderSceneViewport(
			Scene& scene,
			const Rendering::RenderCamera& camera,
			Rendering::RenderGraphExtension* extension = nullptr) const;

		[[nodiscard]] ResultEnvelope Validate(const ApplicationValidationRequest& request, ValidationReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope ScanReflection(const ReflectionToolRequest& request) const;
		[[nodiscard]] ResultEnvelope GenerateReflection(const ReflectionToolRequest& request) const;
		[[nodiscard]] ResultEnvelope ValidateReflection(const ReflectionToolRequest& request) const;

	private:
		friend class Application;

		explicit ApplicationOperations(ApplicationServices& services);
		void RegisterDefaultOperations();

		ApplicationServices* m_Services = nullptr;
		OperationRegistry m_Registry;
	};
}
