#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"
#include <vector>

namespace {
struct Position { float X = 0; };
struct Velocity { float X = 0; };
struct DeferredTag { int Value = 0; };
struct FrameCounter { int Value = 0; };
}

int main() {
    using namespace ECSTestSupport;
    using namespace HE::Ecs;
    EcsContext context({.WorkerCount = 2});
    const auto position = Take(context.Types().Register<Position>("QuerySmoke.Position"));
    const auto velocity = Take(context.Types().Register<Velocity>("QuerySmoke.Velocity"));
    const auto deferred = Take(context.Types().Register<DeferredTag>("QuerySmoke.Deferred"));
    World world(context);
    const auto id = Take(world.CreateEmpty("Mover"));
    Require(world.Find(world.Uuid(id)) == id && world.EntityCount() == 1, "Expected live identity lookup");
    const HE::EntityUuid explicitUuid{0x1111222233334444ULL, 0x5555666677778888ULL};
    const auto explicitId = Take(world.CreateEmpty("Explicit", explicitUuid));
    Require(world.Find(explicitUuid) == explicitId && world.Uuid(explicitId) == explicitUuid && world.Entities().size() == 2,
        "Expected explicit UUID creation and iteration");
    Check(world.Emplace<Position>(id, Position{4}));
    Require(world.Has<Position>(id) && world.TryGet<Position>(id)->X == 4, "Expected component insertion");
    Check(world.Remove(id, position));
    Require(!world.Has<Position>(id), "Expected component removal");
    Check(world.Emplace<Position>(id, Position{1}));
    Check(world.Emplace<Velocity>(id, Velocity{2}));
    auto movement = Take(Query::Create(context, {.Columns = {
        {position, Presence::Required, AccessMode::Write}, {velocity}}}));
    Check(movement.Each(world, [&](QueryRow& row) -> Result<void> {
        Require(row.Entity() == id, "Expected only the matching entity");
        Take(row.Get<Position>(0)).get().X += Take(row.Get<const Velocity>(1)).get().X;
        return {};
    }));
    Require(world.TryGet<Position>(id)->X == 3, "Expected declared read/write query execution");
    Check(world.Destroy(id)); Check(world.Destroy(explicitId));
    Require(!world.IsAlive(id) && !world.TryGet<Position>(id) && world.EntityCount() == 0,
        "Expected destroyed identities to remain invalid");
    const auto target = Take(world.CreateEmpty("Command Target"));
    CommandBuffer addition(context);
    Check(addition.Set(target, Take(OwnedValue::Construct<Position>(*context.Types().Find(position), Position{7}))));
    Require(!world.Has<Position>(target), "Expected deferred writes to remain invisible before playback");
    Check(addition.Playback(world));
    Require(world.TryGet<Position>(target)->X == 7, "Expected deferred owned payload transfer");
    CommandBuffer removal(context);
    Check(removal.Remove(target, position)); Check(removal.Playback(world));
    Require(!world.Has<Position>(target), "Expected deferred component removal");
    CommandBuffer replacement(context);
    const auto temporary = Take(replacement.CreateEmpty("Deferred Entity"));
    Check(replacement.Destroy(target)); Check(replacement.Playback(world));
    const auto created = Take(replacement.Resolve(temporary));
    Require(!world.IsAlive(target) && world.Name(created) == "Deferred Entity", "Expected deferred creation and deletion");
    Check(world.Emplace<Position>(created));
    auto counter = std::make_shared<FrameCounter>();
    const auto resource = Take(context.Resources().Register(counter));
    auto producer = Take(Query::Create(context, {.Columns = {{position}}, .ResourceAccesses = {{resource, AccessMode::Write}}}));
    auto consumer = Take(Query::Create(context, {.Columns = {{position}}, .ResourceAccesses = {{resource, AccessMode::Read}}}));
    std::vector<int> order;
    Timeline timeline(context, {.Mode = TimelineMode::Parallel});
    Take(timeline.Submit(producer, world, [&](TaskBatch& batch) -> Result<void> {
        Take(batch.View().Resource<FrameCounter>(resource)).get().Value = 42;
        Check(batch.Commands().Set(created, Take(OwnedValue::Construct<DeferredTag>(*context.Types().Find(deferred), DeferredTag{42}))));
        order.push_back(1);
        return {};
    }));
    Take(timeline.Submit(consumer, world, [&](TaskBatch& batch) -> Result<void> {
        Require(Take(batch.View().Resource<const FrameCounter>(resource)).get().Value == 42,
            "Expected the dependent reader to observe the earlier resource writer");
        order.push_back(2);
        return {};
    }));
    Check(timeline.Finish());
    Require(!world.Has<DeferredTag>(created), "Expected structural commands to remain pending after completion");
    Check(timeline.CommitCommands());
    Require(order == std::vector<int>{1, 2} && world.TryGet<DeferredTag>(created)->Value == 42,
        "Expected dependency order and explicit structural commit");
    std::cout << "ECSQuerySchedulerSmoke passed\n";
}
