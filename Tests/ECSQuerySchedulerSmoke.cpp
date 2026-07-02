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

	const HE::EntityUuid explicitUuid{0x1111222233334444ULL, 0x5555666677778888ULL};
	auto explicitEntity = world.CreateEntityWithUuid(explicitUuid, "Explicit");
	assert(explicitEntity.IsValid());
	assert(world.GetEntity(explicitUuid).GetId() == explicitEntity.GetId());
	assert(world.GetEntity(explicitEntity.GetId()).GetUuid() == explicitUuid);

	int aliveEntityCount = 0;
	world.ForEachEntity([&](HE::Entity aliveEntity) {
		assert(aliveEntity.IsValid());
		++aliveEntityCount;
	});
	assert(aliveEntityCount == 2);

	entity.AddComponent<QueryPosition>(QueryPosition{4.0f});
	assert(entity.HasComponent<QueryPosition>());
	assert(entity.GetComponent<QueryPosition>().X == 4.0f);
	assert(entity.TryGetComponent<QueryPosition>() != nullptr);
	entity.RemoveComponent<QueryPosition>();
	assert(!entity.HasComponent<QueryPosition>());

	world.AddComponent<QueryPosition>(id, QueryPosition{1.0f});
	world.AddComponent<QueryVelocity>(id, QueryVelocity{2.0f});

	QueryPosition* position = world.TryGetComponent<QueryPosition>(id);
	assert(position != nullptr);
	assert(position->X == 1.0f);

	world.Query<QueryPosition, HE::Read<QueryVelocity>>().ForEach(
		[&](HE::Entity found, QueryPosition& pos, const QueryVelocity& vel) {
			assert(found.GetId() == id);
			pos.X += vel.X;
		});

	position = world.TryGetComponent<QueryPosition>(id);
	assert(position != nullptr);
	assert(position->X == 3.0f);

	world.DestroyEntity(id);
	assert(!world.IsAlive(id));
	assert(world.TryGetComponent<QueryPosition>(id) == nullptr);
	world.DestroyEntity(explicitEntity.GetId());
	assert(world.GetEntityCount() == 0);

	std::cout << "ECSQuerySchedulerSmoke passed" << std::endl;
	return 0;
}
