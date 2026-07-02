#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"

#include <cassert>
#include <filesystem>

int main() {
	HE::Serialization::InitializeSerialization();

	HE::Scene scene("Serialized ECS Scene");
	auto entity = scene.GetWorld().CreateEntity("Camera");
	auto& transform = entity.AddComponent<HE::TransformComponent>();
	transform.Position.x = 3.0f;
	transform.Position.y = 4.0f;

	const auto uuid = entity.GetUuid();
	const std::filesystem::path path = "ecs_scene_serialization_smoke.scene";
	const bool saved = HE::Serialization::SaveScene(scene, path.string());
	assert(saved);

	HE::Scene loaded;
	const bool loadedOk = HE::Serialization::LoadScene(path.string(), loaded);
	assert(loadedOk);
	assert(loaded.GetName() == "Serialized ECS Scene");
	assert(loaded.GetWorld().GetEntityCount() == 1);

	auto loadedEntity = loaded.GetWorld().GetEntity(uuid);
	assert(loadedEntity.IsValid());
	assert(loadedEntity.GetName() == "Camera");
	assert(loadedEntity.HasComponent<HE::TransformComponent>());
	const auto& loadedTransform = loadedEntity.GetComponent<HE::TransformComponent>();
	assert(loadedTransform.Position.x == 3.0f);
	assert(loadedTransform.Position.y == 4.0f);

	std::filesystem::remove(path);
	return 0;
}
