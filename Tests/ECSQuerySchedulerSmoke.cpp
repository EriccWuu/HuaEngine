#include <cassert>
#include <iostream>

#include "HuaEngine/ECS/World.h"

namespace {
	struct QueryPosition {
		float X = 0.0f;
	};

	struct QueryVelocity {
		float X = 0.0f;
	};
}

int main() {
	HE::World world;
	assert(world.GetEntityCount() == 0);

	auto entity = world.CreateEntity("Mover");
	assert(entity.IsValid());

	const HE::EntityId id = entity.GetId();
	const HE::EntityUuid uuid = entity.GetUuid();
	assert(id);
	assert(uuid != HE::EntityUuid{});
	assert(world.GetUuid(id) == uuid);
	assert(world.FindEntity(uuid) == id);

	int aliveEntityCount = 0;
	world.ForEachEntity([&](HE::Entity aliveEntity) {
		assert(aliveEntity.IsValid());
		assert(aliveEntity.GetId() == id);
		assert(aliveEntity.GetUuid() == uuid);
		++aliveEntityCount;
	});
	assert(aliveEntityCount == 1);

	world.AddComponent<QueryPosition>(id, QueryPosition{1.0f});
	world.AddComponent<QueryVelocity>(id, QueryVelocity{2.0f});

	QueryPosition* position = world.TryGetComponent<QueryPosition>(id);
	assert(position != nullptr);
	assert(position->X == 1.0f);

	world.Query<QueryPosition, HE::Read<QueryVelocity>>().ForEach(
		[](HE::EntityId entityId, QueryPosition& pos, const QueryVelocity& vel) {
			assert(entityId);
			pos.X += vel.X;
		});

	position = world.TryGetComponent<QueryPosition>(id);
	assert(position != nullptr);
	assert(position->X == 3.0f);

	world.DestroyEntity(id);
	assert(!world.IsAlive(id));
	assert(world.TryGetComponent<QueryPosition>(id) == nullptr);
	assert(world.GetEntityCount() == 0);

	std::cout << "ECSQuerySchedulerSmoke passed" << std::endl;
	return 0;
}
