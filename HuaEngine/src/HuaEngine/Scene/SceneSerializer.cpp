#include "enginepch.h"
#include "SceneSerializer.h"

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	HE::ComponentRegistry CreateSceneComponentRegistry() {
		HE::ComponentRegistry registry;
		HE::RegisterCoreComponents(registry);
		return registry;
	}

	bool DeserializeLegacySceneComponent(
		HE::Serialization::SerializationBackend& backend,
		const std::string& componentName,
		void* component) {
		if (componentName == "NameComponent") {
			auto& name = *static_cast<HE::NameComponent*>(component);
			backend.BeginObject(componentName);
			const bool success = backend.Deserialize("name", name.Name);
			backend.EndObject();
			return success;
		}

		if (componentName == "TransformComponent") {
			auto& transform = *static_cast<HE::TransformComponent*>(component);
			backend.BeginObject(componentName);
			const bool success =
				HE::Serialization::DeserializeValue(backend, "position", transform.Position) &&
				HE::Serialization::DeserializeValue(backend, "rotation", transform.Rotation) &&
				HE::Serialization::DeserializeValue(backend, "scale", transform.Scale);
			backend.EndObject();
			return success;
		}

		if (componentName == "CameraComponent") {
			auto& camera = *static_cast<HE::Rendering::CameraComponent*>(component);
			backend.BeginObject(componentName);
			bool success = false;
			if (backend.HasField("primary")) {
				success = backend.Deserialize("primary", camera.Primary) || success;
			}
			if (backend.HasField("fixed_aspect_ratio")) {
				success = backend.Deserialize("fixed_aspect_ratio", camera.FixedAspectRatio) || success;
			}
			backend.EndObject();
			return success;
		}

		if (componentName == "MeshComponent") {
			auto& mesh = *static_cast<HE::Rendering::MeshComponent*>(component);
			backend.BeginObject(componentName);
			const bool success = backend.Deserialize("mesh_asset", mesh.MeshAssetName);
			backend.EndObject();
			return success;
		}

		if (componentName == "MaterialComponent") {
			auto& material = *static_cast<HE::Rendering::MaterialComponent*>(component);
			backend.BeginObject(componentName);
			bool success = false;
			if (backend.HasField("material_instance")) {
				success = HE::Serialization::DeserializeValue(backend, "material_instance", material.MaterialInstance);
			}
			backend.EndObject();
			return success;
		}

		return false;
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
			if (metadata == nullptr || component == nullptr || !metadata->Serialize) {
				continue;
			}

			metadata->Serialize(backend, metadata->TypeName, component);
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
			for (const auto& componentName : componentNames) {
				const auto* metadata = componentRegistry.FindByName(componentName);
				if (metadata == nullptr || metadata->ConstructDefault == nullptr ||
					metadata->Destroy == nullptr || !metadata->Deserialize || !metadata->AddCopyToWorld) {
					continue;
				}

				void* component = metadata->ConstructDefault();
				const bool success = metadata->Deserialize(backend, componentName, component) ||
					DeserializeLegacySceneComponent(backend, componentName, component);
				if (success) {
					metadata->AddCopyToWorld(scene.GetWorld(), entity.GetId(), component);
				}
				metadata->Destroy(component);
			}
			backend.EndObject();
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
