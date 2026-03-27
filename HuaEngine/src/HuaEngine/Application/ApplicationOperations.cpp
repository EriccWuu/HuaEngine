#include "enginepch.h"
#include "ApplicationOperations.h"

#include "ApplicationServices.h"
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
