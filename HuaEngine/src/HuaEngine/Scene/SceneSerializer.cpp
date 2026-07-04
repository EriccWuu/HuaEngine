#include "enginepch.h"
#include "SceneSerializer.h"

#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/Project/ProjectService.h"
#include "Module/Rendering/RenderingComponent.h"

#include <filesystem>
#include <vector>

namespace {
	thread_local std::vector<std::filesystem::path> s_SceneSerializationPathStack;

	HE::ComponentRegistry CreateSceneComponentRegistry() {
		HE::ComponentRegistry registry;
		HE::RegisterCoreComponents(registry);
		return registry;
	}

	HE::AssetGuid ResolveManifestMeshGuid(const std::string& meshAssetName) {
		if (s_SceneSerializationPathStack.empty() || meshAssetName.empty()) {
			return {};
		}

		HE::ProjectContext context;
		HE::ProjectService projectService;
		if (!projectService.ResolveProjectContext(s_SceneSerializationPathStack.back(), context).Succeeded()) {
			return {};
		}

		HE::AssetManifest manifest;
		if (!HE::LoadAssetManifest(context, manifest).Succeeded()) {
			return {};
		}

		const HE::AssetManifestRecord* record = manifest.FindByAssetId(meshAssetName);
		if (record == nullptr || record->Kind != HE::AssetKind::Mesh) {
			return {};
		}

		return record->Guid;
	}

	HE::AssetGuid ResolveLegacyMeshGuid(const std::string& meshAssetName) {
		if (meshAssetName == "Quad" || meshAssetName == "quad" || meshAssetName == "builtin/mesh/quad") {
			return HE::BuiltinAssetGuids::QuadMesh;
		}
		if (meshAssetName == "Cube" || meshAssetName == "cube" || meshAssetName == "builtin/mesh/cube") {
			return HE::BuiltinAssetGuids::CubeMesh;
		}
		if (meshAssetName == "Sphere" || meshAssetName == "sphere" || meshAssetName == "builtin/mesh/sphere") {
			return HE::BuiltinAssetGuids::SphereMesh;
		}

		if (auto manifestGuid = ResolveManifestMeshGuid(meshAssetName); !manifestGuid.empty()) {
			return manifestGuid;
		}

		HE_CORE_WARN("Scene migration could not map legacy MeshAssetName '{}'", meshAssetName);
		return {};
	}

	bool DeserializeLegacyMaterialOverride(
		HE::Serialization::SerializationBackend& backend,
		const std::string& parameterName,
		HE::Rendering::MaterialOverrideSet& overrides) {
		HE::Rendering::MaterialParameterValue value;
		if (HE::Serialization::MaterialOverrideSerialization::DeserializeMaterialParameterValue(backend, value)) {
			overrides.Parameters[parameterName] = std::move(value);
			return true;
		}

		HE_CORE_WARN("Scene migration could not migrate legacy material override '{}'", parameterName);
		return false;
	}

	void MigrateLegacyMeshComponent(
		HE::Serialization::SerializationBackend& backend,
		HE::Rendering::MeshComponent& component) {
		if (!backend.HasField("MeshAssetName") || component.Mesh.Reference.IsValid()) {
			return;
		}

		std::string legacyMeshAssetName;
		if (backend.Deserialize("MeshAssetName", legacyMeshAssetName)) {
			component.Mesh.Reference.Guid = ResolveLegacyMeshGuid(legacyMeshAssetName);
		}
	}

	bool MigrateLegacyMaterialComponent(
		HE::Serialization::SerializationBackend& backend,
		HE::Rendering::MaterialComponent& component) {
		if (!backend.HasField("MaterialInstance")) {
			return true;
		}

		if (!component.Material.Reference.IsValid()) {
			component.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		}

		if (backend.GetFieldType("MaterialInstance") != HE::Serialization::SerializationType::Object) {
			HE_CORE_WARN("Scene migration found legacy MaterialInstance with unsupported shape");
			return false;
		}

		bool success = true;
		backend.BeginObject("MaterialInstance");
		if (backend.HasField("parameter_overrides") &&
			backend.GetFieldType("parameter_overrides") == HE::Serialization::SerializationType::Object) {
			backend.BeginObject("parameter_overrides");
			backend.ForEachField([&](const std::string& parameterName) {
				success &= DeserializeLegacyMaterialOverride(backend, parameterName, component.Overrides);
			});
			backend.EndObject();
		}
		backend.EndObject();
		return success;
	}

	bool MigrateLegacyComponent(
		HE::Serialization::SerializationBackend& backend,
		const std::string& componentName,
		void* component) {
		if (componentName != "MeshComponent" && componentName != "MaterialComponent") {
			return true;
		}
		if (!backend.HasField(componentName) ||
			backend.GetFieldType(componentName) != HE::Serialization::SerializationType::Object) {
			return true;
		}

		backend.BeginObject(componentName);
		bool success = true;
		if (componentName == "MeshComponent") {
			MigrateLegacyMeshComponent(backend, *static_cast<HE::Rendering::MeshComponent*>(component));
		}
		else {
			success = MigrateLegacyMaterialComponent(backend, *static_cast<HE::Rendering::MaterialComponent*>(component));
		}
		backend.EndObject();
		return success;
	}

	void SerializeEntity(
		HE::Serialization::SerializationBackend& backend,
		const HE::ComponentRegistry& componentRegistry,
		HE::Entity entity) {
		backend.BeginObject("");
		backend.Serialize("uuid", HE::ToString(entity.GetUuid()));
		backend.Serialize("name", entity.GetName());
		backend.BeginObject("components");

		for (const HE::ComponentTypeId typeId : entity.ListComponentTypes()) {
			const auto* metadata = componentRegistry.FindByTypeId(typeId);
			const void* component = entity.TryGetComponentByType(typeId);
			if (metadata == nullptr || component == nullptr || metadata->RuntimeType == nullptr) {
				continue;
			}

			HE::Refl::SerializeRuntimeObject(*metadata->RuntimeType, backend, metadata->TypeName, component);
		}

		backend.EndObject();
		backend.EndObject();
	}

	bool DeserializeEntity(
		HE::Serialization::SerializationBackend& backend,
		const HE::ComponentRegistry& componentRegistry,
		HE::Scene& scene) {
		backend.BeginObject("");

		std::string uuidText;
		backend.Deserialize("uuid", uuidText);

		std::string entityName = "Entity";
		backend.Deserialize("name", entityName);

		const HE::EntityUuid uuid = HE::EntityUuid::FromString(uuidText);
		HE::Entity entity = scene.GetWorld().CreateEntityWithUuid(uuid, entityName);
		if (!entity.IsValid()) {
			backend.EndObject();
			return false;
		}

		if (backend.HasField("components")) {
			backend.BeginObject("components");
			const auto componentNames = backend.GetObjectKeys();
			bool success = true;
			for (const auto& componentName : componentNames) {
				const auto* metadata = componentRegistry.FindByName(componentName);
				if (metadata == nullptr || metadata->ConstructDefault == nullptr ||
					metadata->Destroy == nullptr || metadata->RuntimeType == nullptr || !metadata->AddCopyToWorld) {
					continue;
				}

				void* component = metadata->ConstructDefault();
				if (component == nullptr) {
					success = false;
					continue;
				}

				const bool componentSuccess = HE::Refl::DeserializeRuntimeObject(
					*metadata->RuntimeType,
					backend,
					componentName,
					component);
				if (componentSuccess) {
					if (MigrateLegacyComponent(backend, componentName, component)) {
						metadata->AddCopyToWorld(scene.GetWorld(), entity.GetId(), component);
					}
					else {
						success = false;
					}
				}
				else {
					success = false;
				}
				metadata->Destroy(component);
			}
			backend.EndObject();

			if (!success) {
				backend.EndObject();
				return false;
			}
		}

		backend.EndObject();
		return true;
	}
}

namespace HE::Serialization {
	namespace Detail {
		void PushSceneSerializationPath(const std::filesystem::path& path) {
			s_SceneSerializationPathStack.push_back(path);
		}

		void PopSceneSerializationPath() {
			if (!s_SceneSerializationPathStack.empty()) {
				s_SceneSerializationPathStack.pop_back();
			}
		}
	}

	void Serializer<Scene>::Serialize(SerializationBackend& backend, const std::string& name, const Scene& scene) {
		if (!name.empty()) {
			backend.BeginObject(name);
		}

		const auto componentRegistry = CreateSceneComponentRegistry();
		backend.Serialize("name", scene.GetName().empty() ? "Untitled Scene" : scene.GetName());
		backend.Serialize("version", 3);
		backend.BeginArray("entities");

		uint32_t entityIndex = 0;
		const_cast<Scene&>(scene).GetWorld().ForEachEntity([&](HE::Entity entity) {
			backend.BeginArrayElement(entityIndex++);
			SerializeEntity(backend, componentRegistry, entity);
			backend.EndArrayElement();
		});

		backend.EndArray();

		if (!name.empty()) {
			backend.EndObject();
		}
	}

	bool Serializer<Scene>::Deserialize(SerializationBackend& backend, const std::string& name, Scene& scene) {
		if (!name.empty()) {
			backend.BeginObject(name);
		}

		scene.GetWorld().Clear();

		std::string sceneName;
		if (backend.Deserialize("name", sceneName)) {
			scene.SetName(sceneName);
		}

		if (backend.HasField("version")) {
			uint32_t ignoredVersion = 0;
			backend.Deserialize("version", ignoredVersion);
		}

		if (!backend.HasField("entities")) {
			if (!name.empty()) {
				backend.EndObject();
			}
			return false;
		}

		bool success = true;
		const auto componentRegistry = CreateSceneComponentRegistry();
		const uint32_t entityCount = static_cast<uint32_t>(backend.GetArraySize("entities"));
		backend.BeginArray("entities");
		for (uint32_t i = 0; i < entityCount; ++i) {
			backend.BeginArrayElement(i);
			success &= DeserializeEntity(backend, componentRegistry, scene);
			backend.EndArrayElement();
		}
		backend.EndArray();

		if (!name.empty()) {
			backend.EndObject();
		}

		return success;
	}
}
