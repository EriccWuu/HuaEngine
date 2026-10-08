#include "enginepch.h"
#include "ApplicationOperations.h"

#include "ApplicationServices.h"
#include "SceneAssetCreationCleanup.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneService.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Asset/AssetSourcePath.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Validation/ValidationService.h"
#include "EcsResultEnvelope.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	std::string MakeSceneEntityTarget(const HE::Scene& scene, uint32_t entityId) {
		return scene.GetName() + "#entity:" + std::to_string(entityId);
	}

	HE::ResultEnvelope MakeSceneEntityMissingResult(
		std::string operation,
		const HE::Scene& scene,
		uint32_t entityId,
		std::string summary) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), MakeSceneEntityTarget(scene, entityId), std::move(summary));
		result.AddDetail({
			HE::DiagnosticSeverity::Error,
			"scene.entity.missing",
			"The target entity does not exist in the active scene",
			std::to_string(entityId)
		});
		return result;
	}

    template<typename Component>
    HE::ResultEnvelope UpsertSceneComponent(HE::Scene& scene, uint32_t index,
        const Component& component, std::string operation, std::string summary, std::string_view kind = {}) {
        auto scope = HE::Ecs::WorldEditScope::Acquire(scene.GetWorld());
        if (!scope) return HE::EcsFailureEnvelope(operation, MakeSceneEntityTarget(scene, index), scope.GetError());
        auto& world = scope.Value().Get();
        const auto entity = world.FindByIndex(index);
        if (!world.IsAlive(entity)) return MakeSceneEntityMissingResult(operation, scene, index, "Scene entity does not exist");
        auto edited = world.Emplace<Component>(entity, component);
        if (!edited) return HE::EcsFailureEnvelope(operation, MakeSceneEntityTarget(scene, index), edited.GetError());
        auto result = HE::ResultEnvelope::Success(std::move(operation), MakeSceneEntityTarget(scene, index), std::move(summary));
        result.SetPayloadValue("entity_id", std::to_string(index));
        if (!kind.empty()) result.SetPayloadValue("component_kind", std::string(kind));
        return result;
    }

	HE::Rendering::CameraComponent MakeDefaultCameraComponent() {
		HE::Rendering::CameraComponent component;
		component.Primary = true;
		component.FixedAspectRatio = false;
		return component;
	}

	HE::Rendering::MeshComponent MakeDefaultMeshComponent() {
		HE::Rendering::MeshComponent component;
		component.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
		return component;
	}

	HE::Rendering::MaterialComponent MakeDefaultMaterialComponent() {
		HE::Rendering::MaterialComponent component;
		component.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		return component;
	}

	std::string ToRenderingGraphDetailCode(HE::Rendering::RenderGraphResultDiagnosticCode code) {
		switch (code) {
			case HE::Rendering::RenderGraphResultDiagnosticCode::EmptyGraph:
				return "rendering.graph.empty_graph";
			case HE::Rendering::RenderGraphResultDiagnosticCode::EmptyPassName:
				return "rendering.graph.empty_pass_name";
			case HE::Rendering::RenderGraphResultDiagnosticCode::DuplicatePassName:
				return "rendering.graph.duplicate_pass_name";
			case HE::Rendering::RenderGraphResultDiagnosticCode::MissingExecuteCallback:
				return "rendering.graph.missing_execute_callback";
			case HE::Rendering::RenderGraphResultDiagnosticCode::EmptyResourceName:
				return "rendering.graph.empty_resource";
			case HE::Rendering::RenderGraphResultDiagnosticCode::DuplicateResourceAccess:
				return "rendering.graph.duplicate_resource_access";
			case HE::Rendering::RenderGraphResultDiagnosticCode::MissingResourceProducer:
				return "rendering.graph.missing_resource_producer";
			case HE::Rendering::RenderGraphResultDiagnosticCode::DuplicateResourceWriter:
				return "rendering.graph.duplicate_resource_writer";
		}

		return "rendering.graph.unknown";
	}

	void AddRenderGraphDetails(HE::ResultEnvelope& result, const HE::Rendering::RenderResult& renderResult) {
		for (const auto& diagnostic : renderResult.GraphDiagnostics) {
			result.AddDetail({
				HE::DiagnosticSeverity::Error,
				ToRenderingGraphDetailCode(diagnostic.Code),
				diagnostic.Message,
				diagnostic.PassName
			});
		}
	}

	bool TryResolveNewSceneAssetPath(
		const HE::ProjectContext& context,
		const std::filesystem::path& requestedPath,
		std::filesystem::path& outPath,
		HE::ResultEnvelope& outError) {
		if (!context.IsLoaded()) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", requestedPath.generic_string(), "A loaded project context is required");
			return false;
		}
		if (requestedPath.empty()) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", {}, "Scene asset path is empty");
			return false;
		}

		std::error_code errorCode;
		const auto assetRoot = std::filesystem::weakly_canonical(context.GetAssetRootPath(), errorCode);
		if (errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", requestedPath.generic_string(), "Project asset root could not be resolved");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			return false;
		}

		const auto candidate = requestedPath.is_absolute() ? requestedPath : assetRoot / requestedPath;
		outPath = std::filesystem::weakly_canonical(candidate, errorCode);
		const auto relativePath = outPath.lexically_relative(assetRoot);
		if (errorCode || !HE::IsSafeAssetRelativePath(relativePath)) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", requestedPath.generic_string(), "Scene asset path must remain under the project Assets directory");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.path.outside_root", errorCode ? errorCode.message() : "Path escapes the asset root", requestedPath.generic_string() });
			return false;
		}
		if (outPath.extension() != ".scene") {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", relativePath.generic_string(), "Scene asset path must use the .scene extension");
			return false;
		}

		errorCode.clear();
		const auto metaPath = HE::GetAssetMetaPath(outPath);
		if (std::filesystem::exists(outPath, errorCode) || std::filesystem::exists(metaPath, errorCode)) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", relativePath.generic_string(), "Scene asset target already exists");
			return false;
		}
		if (errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.scene.create", relativePath.generic_string(), "Scene asset target could not be inspected");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.scene.target_inspection_failed", errorCode.message(), outPath.generic_string() });
			return false;
		}
		return true;
	}
}

namespace HE {
	ApplicationOperations::ApplicationOperations(ApplicationServices& services)
		: m_Services(&services)
	{
		RegisterDefaultOperations();
	}

	ResultEnvelope ApplicationOperations::InitializeProject(
		const std::filesystem::path& rootPath,
		ProjectContext* outContext,
		std::string_view projectName) const
	{
		return m_Services->Projects().InitializeProject(rootPath, outContext, projectName);
	}

	ResultEnvelope ApplicationOperations::ResolveProjectContext(
		const std::filesystem::path& startingPath,
		ProjectContext& outContext) const
	{
		return m_Services->Projects().ResolveProjectContext(startingPath, outContext);
	}

	ResultEnvelope ApplicationOperations::CheckProjectStatus(
		const ProjectContext& context,
		ProjectStatusReport* outReport) const
	{
		return m_Services->Projects().CheckProjectStatus(context, outReport);
	}

	ResultEnvelope ApplicationOperations::CreateScene(std::string_view sceneName, Ref<Scene>& outScene) const
	{
		return m_Services->Scenes().CreateScene(sceneName, outScene);
	}

	ResultEnvelope ApplicationOperations::LoadScene(const std::filesystem::path& scenePath, Ref<Scene>& outScene) const
	{
		return m_Services->Scenes().LoadScene(scenePath, outScene);
	}

	ResultEnvelope ApplicationOperations::SaveScene(const Scene& scene, const std::filesystem::path& scenePath) const
	{
		return m_Services->Scenes().SaveScene(scene, scenePath);
	}

	ResultEnvelope ApplicationOperations::ValidateScene(const Scene& scene, SceneValidationReport* outReport) const
	{
		return m_Services->Scenes().ValidateScene(scene, outReport);
	}

    ResultEnvelope ApplicationOperations::CreateSceneEntity(Scene& scene, std::string_view name, uint32_t* outEntityId, EntityUuid uuid) const {
        auto created = scene.CreateEntity(name, uuid);
        if (!created) return EcsFailureEnvelope("scene.entity.create", scene.GetName(), created.GetError());
        const auto index = created.Value().Index;
        if (outEntityId) *outEntityId = index;
        auto result = ResultEnvelope::Success("scene.entity.create", MakeSceneEntityTarget(scene, index), "Scene entity created");
        result.SetPayloadValue("scene_name", scene.GetName());
        result.SetPayloadValue("entity_id", std::to_string(index));
        result.SetPayloadValue("entity_name", std::string(scene.GetWorld().Name(created.Value())));
        return result;
    }

    ResultEnvelope ApplicationOperations::DeleteSceneEntities(Scene& scene, std::span<const uint32_t> indices, uint32_t* outDeletedCount) const {
        if (outDeletedCount) *outDeletedCount = 0;
        if (indices.empty()) {
            auto result = ResultEnvelope::Failure("scene.entity.delete", scene.GetName(), "At least one scene entity id is required");
            result.AddDetail({DiagnosticSeverity::Error, "scene.entity.delete.empty", "DeleteSceneEntities requires at least one entity id", {}});
            return result;
        }
        auto scope = Ecs::WorldEditScope::Acquire(scene.GetWorld());
        if (!scope) return EcsFailureEnvelope("scene.entity.delete", scene.GetName(), scope.GetError());
        auto& world = scope.Value().Get();
        uint32_t count = 0;
        for (const auto index : indices) {
            const auto entity = world.FindByIndex(index);
            if (!world.IsAlive(entity)) continue;
            auto removed = world.Destroy(entity);
            if (!removed) {
                auto result = EcsFailureEnvelope("scene.entity.delete", scene.GetName(), removed.GetError());
                result.SetPayloadValue("deleted_count", std::to_string(count));
                return result;
            }
            ++count;
            if (outDeletedCount) *outDeletedCount = count;
        }
        if (count == 0) {
            auto result = ResultEnvelope::Failure("scene.entity.delete", scene.GetName(), "No matching scene entities were deleted");
            result.AddDetail({DiagnosticSeverity::Error, "scene.entity.delete.none_deleted", "None of the requested entity ids resolved to live scene entities", {}});
            return result;
        }
        auto result = ResultEnvelope::Success("scene.entity.delete", scene.GetName(), count > 1 ? "Scene entities deleted" : "Scene entity deleted");
        result.SetPayloadValue("scene_name", scene.GetName());
        result.SetPayloadValue("deleted_count", std::to_string(count));
        return result;
    }

    ResultEnvelope ApplicationOperations::UpsertSceneEntityName(Scene& scene, uint32_t index, std::string_view name) const {
        auto scope = Ecs::WorldEditScope::Acquire(scene.GetWorld());
        if (!scope) return EcsFailureEnvelope("scene.entity.upsert_name", MakeSceneEntityTarget(scene, index), scope.GetError());
        auto& world = scope.Value().Get();
        const auto entity = world.FindByIndex(index);
        if (!world.IsAlive(entity)) return MakeSceneEntityMissingResult("scene.entity.upsert_name", scene, index, "Scene entity does not exist");
        auto edited = world.SetName(entity, name);
        if (!edited) return EcsFailureEnvelope("scene.entity.upsert_name", MakeSceneEntityTarget(scene, index), edited.GetError());
        auto result = ResultEnvelope::Success("scene.entity.upsert_name", MakeSceneEntityTarget(scene, index), "Scene entity name updated");
        result.SetPayloadValue("entity_id", std::to_string(index));
        result.SetPayloadValue("entity_name", std::string(world.Name(entity)));
        return result;
    }

    ResultEnvelope ApplicationOperations::UpsertSceneEntityTransform(Scene& scene, uint32_t index, const TransformComponent& component) const {
        return UpsertSceneComponent(scene, index, component, "scene.entity.upsert_transform", "Scene entity transform updated");
    }

    ResultEnvelope ApplicationOperations::AddSceneComponent(Scene& scene, uint32_t index, SceneComponentKind kind) const {
        // End the inspection scope before the public mutation acquires its edit scope.
        {
            auto scope = Ecs::WorldReadScope::Acquire(scene.GetWorld());
            if (!scope) return EcsFailureEnvelope("scene.component.add", MakeSceneEntityTarget(scene, index), scope.GetError());
            const auto& world = scope.Value().Get();
            const auto entity = world.FindByIndex(index);
            if (!world.IsAlive(entity)) return MakeSceneEntityMissingResult("scene.component.add", scene, index, "Scene entity does not exist");
            bool exists = false;
            switch (kind) {
            case SceneComponentKind::Camera: exists = world.Has<Rendering::CameraComponent>(entity); break;
            case SceneComponentKind::Mesh: exists = world.Has<Rendering::MeshComponent>(entity); break;
            case SceneComponentKind::Material: exists = world.Has<Rendering::MaterialComponent>(entity); break;
            }
            if (exists) return ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, index), "The component already exists on the target entity");
        }
        switch (kind) {
        case SceneComponentKind::Camera: return UpsertSceneCameraComponent(scene, index, MakeDefaultCameraComponent());
        case SceneComponentKind::Mesh: return UpsertSceneMeshComponent(scene, index, MakeDefaultMeshComponent());
        case SceneComponentKind::Material: return UpsertSceneMaterialComponent(scene, index, MakeDefaultMaterialComponent());
        }
        auto result = ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, index), "Unsupported scene component kind");
        result.AddDetail({DiagnosticSeverity::Error, "scene.component.add.unsupported", "The requested scene component kind is not supported", std::string(ToString(kind))});
        return result;
    }

    ResultEnvelope ApplicationOperations::RemoveSceneComponent(Scene& scene, uint32_t index, SceneComponentKind kind) const {
        auto scope = Ecs::WorldEditScope::Acquire(scene.GetWorld());
        if (!scope) return EcsFailureEnvelope("scene.component.remove", MakeSceneEntityTarget(scene, index), scope.GetError());
        auto& world = scope.Value().Get();
        const auto entity = world.FindByIndex(index);
        if (!world.IsAlive(entity)) return MakeSceneEntityMissingResult("scene.component.remove", scene, index, "Scene entity does not exist");
        const Ecs::RegisteredType* type = nullptr;
        switch (kind) {
        case SceneComponentKind::Camera: type = world.Types().Find<Rendering::CameraComponent>(); break;
        case SceneComponentKind::Mesh: type = world.Types().Find<Rendering::MeshComponent>(); break;
        case SceneComponentKind::Material: type = world.Types().Find<Rendering::MaterialComponent>(); break;
        }
        if (!type || !world.Has(entity, type->Id)) return ResultEnvelope::Failure("scene.component.remove", MakeSceneEntityTarget(scene, index), "The component is not present on the target entity");
        auto removed = world.Remove(entity, type->Id);
        if (!removed) return EcsFailureEnvelope("scene.component.remove", MakeSceneEntityTarget(scene, index), removed.GetError());
        auto result = ResultEnvelope::Success("scene.component.remove", MakeSceneEntityTarget(scene, index), "Scene component removed");
        result.SetPayloadValue("entity_id", std::to_string(index));
        result.SetPayloadValue("component_kind", std::string(ToString(kind)));
        return result;
    }

    ResultEnvelope ApplicationOperations::UpsertSceneCameraComponent(Scene& scene, uint32_t index, const Rendering::CameraComponent& component) const {
        return UpsertSceneComponent(scene, index, component, "scene.component.upsert", "Camera component upserted", ToString(SceneComponentKind::Camera));
    }
    ResultEnvelope ApplicationOperations::UpsertSceneMeshComponent(Scene& scene, uint32_t index, const Rendering::MeshComponent& component) const {
        return UpsertSceneComponent(scene, index, component, "scene.component.upsert", "Mesh component upserted", ToString(SceneComponentKind::Mesh));
    }
    ResultEnvelope ApplicationOperations::UpsertSceneMaterialComponent(Scene& scene, uint32_t index, const Rendering::MaterialComponent& component) const {
        return UpsertSceneComponent(scene, index, component, "scene.component.upsert", "Material component upserted", ToString(SceneComponentKind::Material));
    }

	ResultEnvelope ApplicationOperations::CreateBuiltinMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		BuiltinMeshPrimitive primitive,
		std::string_view meshName,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().CreateBuiltinMeshAsset(context, assetId, primitive, meshName, outHandle);
	}

	ResultEnvelope ApplicationOperations::RegisterMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Mesh>& mesh,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().RegisterMeshAsset(context, assetId, mesh, outHandle);
	}

	ResultEnvelope ApplicationOperations::LoadMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().LoadMeshAsset(context, assetId, outHandle);
	}

	ResultEnvelope ApplicationOperations::RegisterMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Material>& material,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().RegisterMaterialAsset(context, assetId, material, outHandle);
	}

	ResultEnvelope ApplicationOperations::LoadMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().LoadMaterialAsset(context, assetId, outHandle);
	}

	ResultEnvelope ApplicationOperations::RegisterTextureAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::TextureResource>& texture,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().RegisterTextureAsset(context, assetId, texture, outHandle);
	}

	ResultEnvelope ApplicationOperations::InitializeAssetManifest(const ProjectContext& context) const
	{
		auto result = m_Services->Assets().LoadOrCreateManifest(context);
		result.Operation = "asset.manifest.init";
		result.SetPayloadValue("manifest_path", GetAssetManifestPath(context).generic_string());
		return result;
	}

	ResultEnvelope ApplicationOperations::InitializeProjectAssets(
		const ProjectContext& context,
		AssetImportReport* outReport) const
	{
		auto result = m_Services->Assets().InitializeProjectAssets(context, outReport);
		result.Operation = "asset.initialize";
		result.SetPayloadValue("manifest_path", GetAssetManifestPath(context).generic_string());
		return result;
	}

	ResultEnvelope ApplicationOperations::ImportAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetKind kind,
		AssetGuid* outGuid) const
	{
		AssetHandle handle = 0;
		ResultEnvelope result;
		switch (kind) {
		case AssetKind::Mesh:
			result = m_Services->Assets().LoadMeshAsset(context, assetId, &handle);
			break;
		case AssetKind::Material:
			result = m_Services->Assets().LoadMaterialAsset(context, assetId, &handle);
			break;
		case AssetKind::Texture2D:
			result = m_Services->Assets().RegisterTextureAsset(context, assetId, nullptr, &handle);
			break;
		case AssetKind::Shader:
			result = m_Services->Assets().RegisterShaderAsset(context, assetId, &handle);
			break;
		case AssetKind::Unknown:
		default:
			result = ResultEnvelope::Failure("asset.import", std::string(assetId), "Unsupported asset import kind");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.import.kind_invalid", "Asset import requires mesh, material, texture2d, or shader kind", std::string(ToString(kind)) });
			return result;
		}

		AssetRecord record;
		if (result.Status != OperationStatus::Failure && m_Services->Assets().ResolveAsset(std::string(assetId), record).Succeeded()) {
			result.SetPayloadValue("asset_guid", record.Guid);
			if (outGuid) {
				*outGuid = record.Guid;
			}

			if (result.Succeeded()) {
				AssetImportReport importReport;
				auto initializeResult = m_Services->Assets().InitializeProjectAssets(context, &importReport);
				const bool initializeFailed = initializeResult.Failed();
				if (initializeFailed) {
					result = std::move(initializeResult);
				}
				const auto* importer = m_Services->Assets().GetImporterRegistry().Find(record.Kind, record.RelativePath.extension().string());
				const bool artifactAvailable = importer && m_Services->Assets().GetLibrary().IsArtifactAvailable(
					record.Guid,
					record.Kind,
					importer->GetId(),
					importer->GetVersion(),
					importer->GetArtifactVersion());
				if (!initializeFailed && artifactAvailable) {
					const auto* libraryRecord = m_Services->Assets().GetLibrary().Find(record.Guid);
					result = ResultEnvelope::Success("asset.import", record.AssetId, "Asset registered and imported into the project Library");
					for (auto& diagnostic : initializeResult.Details) result.AddDetail(std::move(diagnostic));
					result.SetPayloadValue("artifact_path", (m_Services->Assets().GetLibrary().GetRootPath() / libraryRecord->ArtifactRelativePath).generic_string());
				}
				else if (!initializeFailed) {
					result = ResultEnvelope::ManualIntervention("asset.import", record.AssetId, "Asset was registered but no compatible artifact was produced");
					for (auto& diagnostic : initializeResult.Details) result.AddDetail(std::move(diagnostic));
				}
				result.SetPayloadValue("asset_guid", record.Guid);
				result.SetPayloadValue("imported_asset_count", std::to_string(importReport.ImportedAssets));
				result.SetPayloadValue("failed_asset_count", std::to_string(importReport.FailedAssets));
			}
		}
		else if (outGuid) {
			*outGuid = {};
		}

		result.Operation = "asset.import";
		result.SetPayloadValue("asset_id", std::string(assetId));
		result.SetPayloadValue("asset_kind", std::string(ToString(kind)));
		return result;
	}

	bool ApplicationOperations::CanImportAssetSource(const std::filesystem::path& sourcePath) const
	{
		return m_Services->Assets().CanImportSource(sourcePath);
	}

	ResultEnvelope ApplicationOperations::ReimportAssets(
		const ProjectContext& context,
		const std::filesystem::path& targetPath,
		AssetReimportReport* outReport) const
	{
		auto result = m_Services->Assets().ReimportAssets(context, targetPath, outReport);
		result.Operation = "asset.reimport";
		return result;
	}

	ResultEnvelope ApplicationOperations::ListAssets(
		const ProjectContext& context,
		std::vector<AssetRecord>& outRecords) const
	{
		outRecords.clear();
		if (!m_Services->Assets().IsManifestLoaded()) {
			auto manifestResult = m_Services->Assets().LoadManifestReadOnly(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.list";
				return manifestResult;
			}
		}

		m_Services->Assets().GetAssetRegistry().ForEachRecord([&](const AssetRecord& record) {
			outRecords.push_back(record);
		});

		auto result = ResultEnvelope::Success("asset.list", context.GetTargetId(), "Project assets listed");
		result.SetPayloadValue("asset_count", std::to_string(outRecords.size()));
		return result;
	}

	ResultEnvelope ApplicationOperations::RegisterSceneAsset(const ProjectContext& context, const std::filesystem::path& sourcePath, AssetGuid* outGuid) const
	{
		return m_Services->Assets().RegisterSceneAsset(context, sourcePath, outGuid);
	}

	ResultEnvelope ApplicationOperations::CreateSceneAsset(
		const ProjectContext& context,
		const std::filesystem::path& assetPath,
		AssetGuid* outGuid) const
	{
		std::filesystem::path resolvedPath;
		ResultEnvelope pathError;
		if (!TryResolveNewSceneAssetPath(context, assetPath, resolvedPath, pathError)) {
			return pathError;
		}

		Ref<Scene> scene;
		auto createResult = m_Services->Scenes().CreateScene(resolvedPath.stem().string(), scene);
		if (!createResult.Succeeded()) {
			createResult.Operation = "asset.scene.create";
			return createResult;
		}

		auto saveResult = m_Services->Scenes().SaveScene(*scene, resolvedPath);
		if (!saveResult.Succeeded()) {
			const std::array createdPaths{ resolvedPath };
			return CleanupSceneAssetCreationFailure(std::move(saveResult), createdPaths);
		}

		AssetGuid guid;
		auto registerResult = m_Services->Assets().RegisterSceneAsset(context, resolvedPath, &guid);
		if (!registerResult.Succeeded()) {
			const std::array createdPaths{ resolvedPath };
			return CleanupSceneAssetCreationFailure(std::move(registerResult), createdPaths);
		}

		if (outGuid) {
			*outGuid = guid;
		}
		registerResult.Operation = "asset.scene.create";
		registerResult.Summary = "Scene asset created";
		return registerResult;
	}

	ResultEnvelope ApplicationOperations::RenameAsset(
		const ProjectContext& context,
		const AssetGuid& guid,
		std::string_view newBaseName,
		AssetRecord* outRecord) const
	{
		return m_Services->Assets().RenameAsset(context, guid, newBaseName, outRecord);
	}

	ResultEnvelope ApplicationOperations::DeleteAsset(const ProjectContext& context, const AssetGuid& guid) const {
		return m_Services->Assets().DeleteAsset(context, guid);
	}

	ResultEnvelope ApplicationOperations::InspectAsset(const AssetGuid& guid, AssetInspectionSnapshot& outSnapshot) const {
		return m_Services->Assets().InspectAsset(guid, outSnapshot);
	}

	ResultEnvelope ApplicationOperations::ApplyAssetEdit(const ProjectContext& context, const AssetEditCommit& commit, AssetApplyState& outState, AssetReimportReport* outReport) const {
		return m_Services->AssetAuthoring().Apply(context, commit, outState, outReport);
	}

	ResultEnvelope ApplicationOperations::GetMaterialDefinition(const AssetGuid& materialGuid, Rendering::MaterialDefinition& outDefinition, AssetImportHealth* outHealth) const {
		return m_Services->Assets().GetMaterialDefinition(materialGuid, outDefinition, outHealth);
	}

	ResultEnvelope ApplicationOperations::GetShaderAuthoringMetadata(const AssetGuid& shaderGuid, Rendering::ShaderAuthoringMetadata& outMetadata) const {
		return m_Services->Assets().GetShaderAuthoringMetadata(shaderGuid, outMetadata);
	}

	ResultEnvelope ApplicationOperations::ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const
	{
		return m_Services->Assets().ResolveAsset(handle, outRecord);
	}

	ResultEnvelope ApplicationOperations::ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const
	{
		return m_Services->Assets().ResolveAsset(assetId, outRecord);
	}

	ResultEnvelope ApplicationOperations::ResolveAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetRecord& outRecord) const
	{
		if (!m_Services->Assets().IsManifestLoaded()) {
			auto manifestResult = m_Services->Assets().LoadManifestReadOnly(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.resolve";
				return manifestResult;
			}
		}

		return ResolveAsset(assetId, outRecord);
	}

	ResultEnvelope ApplicationOperations::ResolveAssetByGuid(
		const ProjectContext& context,
		const AssetGuid& guid,
		AssetRecord& outRecord) const
	{
		if (!m_Services->Assets().IsManifestLoaded()) {
			auto manifestResult = m_Services->Assets().LoadManifestReadOnly(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.resolve";
				return manifestResult;
			}
		}

		const auto* record = m_Services->Assets().FindRecordByGuid(guid);
		if (!record) {
			auto result = ResultEnvelope::Failure("asset.resolve", guid, "Asset guid was not found");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.guid.missing", "The requested asset GUID is not present in the project registry", guid });
			return result;
		}

		outRecord = *record;
		auto result = ResultEnvelope::Success("asset.resolve", guid, "Asset resolved by guid");
		result.SetPayloadValue("asset_handle", std::to_string(record->Handle));
		result.SetPayloadValue("asset_id", record->AssetId);
		result.SetPayloadValue("asset_kind", std::string(ToString(record->Kind)));
		return result;
	}

	ResultEnvelope ApplicationOperations::ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const
	{
		return m_Services->Assets().ResolveMeshAsset(handle, outMesh);
	}

	ResultEnvelope ApplicationOperations::ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const
	{
		return m_Services->Assets().ResolveMaterialAsset(handle, outMaterial);
	}

	ResultEnvelope ApplicationOperations::ResolveTextureAsset(AssetHandle handle, Ref<Rendering::TextureResource>& outTexture) const
	{
		return m_Services->Assets().ResolveTextureAsset(handle, outTexture);
	}

	ResultEnvelope ApplicationOperations::ValidateAssets(const ProjectContext& context, AssetValidationReport* outReport) const
	{
		return m_Services->Assets().ValidateRegistry(context, outReport);
	}

	ResultEnvelope ApplicationOperations::AttachSceneViewportRenderer(
		const Ref<Scene>& scene,
		const Ref<Rendering::RenderTarget>& renderTarget) const
	{
		if (!scene) {
			auto result = ResultEnvelope::Failure("rendering.attach_scene_viewport", {}, "Scene is not available");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.attach_scene_viewport.scene_missing", "A renderable scene is required", {} });
			return result;
		}

		if (!renderTarget) {
			auto result = ResultEnvelope::Failure("rendering.attach_scene_viewport", scene->GetName(), "Render target is not available");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.attach_scene_viewport.render_target_missing", "A target render target is required", {} });
			return result;
		}

        auto attached = scene->AttachRenderer(renderTarget, &m_Services->GetAssetResolver());
        if (!attached) return EcsFailureEnvelope("rendering.attach_scene_viewport", scene->GetName(), attached.GetError());
        const bool createdRenderer = attached.Value();

		auto result = ResultEnvelope::Success("rendering.attach_scene_viewport", scene->GetName(), "Scene viewport renderer attached");
		result.SetPayloadValue("scene_name", scene->GetName());
		result.SetPayloadValue("created_render_system", createdRenderer ? "true" : "false");
		return result;
	}

	ResultEnvelope ApplicationOperations::RenderSceneViewport(
		Scene& scene,
		const Rendering::RenderCamera& camera,
		Rendering::RenderGraphExtension* extension) const
	{
		if (!scene.HasRenderer()) {
			auto result = ResultEnvelope::Failure("rendering.render_scene_viewport", scene.GetName(), "Scene viewport renderer is not attached");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.render_scene_viewport.missing_render_system", "AttachSceneViewportRenderer must succeed before rendering a scene viewport", {} });
			return result;
		}

        auto rendered = scene.RenderSingleCamera(camera, extension);
        if (!rendered) {
            auto result = EcsFailureEnvelope("rendering.render_scene_viewport", scene.GetName(), rendered.GetError());
            if (rendered.GetError().Operation == "RenderPipeline") {
                result.AddDetail({ DiagnosticSeverity::Error, "rendering.render_scene_viewport.pipeline_failed",
                    "RenderPipeline did not produce a successful result", {} });
                if (const auto* failedRender = scene.LastRenderResult()) AddRenderGraphDetails(result, *failedRender);
            }
            return result;
        }
        const auto* lastResult = scene.LastRenderResult();
        if (!lastResult) return ResultEnvelope::Failure("rendering.render_scene_viewport", scene.GetName(), "Scene viewport renderer produced no result");
        const auto& renderResult = *lastResult;
		if (!renderResult.Succeeded) {
			auto result = ResultEnvelope::Failure("rendering.render_scene_viewport", scene.GetName(), "Scene viewport render failed");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.render_scene_viewport.pipeline_failed", "RenderPipeline did not produce a successful result", {} });
			AddRenderGraphDetails(result, renderResult);
			return result;
		}

		auto result = ResultEnvelope::Success("rendering.render_scene_viewport", scene.GetName(), "Scene viewport rendered");
		result.SetPayloadValue("scene_name", scene.GetName());
		result.SetPayloadValue("render_items", std::to_string(renderResult.Stats.RenderItems));
		result.SetPayloadValue("submitted_items", std::to_string(renderResult.Stats.SubmittedItems));
		result.SetPayloadValue("skipped_items", std::to_string(renderResult.Stats.SkippedItems));
		result.SetPayloadValue("draw_calls", std::to_string(renderResult.Stats.DrawCalls));
		result.SetPayloadValue("pass_count", std::to_string(renderResult.Stats.PassCount));
		result.SetPayloadValue("graphics_queue_signal", std::to_string(renderResult.Stats.GraphicsQueueSignalValue));
		result.SetPayloadValue("graphics_queue_completed", std::to_string(renderResult.Stats.GraphicsQueueCompletedValue));
		result.SetPayloadValue("frames_in_flight", std::to_string(renderResult.Stats.FramesInFlight));
		result.SetPayloadValue("visible_items", std::to_string(renderResult.Stats.VisibleItems));
		result.SetPayloadValue("fallback_items", std::to_string(renderResult.Stats.FallbackItems));
		result.SetPayloadValue("diagnostics", std::to_string(renderResult.Diagnostics.size()));
		result.SetPayloadValue("graph_resources", std::to_string(renderResult.GraphStats.ResourceCount));
		result.SetPayloadValue("graph_edges", std::to_string(renderResult.GraphStats.EdgeCount));
		result.SetPayloadValue("graph_outputs", std::to_string(renderResult.GraphStats.OutputCount));
		result.SetPayloadValue("graph_diagnostics", std::to_string(renderResult.GraphDiagnostics.size()));
		return result;
	}

	ResultEnvelope ApplicationOperations::Validate(const ApplicationValidationRequest& request, ValidationReport* outReport) const
	{
		ValidationRequest validationRequest;
		validationRequest.Project = request.Project;
		validationRequest.SceneTarget = request.SceneTarget;
		validationRequest.Assets = request.IncludeAssets ? &m_Services->Assets() : nullptr;
		auto result = m_Services->Validation().Validate(validationRequest, outReport);
		result.Operation = "validation.validate";
		return result;
	}

	ResultEnvelope ApplicationOperations::ScanReflection(const ReflectionToolRequest& request) const
	{
		return m_Services->ReflectionTools().Scan(request);
	}

	ResultEnvelope ApplicationOperations::GenerateReflection(const ReflectionToolRequest& request) const
	{
		return m_Services->ReflectionTools().Generate(request);
	}

	ResultEnvelope ApplicationOperations::ValidateReflection(const ReflectionToolRequest& request) const
	{
		return m_Services->ReflectionTools().Validate(request);
	}

	void ApplicationOperations::RegisterDefaultOperations()
	{
		m_Registry.Register({ "project.initialize", OperationDomain::Project, "Initialize a HuaEngine project root" });
		m_Registry.Register({ "project.resolve_context", OperationDomain::Project, "Resolve the nearest HuaEngine project context" });
		m_Registry.Register({ "project.check_status", OperationDomain::Project, "Validate project metadata and managed directories" });

		m_Registry.Register({ "scene.create", OperationDomain::Scene, "Create an in-memory scene" });
		m_Registry.Register({ "scene.load", OperationDomain::Scene, "Load a scene from the project scene root" });
		m_Registry.Register({ "scene.save", OperationDomain::Scene, "Persist a scene to disk" });
		m_Registry.Register({ "scene.validate", OperationDomain::Scene, "Validate scene structure for runtime use" });
		m_Registry.Register({ "scene.entity.create", OperationDomain::Scene, "Create a scene entity through the formal application layer" });
		m_Registry.Register({ "scene.entity.delete", OperationDomain::Scene, "Delete one or more scene entities through the formal application layer" });
		m_Registry.Register({ "scene.entity.upsert_name", OperationDomain::Scene, "Update scene entity naming state through the formal application layer" });
		m_Registry.Register({ "scene.entity.upsert_transform", OperationDomain::Scene, "Update scene entity transform state through the formal application layer" });
		m_Registry.Register({ "scene.component.add", OperationDomain::Scene, "Add a supported scene component through the formal application layer" });
		m_Registry.Register({ "scene.component.remove", OperationDomain::Scene, "Remove a supported scene component through the formal application layer" });
		m_Registry.Register({ "scene.component.upsert", OperationDomain::Scene, "Restore or replace a supported scene component through the formal application layer" });

		m_Registry.Register({ "asset.create_builtin_mesh", OperationDomain::Asset, "Create, persist, and register a built-in mesh asset" });
		m_Registry.Register({ "asset.register_mesh", OperationDomain::Asset, "Register a mesh asset into the project asset registry" });
		m_Registry.Register({ "asset.load_mesh", OperationDomain::Asset, "Load and register a mesh asset from disk" });
		m_Registry.Register({ "asset.register_material", OperationDomain::Asset, "Register a material asset into the project asset registry" });
		m_Registry.Register({ "asset.load_material", OperationDomain::Asset, "Load and register a material asset from disk" });
		m_Registry.Register({ "asset.register_texture", OperationDomain::Asset, "Register a texture asset into the project asset registry" });
		m_Registry.Register({ "asset.manifest.init", OperationDomain::Asset, "Initialize the project asset manifest" });
		m_Registry.Register({ "asset.initialize", OperationDomain::Asset, "Initialize the project asset Library and import missing artifacts" });
		m_Registry.Register({ "asset.import", OperationDomain::Asset, "Import a single project asset into the manifest" });
		m_Registry.Register({ "asset.reimport", OperationDomain::Asset, "Reimport project asset files into the Library" });
		m_Registry.Register({ "asset.scene.create", OperationDomain::Asset, "Create and register a persisted scene asset" });
		m_Registry.Register({ "asset.delete", OperationDomain::Asset, "Delete a project asset and generated artifacts" });
		m_Registry.Register({ "asset.list", OperationDomain::Asset, "List project manifest assets" });
		m_Registry.Register({ "asset.resolve", OperationDomain::Asset, "Resolve an asset record by GUID, handle, or asset id" });
		m_Registry.Register({ "asset.validate", OperationDomain::Asset, "Validate project asset registry health" });

		m_Registry.Register({ "rendering.attach_scene_viewport", OperationDomain::Rendering, "Attach or reuse a scene viewport renderer through the application layer" });
		m_Registry.Register({ "rendering.render_scene_viewport", OperationDomain::Rendering, "Render a scene viewport through the application layer" });

		m_Registry.Register({ "validation.validate", OperationDomain::Validation, "Run aggregate validation across project, scene, and asset domains" });
		m_Registry.Register({ "reflection.scan", OperationDomain::Validation, "Scan source reflection markers into a manifest" });
		m_Registry.Register({ "reflection.generate", OperationDomain::Validation, "Generate C++ reflection metadata from source markers" });
		m_Registry.Register({ "reflection.validate", OperationDomain::Validation, "Validate source reflection markers without writing generated files" });
	}
}
