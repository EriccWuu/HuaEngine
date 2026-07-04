#include "enginepch.h"
#include "SceneSerializer.h"

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	HE::ComponentRegistry CreateSceneComponentRegistry() {
		HE::ComponentRegistry registry;
		HE::RegisterCoreComponents(registry);
		return registry;
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

		HE_CORE_WARN("Scene migration could not map legacy MeshAssetName '{}'", meshAssetName);
		return {};
	}

	bool DeserializeLegacyVec4Override(
		HE::Serialization::SerializationBackend& backend,
		const std::string& parameterName,
		HE::Rendering::MaterialOverrideSet& overrides) {
		std::string type;
		if (!backend.Deserialize("value_type", type)) {
			(void)backend.Deserialize("type", type);
		}

		if (type != "Vec4" && type != "vec4") {
			return true;
		}

		glm::vec4 value(0.0f);
		bool success = false;
		if (backend.HasField("value") && backend.GetFieldType("value") == HE::Serialization::SerializationType::Object) {
			backend.BeginObject("value");
			success = backend.Deserialize("x", value.x);
			success &= backend.Deserialize("y", value.y);
			success &= backend.Deserialize("z", value.z);
			success &= backend.Deserialize("w", value.w);
			backend.EndObject();
		}
		else if (backend.HasField("value") && backend.GetFieldType("value") == HE::Serialization::SerializationType::Array &&
			backend.GetArraySize("value") == 4) {
			backend.BeginArray("value");
			backend.BeginArrayElement(0);
			success = backend.Deserialize("", value.x);
			backend.EndArrayElement();
			backend.BeginArrayElement(1);
			success &= backend.Deserialize("", value.y);
			backend.EndArrayElement();
			backend.BeginArrayElement(2);
			success &= backend.Deserialize("", value.z);
			backend.EndArrayElement();
			backend.BeginArrayElement(3);
			success &= backend.Deserialize("", value.w);
			backend.EndArrayElement();
			backend.EndArray();
		}

		if (success) {
			overrides.Parameters[parameterName] = value;
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

	void MigrateLegacyMaterialComponent(
		HE::Serialization::SerializationBackend& backend,
		HE::Rendering::MaterialComponent& component) {
		if (!backend.HasField("MaterialInstance")) {
			return;
		}

		if (!component.Material.Reference.IsValid()) {
			component.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		}

		if (backend.GetFieldType("MaterialInstance") != HE::Serialization::SerializationType::Object) {
			HE_CORE_WARN("Scene migration found legacy MaterialInstance with unsupported shape");
			return;
		}

		backend.BeginObject("MaterialInstance");
		if (backend.HasField("parameter_overrides") &&
			backend.GetFieldType("parameter_overrides") == HE::Serialization::SerializationType::Object) {
			backend.BeginObject("parameter_overrides");
			backend.ForEachField([&](const std::string& parameterName) {
				(void)DeserializeLegacyVec4Override(backend, parameterName, component.Overrides);
			});
			backend.EndObject();
		}
		backend.EndObject();
	}

	void MigrateLegacyComponent(
		HE::Serialization::SerializationBackend& backend,
		const std::string& componentName,
		void* component) {
		if (componentName != "MeshComponent" && componentName != "MaterialComponent") {
			return;
		}
		if (!backend.HasField(componentName) ||
			backend.GetFieldType(componentName) != HE::Serialization::SerializationType::Object) {
			return;
		}

		backend.BeginObject(componentName);
		if (componentName == "MeshComponent") {
			MigrateLegacyMeshComponent(backend, *static_cast<HE::Rendering::MeshComponent*>(component));
		}
		else {
			MigrateLegacyMaterialComponent(backend, *static_cast<HE::Rendering::MaterialComponent*>(component));
		}
		backend.EndObject();
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
					MigrateLegacyComponent(backend, componentName, component);
					metadata->AddCopyToWorld(scene.GetWorld(), entity.GetId(), component);
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
