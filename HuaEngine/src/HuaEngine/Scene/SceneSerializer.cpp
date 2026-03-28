#include "enginepch.h"
#include "SceneSerializer.h"

#include <functional>
#include <unordered_map>

#include "HuaEngine/Math/Math.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	void SerializeNameComponent(HE::Serialization::SerializationBackend& backend, const HE::NameComponent& component) {
		backend.Serialize("name", component.Name);
	}

	bool DeserializeNameComponent(HE::Serialization::SerializationBackend& backend, HE::NameComponent& component) {
		return HE::Serialization::DeserializeValue(backend, "name", component.Name);
	}

	void SerializeTransformComponent(HE::Serialization::SerializationBackend& backend, const HE::TransformComponent& component) {
		HE::Serialization::SerializeValue(backend, "position", component.Position);
		HE::Serialization::SerializeValue(backend, "rotation", component.Rotation);
		HE::Serialization::SerializeValue(backend, "scale", component.Scale);
	}

	bool DeserializeTransformComponent(HE::Serialization::SerializationBackend& backend, HE::TransformComponent& component) {
		bool success = true;
		success &= HE::Serialization::DeserializeValue(backend, "position", component.Position);
		success &= HE::Serialization::DeserializeValue(backend, "rotation", component.Rotation);
		success &= HE::Serialization::DeserializeValue(backend, "scale", component.Scale);
		return success;
	}

	void SerializeCameraComponent(HE::Serialization::SerializationBackend& backend, const HE::Rendering::CameraComponent& component) {
		backend.Serialize("primary", component.Primary);
		backend.Serialize("fixed_aspect_ratio", component.FixedAspectRatio);
	}

	bool DeserializeCameraComponent(HE::Serialization::SerializationBackend& backend, HE::Rendering::CameraComponent& component) {
		bool success = true;
		success &= HE::Serialization::DeserializeValue(backend, "primary", component.Primary);
		success &= HE::Serialization::DeserializeValue(backend, "fixed_aspect_ratio", component.FixedAspectRatio);
		return success;
	}

	void SerializeMeshComponent(HE::Serialization::SerializationBackend& backend, const HE::Rendering::MeshComponent& component) {
		backend.Serialize("mesh_asset", component.MeshAssetName);
	}

	bool DeserializeMeshComponent(HE::Serialization::SerializationBackend& backend, HE::Rendering::MeshComponent& component) {
		return HE::Serialization::DeserializeValue(backend, "mesh_asset", component.MeshAssetName);
	}

	void SerializeMaterialComponent(HE::Serialization::SerializationBackend& backend, const HE::Rendering::MaterialComponent& component) {
		HE::Serialization::SerializeValue(backend, "material_instance", component.MaterialInstance);
	}

	bool DeserializeMaterialComponent(HE::Serialization::SerializationBackend& backend, HE::Rendering::MaterialComponent& component) {
		return HE::Serialization::DeserializeValue(backend, "material_instance", component.MaterialInstance);
	}
}

namespace HE {
	struct ComponentSerializers {
		using SerializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity, const std::string&)>;
		using DeserializeFunc = std::function<bool(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity, const std::string&)>;

		std::unordered_map<entt::id_type, SerializeFunc> SerializeByTypeId;
		std::unordered_map<std::string, DeserializeFunc> DeserializeByName;
		std::unordered_map<entt::id_type, std::string> TypeIdToName;

		template<typename T, typename SerializeBody, typename DeserializeBody>
		void RegisterComponent(const std::string& componentName, SerializeBody&& serializeBody, DeserializeBody&& deserializeBody) {
			constexpr auto typeId = entt::type_hash<T>::value();
			auto serializeFn = std::function<void(HE::Serialization::SerializationBackend&, const T&)>(std::forward<SerializeBody>(serializeBody));
			auto deserializeFn = std::function<bool(HE::Serialization::SerializationBackend&, T&)>(std::forward<DeserializeBody>(deserializeBody));

			TypeIdToName[typeId] = componentName;

			SerializeByTypeId[typeId] = [componentName, serialize = serializeFn](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity, const std::string&) {
				if (!registry.all_of<T>(entity)) {
					return;
				}

				backend.BeginObject(componentName);
				serialize(backend, registry.get<T>(entity));
				backend.EndObject();
			};

			DeserializeByName[componentName] = [deserialize = deserializeFn](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity, const std::string& fieldName) {
				if (!backend.HasField(fieldName)) {
					return false;
				}

				T component{};
				backend.BeginObject(fieldName);
				const bool success = deserialize(backend, component);
				backend.EndObject();

				if (success) {
					registry.emplace_or_replace<T>(entity, std::move(component));
				}
				return success;
			};
		}

		const std::string& GetComponentName(entt::id_type typeId) const {
			static const std::string kEmpty;
			const auto it = TypeIdToName.find(typeId);
			return it != TypeIdToName.end() ? it->second : kEmpty;
		}

		static ComponentSerializers& Instance() {
			static ComponentSerializers instance;
			static bool initialized = false;

			if (!initialized) {
				instance.RegisterComponent<NameComponent>("NameComponent", SerializeNameComponent, DeserializeNameComponent);
				instance.RegisterComponent<TransformComponent>("TransformComponent", SerializeTransformComponent, DeserializeTransformComponent);
				instance.RegisterComponent<Rendering::CameraComponent>("CameraComponent", SerializeCameraComponent, DeserializeCameraComponent);
				instance.RegisterComponent<Rendering::MeshComponent>("MeshComponent", SerializeMeshComponent, DeserializeMeshComponent);
				instance.RegisterComponent<Rendering::MaterialComponent>("MaterialComponent", SerializeMaterialComponent, DeserializeMaterialComponent);
				initialized = true;
			}

			return instance;
		}
	};

	static void SerializeEntity(HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
		auto& serializers = ComponentSerializers::Instance();

		backend.BeginObject("");
		backend.Serialize("id", static_cast<uint32_t>(entity));
		backend.BeginObject("components");

		for (auto&& [typeId, storage] : registry.storage()) {
			if (!storage.contains(entity)) {
				continue;
			}

			const auto& componentName = serializers.GetComponentName(typeId);
			if (componentName.empty()) {
				continue;
			}

			const auto serializerIt = serializers.SerializeByTypeId.find(typeId);
			if (serializerIt == serializers.SerializeByTypeId.end()) {
				continue;
			}

			serializerIt->second(backend, registry, entity, componentName);
		}

		backend.EndObject();
		backend.EndObject();
	}

	static entt::entity DeserializeEntity(HE::Serialization::SerializationBackend& backend, Scene& scene) {
		auto& serializers = ComponentSerializers::Instance();
		auto& registry = scene.GetEntityManager().GetRegistry();

		backend.BeginObject("");
		if (backend.HasField("id")) {
			uint32_t ignoredId = 0;
			backend.Deserialize("id", ignoredId);
		}

		Entity entity = scene.GetEntityManager().CreateEntity();

		if (backend.HasField("components")) {
			backend.BeginObject("components");
			const auto componentNames = backend.GetObjectKeys();
			for (const auto& componentName : componentNames) {
				const auto deserializeIt = serializers.DeserializeByName.find(componentName);
				if (deserializeIt == serializers.DeserializeByName.end()) {
					continue;
				}

				deserializeIt->second(backend, registry, static_cast<entt::entity>(entity), componentName);
			}
			backend.EndObject();
		}

		backend.EndObject();
		return entity;
	}
}

namespace HE::Serialization {
	void Serializer<Scene>::Serialize(SerializationBackend& backend, const std::string& name, const Scene& scene) {
		if (!name.empty()) {
			backend.BeginObject(name);
		}

		backend.Serialize("name", scene.GetName().empty() ? "Untitled Scene" : scene.GetName());
		backend.Serialize("version", 2);
		backend.BeginArray("entities");

		auto& registry = const_cast<Scene&>(scene).GetEntityManager().GetRegistry();
		uint32_t entityIndex = 0;
		for (auto entity : registry.storage<entt::entity>()) {
			backend.BeginArrayElement(entityIndex++);
			HE::SerializeEntity(backend, registry, entity);
			backend.EndArrayElement();
		}

		backend.EndArray();

		if (!name.empty()) {
			backend.EndObject();
		}
	}

	bool Serializer<Scene>::Deserialize(SerializationBackend& backend, const std::string& name, Scene& scene) {
		if (!name.empty()) {
			backend.BeginObject(name);
		}

		auto& registry = scene.GetEntityManager().GetRegistry();
		std::vector<entt::entity> entities;
		for (auto entity : registry.storage<entt::entity>()) {
			entities.push_back(entity);
		}
		for (auto entity : entities) {
			registry.destroy(entity);
		}

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

		const uint32_t entityCount = static_cast<uint32_t>(backend.GetArraySize("entities"));
		backend.BeginArray("entities");
		for (uint32_t i = 0; i < entityCount; ++i) {
			backend.BeginArrayElement(i);
			HE::DeserializeEntity(backend, scene);
			backend.EndArrayElement();
		}
		backend.EndArray();

		if (!name.empty()) {
			backend.EndObject();
		}

		return true;
	}
}
