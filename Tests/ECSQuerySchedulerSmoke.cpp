#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "HuaEngine/ECS/Scheduler.h"
#include "HuaEngine/ECS/World.h"

namespace {
	struct QueryPosition {
		float X = 0.0f;
	};

	struct QueryVelocity {
		float X = 0.0f;
	};

	struct DeferredTag {
		int Value = 0;
	};

	struct FrameCounter {
		int Value = 0;
	};

	class FirstSystem final : public HE::System {
	public:
		FirstSystem(std::vector<int>& order, HE::EntityId target)
			: m_Order(order), m_Target(target) {}

		HE::SystemDescriptor Describe() const override {
			HE::SystemDescriptor descriptor;
			descriptor.Name = "FirstSystem";
			descriptor.Stage = HE::SystemStage::Update;
			descriptor.ResourceWrites = { "Smoke.FrameCounter" };
			return descriptor;
		}

		void Update(HE::SystemContext& context) override {
			assert(context.DeltaTime() > 0.0f);
			context.Commands().AddComponent<DeferredTag>(m_Target, DeferredTag{42});
			context.Frame().GetOrCreate<FrameCounter>().Value = 42;
			m_Order.push_back(1);
		}

	private:
		std::vector<int>& m_Order;
		HE::EntityId m_Target;
	};

	class SecondSystem final : public HE::System {
	public:
		explicit SecondSystem(std::vector<int>& order)
			: m_Order(order) {}

		HE::SystemDescriptor Describe() const override {
			HE::SystemDescriptor descriptor;
			descriptor.Name = "SecondSystem";
			descriptor.Stage = HE::SystemStage::Update;
			descriptor.ResourceReads = { "Smoke.FrameCounter" };
			return descriptor;
		}

		void Update(HE::SystemContext& context) override {
			auto* tag = context.WorldRef().TryGetComponent<DeferredTag>(m_Target);
			assert(tag != nullptr);
			assert(tag->Value == 42);
			const auto* frameCounter = context.Frame().TryGet<FrameCounter>();
			assert(frameCounter != nullptr);
			assert(frameCounter->Value == 42);
			m_Order.push_back(2);
		}

		void SetTarget(HE::EntityId target) {
			m_Target = target;
		}

	private:
		std::vector<int>& m_Order;
		HE::EntityId m_Target;
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

	auto commandEntity = world.CreateEntity("Command Target");
	HE::CommandBuffer commandBuffer;
	commandBuffer.AddComponent<QueryPosition>(commandEntity.GetId(), QueryPosition{7.0f});
	assert(!commandEntity.HasComponent<QueryPosition>());
	commandBuffer.Playback(world);
	assert(commandEntity.GetComponent<QueryPosition>().X == 7.0f);

	commandBuffer.RemoveComponent<QueryPosition>(commandEntity.GetId());
	commandBuffer.Playback(world);
	assert(!commandEntity.HasComponent<QueryPosition>());

	const HE::EntityUuid createdUuid = commandBuffer.CreateEntity("Deferred Entity");
	commandBuffer.DestroyEntity(commandEntity.GetId());
	commandBuffer.Playback(world);
	assert(!commandEntity.IsValid());
	assert(world.GetEntity(createdUuid).IsValid());
	assert(world.GetEntity(createdUuid).GetName() == "Deferred Entity");

	std::vector<int> order;
	HE::Scheduler scheduler;
	auto schedulerEntity = world.CreateEntity("Scheduler Target");
	auto secondSystem = std::make_shared<SecondSystem>(order);
	secondSystem->SetTarget(schedulerEntity.GetId());
	scheduler.AddSystem(secondSystem);
	scheduler.AddSystem(std::make_shared<FirstSystem>(order, schedulerEntity.GetId()));
	HE::SystemContext context{world, 1.0f / 60.0f};
	const bool sorted = scheduler.Build();
	assert(sorted);
	scheduler.Update(context);
	assert((order == std::vector<int>{1, 2}));

	std::cout << "ECSQuerySchedulerSmoke passed" << std::endl;
	return 0;
}
