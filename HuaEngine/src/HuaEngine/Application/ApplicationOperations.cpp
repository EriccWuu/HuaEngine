#include "enginepch.h"
#include "ApplicationOperations.h"

#include "ApplicationServices.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneService.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Script/ScriptService.h"
#include "HuaEngine/Script/ScriptRuntimeSystem.h"
#include "HuaEngine/Validation/ValidationService.h"
#include "Module/Rendering/RenderSystem.h"
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

	HE::Entity ResolveSceneEntity(HE::Scene& scene, uint32_t entityId) {
		auto& entityManager = scene.GetEntityManager();
		return { static_cast<entt::entity>(entityId), &entityManager };
	}

	bool EnsureSceneEntityExists(HE::Scene& scene, uint32_t entityId, HE::ResultEnvelope& outError) {
		auto entity = ResolveSceneEntity(scene, entityId);
		if (entity.IsValid()) {
			return true;
		}

		outError = MakeSceneEntityMissingResult("scene.entity.resolve", scene, entityId, "Scene entity does not exist");
		return false;
	}

	HE::Rendering::CameraComponent MakeDefaultCameraComponent() {
		HE::Rendering::CameraComponent component;
		component.Camera = HE::CreateRef<HE::Rendering::Camera>();
		component.Primary = true;
		component.FixedAspectRatio = false;
		return component;
	}

	HE::Rendering::MeshComponent MakeDefaultMeshComponent() {
		HE::Rendering::MeshManager::Instance().LoadDefaultMeshes();
		return HE::Rendering::MeshComponent("Quad");
	}

	HE::Rendering::MaterialComponent MakeDefaultMaterialComponent() {
		auto& library = HE::Rendering::MaterialLibrary::Instance();
		HE::Ref<HE::Rendering::Material> baseMaterial = nullptr;

		if (library.HasMaterial("SandboxMaterial")) {
			baseMaterial = library.GetMaterial("SandboxMaterial");
		}

		if (!baseMaterial) {
			if (!library.GetDefaultMaterial()) {
				library.CreateDefaultMaterials();
			}
			baseMaterial = library.GetDefaultMaterial();
		}

		HE::Rendering::MaterialComponent component;
		component.MaterialInstance = baseMaterial ? baseMaterial->CreateInstance() : nullptr;
		return component;
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

	ResultEnvelope ApplicationOperations::CreateSceneEntity(
		Scene& scene,
		std::string_view entityName,
		uint32_t* outEntityId) const
	{
		auto entity = scene.GetEntityManager().CreateEntity(std::string(entityName));
		const auto entityId = entity.GetUid();
		if (outEntityId) {
			*outEntityId = entityId;
		}

		auto result = ResultEnvelope::Success("scene.entity.create", MakeSceneEntityTarget(scene, entityId), "Scene entity created");
		result.SetPayloadValue("scene_name", scene.GetName());
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("entity_name", entity.GetName());
		return result;
	}

	ResultEnvelope ApplicationOperations::DeleteSceneEntities(
		Scene& scene,
		std::span<const uint32_t> entityIds,
		uint32_t* outDeletedCount) const
	{
		if (entityIds.empty()) {
			auto result = ResultEnvelope::Failure("scene.entity.delete", scene.GetName(), "At least one scene entity id is required");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.entity.delete.empty", "DeleteSceneEntities requires at least one entity id", {} });
			return result;
		}

		uint32_t deletedCount = 0;
		for (const auto entityId : entityIds) {
			auto entity = ResolveSceneEntity(scene, entityId);
			if (!entity.IsValid()) {
				continue;
			}

			scene.GetEntityManager().DestroyEntity(entity);
			++deletedCount;
		}

		if (outDeletedCount) {
			*outDeletedCount = deletedCount;
		}

		if (deletedCount == 0) {
			auto result = ResultEnvelope::Failure("scene.entity.delete", scene.GetName(), "No matching scene entities were deleted");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.entity.delete.none_deleted", "None of the requested entity ids resolved to live scene entities", {} });
			return result;
		}

		auto result = ResultEnvelope::Success("scene.entity.delete", scene.GetName(), deletedCount > 1 ? "Scene entities deleted" : "Scene entity deleted");
		result.SetPayloadValue("scene_name", scene.GetName());
		result.SetPayloadValue("deleted_count", std::to_string(deletedCount));
		return result;
	}

	ResultEnvelope ApplicationOperations::UpsertSceneEntityName(
		Scene& scene,
		uint32_t entityId,
		const NameComponent& component) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.entity.upsert_name";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		registry.emplace_or_replace<NameComponent>(static_cast<entt::entity>(entityId), component);

		auto result = ResultEnvelope::Success("scene.entity.upsert_name", MakeSceneEntityTarget(scene, entityId), "Scene entity name updated");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("entity_name", component.Name);
		return result;
	}

	ResultEnvelope ApplicationOperations::UpsertSceneEntityTransform(
		Scene& scene,
		uint32_t entityId,
		const TransformComponent& component) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.entity.upsert_transform";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		registry.emplace_or_replace<TransformComponent>(static_cast<entt::entity>(entityId), component);

		auto result = ResultEnvelope::Success("scene.entity.upsert_transform", MakeSceneEntityTarget(scene, entityId), "Scene entity transform updated");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		return result;
	}

	ResultEnvelope ApplicationOperations::AddSceneComponent(
		Scene& scene,
		uint32_t entityId,
		SceneComponentKind componentKind) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.component.add";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		const auto handle = static_cast<entt::entity>(entityId);
		switch (componentKind) {
		case SceneComponentKind::Camera:
			if (registry.all_of<Rendering::CameraComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, entityId), "Camera component already exists on the target entity");
			}
			break;
		case SceneComponentKind::Mesh:
			if (registry.all_of<Rendering::MeshComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, entityId), "Mesh component already exists on the target entity");
			}
			break;
		case SceneComponentKind::Material:
			if (registry.all_of<Rendering::MaterialComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, entityId), "Material component already exists on the target entity");
			}
			break;
		}

		switch (componentKind) {
		case SceneComponentKind::Camera:
			return UpsertSceneCameraComponent(scene, entityId, MakeDefaultCameraComponent());
		case SceneComponentKind::Mesh:
			return UpsertSceneMeshComponent(scene, entityId, MakeDefaultMeshComponent());
		case SceneComponentKind::Material:
			return UpsertSceneMaterialComponent(scene, entityId, MakeDefaultMaterialComponent());
		}

		auto result = ResultEnvelope::Failure("scene.component.add", MakeSceneEntityTarget(scene, entityId), "Unsupported scene component kind");
		result.AddDetail({ DiagnosticSeverity::Error, "scene.component.add.unsupported", "The requested scene component kind is not supported", std::string(ToString(componentKind)) });
		return result;
	}

	ResultEnvelope ApplicationOperations::RemoveSceneComponent(
		Scene& scene,
		uint32_t entityId,
		SceneComponentKind componentKind) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.component.remove";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		const auto handle = static_cast<entt::entity>(entityId);
		switch (componentKind) {
		case SceneComponentKind::Camera:
			if (!registry.all_of<Rendering::CameraComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.remove", MakeSceneEntityTarget(scene, entityId), "Camera component is not present on the target entity");
			}
			registry.remove<Rendering::CameraComponent>(handle);
			break;
		case SceneComponentKind::Mesh:
			if (!registry.all_of<Rendering::MeshComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.remove", MakeSceneEntityTarget(scene, entityId), "Mesh component is not present on the target entity");
			}
			registry.remove<Rendering::MeshComponent>(handle);
			break;
		case SceneComponentKind::Material:
			if (!registry.all_of<Rendering::MaterialComponent>(handle)) {
				return ResultEnvelope::Failure("scene.component.remove", MakeSceneEntityTarget(scene, entityId), "Material component is not present on the target entity");
			}
			registry.remove<Rendering::MaterialComponent>(handle);
			break;
		}

		auto result = ResultEnvelope::Success("scene.component.remove", MakeSceneEntityTarget(scene, entityId), "Scene component removed");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("component_kind", std::string(ToString(componentKind)));
		return result;
	}

	ResultEnvelope ApplicationOperations::UpsertSceneCameraComponent(
		Scene& scene,
		uint32_t entityId,
		const Rendering::CameraComponent& component) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.component.upsert";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		registry.emplace_or_replace<Rendering::CameraComponent>(static_cast<entt::entity>(entityId), component);

		auto result = ResultEnvelope::Success("scene.component.upsert", MakeSceneEntityTarget(scene, entityId), "Camera component upserted");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("component_kind", std::string(ToString(SceneComponentKind::Camera)));
		return result;
	}

	ResultEnvelope ApplicationOperations::UpsertSceneMeshComponent(
		Scene& scene,
		uint32_t entityId,
		const Rendering::MeshComponent& component) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.component.upsert";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		registry.emplace_or_replace<Rendering::MeshComponent>(static_cast<entt::entity>(entityId), component);

		auto result = ResultEnvelope::Success("scene.component.upsert", MakeSceneEntityTarget(scene, entityId), "Mesh component upserted");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("component_kind", std::string(ToString(SceneComponentKind::Mesh)));
		return result;
	}

	ResultEnvelope ApplicationOperations::UpsertSceneMaterialComponent(
		Scene& scene,
		uint32_t entityId,
		const Rendering::MaterialComponent& component) const
	{
		ResultEnvelope resolveError;
		if (!EnsureSceneEntityExists(scene, entityId, resolveError)) {
			resolveError.Operation = "scene.component.upsert";
			return resolveError;
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		registry.emplace_or_replace<Rendering::MaterialComponent>(static_cast<entt::entity>(entityId), component);

		auto result = ResultEnvelope::Success("scene.component.upsert", MakeSceneEntityTarget(scene, entityId), "Material component upserted");
		result.SetPayloadValue("entity_id", std::to_string(entityId));
		result.SetPayloadValue("component_kind", std::string(ToString(SceneComponentKind::Material)));
		return result;
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
		const Ref<Rendering::Texture2D>& texture,
		AssetHandle* outHandle)
	{
		return m_Services->Assets().RegisterTextureAsset(context, assetId, texture, outHandle);
	}

	ResultEnvelope ApplicationOperations::ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const
	{
		return m_Services->Assets().ResolveAsset(handle, outRecord);
	}

	ResultEnvelope ApplicationOperations::ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const
	{
		return m_Services->Assets().ResolveAsset(assetId, outRecord);
	}

	ResultEnvelope ApplicationOperations::ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const
	{
		return m_Services->Assets().ResolveMeshAsset(handle, outMesh);
	}

	ResultEnvelope ApplicationOperations::ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const
	{
		return m_Services->Assets().ResolveMaterialAsset(handle, outMaterial);
	}

	ResultEnvelope ApplicationOperations::ResolveTextureAsset(AssetHandle handle, Ref<Rendering::Texture2D>& outTexture) const
	{
		return m_Services->Assets().ResolveTextureAsset(handle, outTexture);
	}

	ResultEnvelope ApplicationOperations::ValidateAssets(const ProjectContext& context, AssetValidationReport* outReport) const
	{
		return m_Services->Assets().ValidateRegistry(context, outReport);
	}

	ResultEnvelope ApplicationOperations::UnbindNativeScript(Entity entity) const
	{
		return m_Services->Scripts().UnbindNativeScript(entity);
	}

	ResultEnvelope ApplicationOperations::AttachScriptRuntime(Scene& scene) const
	{
		scene.AddSyetem(CreateRef<ScriptRuntimeSystem>(scene, m_Services->Scripts()));

		auto result = ResultEnvelope::Success("script.attach_runtime", scene.GetName(), "Scene script runtime attached");
		result.SetPayloadValue("scene_name", scene.GetName());
		return result;
	}

	ResultEnvelope ApplicationOperations::InitializeSceneScripts(Scene& scene, ScriptStatusReport* outReport) const
	{
		return m_Services->Scripts().InitializeSceneScripts(scene, outReport);
	}

	ResultEnvelope ApplicationOperations::UpdateSceneScripts(Scene& scene, ScriptStatusReport* outReport) const
	{
		return m_Services->Scripts().UpdateSceneScripts(scene, outReport);
	}

	ResultEnvelope ApplicationOperations::ShutdownSceneScripts(Scene& scene, ScriptStatusReport* outReport) const
	{
		return m_Services->Scripts().ShutdownSceneScripts(scene, outReport);
	}

	ResultEnvelope ApplicationOperations::CheckSceneScripts(Scene& scene, ScriptStatusReport* outReport) const
	{
		return m_Services->Scripts().CheckSceneScripts(scene, outReport);
	}

	ResultEnvelope ApplicationOperations::AttachSceneViewportRenderer(
		const Ref<Scene>& scene,
		const Ref<Rendering::FrameBuffer>& framebuffer) const
	{
		if (!scene) {
			auto result = ResultEnvelope::Failure("rendering.attach_scene_viewport", {}, "Scene is not available");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.attach_scene_viewport.scene_missing", "A renderable scene is required", {} });
			return result;
		}

		if (!framebuffer) {
			auto result = ResultEnvelope::Failure("rendering.attach_scene_viewport", scene->GetName(), "Framebuffer is not available");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.attach_scene_viewport.framebuffer_missing", "A target framebuffer is required", {} });
			return result;
		}

		auto renderSystem = scene->FindSystem<RenderSystem>();
		const bool createdNewSystem = !renderSystem;
		if (createdNewSystem) {
			renderSystem = CreateRef<RenderSystem>(scene);
			scene->AddSyetem(renderSystem);
		}

		auto framebufferBinding = framebuffer;
		renderSystem->SetFrameBuffer(framebufferBinding);

		auto result = ResultEnvelope::Success("rendering.attach_scene_viewport", scene->GetName(), "Scene viewport renderer attached");
		result.SetPayloadValue("scene_name", scene->GetName());
		result.SetPayloadValue("created_render_system", createdNewSystem ? "true" : "false");
		return result;
	}

	ResultEnvelope ApplicationOperations::RenderSceneViewport(
		Scene& scene,
		Rendering::Camera& camera) const
	{
		auto renderSystem = scene.FindSystem<RenderSystem>();
		if (!renderSystem) {
			auto result = ResultEnvelope::Failure("rendering.render_scene_viewport", scene.GetName(), "Scene viewport renderer is not attached");
			result.AddDetail({ DiagnosticSeverity::Error, "rendering.render_scene_viewport.missing_render_system", "AttachSceneViewportRenderer must succeed before rendering a scene viewport", {} });
			return result;
		}

		renderSystem->RenderSingleCamera(scene, camera);

		auto result = ResultEnvelope::Success("rendering.render_scene_viewport", scene.GetName(), "Scene viewport rendered");
		result.SetPayloadValue("scene_name", scene.GetName());
		return result;
	}

	ResultEnvelope ApplicationOperations::Validate(const ApplicationValidationRequest& request, ValidationReport* outReport) const
	{
		ValidationRequest validationRequest;
		validationRequest.Project = request.Project;
		validationRequest.SceneTarget = request.SceneTarget;
		validationRequest.Assets = request.IncludeAssets ? &m_Services->Assets() : nullptr;
		validationRequest.ScriptScene = request.ScriptScene;
		validationRequest.Scripts = request.IncludeScripts ? &m_Services->Scripts() : nullptr;
		auto result = m_Services->Validation().Validate(validationRequest, outReport);
		result.Operation = "validation.validate";
		return result;
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
		m_Registry.Register({ "asset.resolve", OperationDomain::Asset, "Resolve an asset record by handle or asset id" });
		m_Registry.Register({ "asset.validate", OperationDomain::Asset, "Validate project asset registry health" });

		m_Registry.Register({ "script.unbind", OperationDomain::Script, "Remove a native script binding from an entity" });
		m_Registry.Register({ "script.attach_runtime", OperationDomain::Script, "Attach the script runtime system to a scene" });
		m_Registry.Register({ "script.initialize", OperationDomain::Script, "Create script instances for a scene" });
		m_Registry.Register({ "script.update", OperationDomain::Script, "Advance bound scripts for a scene" });
		m_Registry.Register({ "script.shutdown", OperationDomain::Script, "Destroy active script instances for a scene" });
		m_Registry.Register({ "script.status", OperationDomain::Script, "Inspect current script binding health" });

		m_Registry.Register({ "rendering.attach_scene_viewport", OperationDomain::Rendering, "Attach or reuse a scene viewport renderer through the application layer" });
		m_Registry.Register({ "rendering.render_scene_viewport", OperationDomain::Rendering, "Render a scene viewport through the application layer" });

		m_Registry.Register({ "validation.validate", OperationDomain::Validation, "Run aggregate validation across project, scene, asset, and script domains" });
	}
}
