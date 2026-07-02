#include "HuaEngine/ECS/World.h"

#include "entt.hpp"

#include <cassert>

struct BackendPosition {
	float X = 0.0f;
};

int main() {
	entt::registry registry;
	const entt::entity rawEntity = registry.create();
	registry.emplace<BackendPosition>(rawEntity, BackendPosition{ 4.0f });
	assert(registry.get<BackendPosition>(rawEntity).X == 4.0f);

	HE::World world;
	auto entity = world.CreateEntity("Backend Entity");
	entity.AddComponent<BackendPosition>(BackendPosition{ 8.0f });
	assert(entity.GetComponent<BackendPosition>().X == 8.0f);

	return 0;
}
