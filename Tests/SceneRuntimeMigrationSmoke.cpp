#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Scene/Scene.h"

#include <memory>
#include <memory_resource>
#include <new>
#include <type_traits>

namespace {
using namespace ECSTestSupport;
using namespace HE::Ecs;
static_assert(std::is_same_v<decltype(std::declval<HE::Scene&>().GetWorld()), World&>);

struct UnrelatedSceneTag {};

void VerifyDefaultTransformFailureCleanup() {
    struct FailSecondBlock final : std::pmr::memory_resource {
        std::pmr::memory_resource* Upstream = std::pmr::get_default_resource();
        size_t Attempts = 0;
        size_t LiveBlocks = 0;
        void* do_allocate(size_t bytes, size_t alignment) override {
            if (++Attempts == 2) throw std::bad_alloc{};
            void* block = Upstream->allocate(bytes, alignment);
            ++LiveBlocks;
            return block;
        }
        void do_deallocate(void* block, size_t bytes, size_t alignment) override {
            Require(LiveBlocks > 0, "Expected exact ownership of injected Scene chunk allocations");
            --LiveBlocks;
            Upstream->deallocate(block, bytes, alignment);
        }
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    } resource;
    struct DefaultResourceGuard {
        std::pmr::memory_resource* Previous;
        ~DefaultResourceGuard() { std::pmr::set_default_resource(Previous); }
    } restore{std::pmr::set_default_resource(&resource)};
    {
        EcsContext context({.WorkerCount = 1});
        HE::Scene scene(context, "default Transform failure");
        const HE::EntityUuid uuid{0x718ac0deULL, 99};
        const auto failed = scene.CreateEntity("must roll back", uuid);
        Require(!failed && resource.Attempts == 2 && scene.GetWorld().EntityCount() == 0 && !scene.GetWorld().Find(uuid),
            "Expected failed Transform target-chunk allocation to remove the newly created entity and UUID");
        const auto retry = Take(scene.CreateEntity("retry", uuid));
        Require(scene.GetWorld().Find(uuid) == retry && scene.GetWorld().Has<HE::TransformComponent>(retry) &&
            scene.GetWorld().EntityCount() == 1, "Expected a failed Scene create to leave its UUID reusable");
    }
    Require(resource.LiveBlocks == 0, "Expected the Scene to release every injected chunk before restoring the PMR default");
}

void VerifyCreationAndIdentity() {
    EcsContext context({.WorkerCount = 2});
    HE::Scene first(context, "first");
    HE::Scene second(context, "second");
    Require(&first.GetWorld().Context() == &context && &second.GetWorld().Types() == &context.Types(),
        "Expected both scenes to borrow the same Context registry");
    const HE::EntityUuid uuid{0x718ac0deULL, 1};
    const auto id = Take(first.CreateEntity("created", uuid));
    auto& world = first.GetWorld();
    const auto* transformType = world.Types().Find<HE::TransformComponent>();
    Require(transformType && transformType->Descriptor.Reflection,
        "Expected scene construction to register canonical generated metadata");
    Require(world.ListTypes(id) == std::vector<TypeId>{transformType->Id},
        "Expected exactly one default Transform on scene creation");
    {
        auto edit = Take(WorldEditScope::Acquire(world));
        edit.Get().TryGet<HE::TransformComponent>(id)->Position = {3, 5, 7};
    }
    Require(Take(first.CreateEntity("must not replace", uuid)) == id && world.Name(id) == "created" &&
        world.TryGet<HE::TransformComponent>(id)->Position == glm::vec3(3, 5, 7),
        "Expected duplicate UUID creation to preserve identity, name and component values");
    {
        auto edit = Take(WorldEditScope::Acquire(world));
        const auto scoped = Take(HE::Scene::CreateEntityInScope(edit.Get(), "scoped"));
        Require(edit.Get().Has<HE::TransformComponent>(scoped), "Expected creation inside an existing EditScope");
    }
    Require(world.FindByIndex(id.Index) == id, "Expected index lookup to recover the full live generation");
    Check(world.Destroy(id));
    Require(!world.FindByIndex(id.Index), "Expected a freed slot to have no live index lookup");
    const auto replacement = Take(first.CreateEntity("replacement"));
    Require(replacement.Index == id.Index && replacement.Generation != id.Generation &&
        world.FindByIndex(id.Index) == replacement && !world.IsAlive(id),
        "Expected slot reuse to retain the new generation without reviving the old ID");
    Check(world.Clear());
    Require(!world.FindByIndex(replacement.Index), "Expected Clear to invalidate index lookup");
    Check(first.Update());
    Check(second.Update());
    first.OnRuntimeStart();
    Check(first.OnUpdate(1.0f / 60.0f));
    first.OnRuntimeStop();
    Check(second.OnUpdate());
    Require(!context.HasScheduledWork(), "Expected each scene tick to finish its local timeline");
}

void VerifyCreationPreservesUnrelatedChangedObservation() {
    EcsContext context({.WorkerCount = 1});
    HE::Scene scene(context, "changed creation");
    auto& world = scene.GetWorld();
    const auto tag = Take(context.Types().Register<UnrelatedSceneTag>(
        TypeGuid::FromName("Scene.UnrelatedCreationTag"), "Scene.UnrelatedCreationTag", true));
    const auto* transform = context.Types().Find<HE::TransformComponent>();
    Require(transform != nullptr, "Expected the Scene to register Transform before Changed observation");

    const auto tagged = Take(scene.CreateEntity("observed"));
    Check(world.SetTag(tagged, tag));
    QuerySpec spec;
    spec.Columns = {{transform->Id}};
    spec.Required = {tag};
    spec.ChangedTypes = {transform->Id};
    auto query = Take(Query::Create(context, spec));
    ChangedState observed;
    auto observe = [&]() {
        size_t visits = 0;
        Check(query.Each(world, [&](QueryRow& row) -> Result<void> {
            Require(row.Entity() == tagged, "Expected only the tagged entity in the Changed query");
            ++visits;
            return {};
        }, &observed));
        return visits;
    };
    Require(observe() == 1 && observe() == 0, "Expected the initial tagged chunk to become clean");
    const auto unrelated = Take(scene.CreateEntity("unrelated"));
    Require(unrelated != tagged && world.Has<HE::TransformComponent>(unrelated),
        "Expected ordinary Scene creation to retain its default Transform");
    Require(observe() == 0, "Expected creating an entity in another Group not to dirty the tagged Transform chunk");
    world.TryGet<HE::TransformComponent>(tagged)->Position.x += 1.0f;
    Require(observe() == 1, "Expected a real write to the tagged Transform chunk to remain visible to Changed");
}

void VerifyExternalTimelineAndLifetime() {
    EcsContext context({.WorkerCount = 1});
    auto scene = std::make_unique<HE::Scene>(context, "borrowed context");
    const auto id = Take(scene->CreateEntity("pending"));
    const auto type = context.Types().Find<HE::TransformComponent>()->Id;
    auto query = Take(Query::Create(context, {.Columns = {{type, Presence::Required, AccessMode::Write}}}));
    {
        Timeline timeline(context, {.Mode = TimelineMode::Serial});
        const auto task = Take(timeline.Submit(query, scene->GetWorld(), [](TaskBatch& batch) -> Result<void> {
            auto transforms = Take(batch.View().Column<HE::TransformComponent>(0));
            for (size_t row = 0; row < transforms.Size(); ++row) transforms.At(row).Position.x += 11;
            return {};
        }));
        const auto busy = scene->OnUpdate();
        Require(!busy && busy.GetError().Code == ErrorCode::Busy,
            "Expected a scene tick to reject an already active external Timeline");
        scene.reset();
        Require(task.IsComplete() && !context.HasScheduledWork(),
            "Expected scene destruction to drain its borrowed World before releasing storage");
        Check(timeline.Finish());
    }
    HE::Scene replacement(context, "context remains alive");
    Require(Take(replacement.CreateEntity()) != HE::EntityId{}, "Expected Context reuse after scene destruction");
    Check(replacement.OnUpdate());
    (void)id;
}
}

int main() {
    VerifyDefaultTransformFailureCleanup();
    VerifyCreationAndIdentity();
    VerifyCreationPreservesUnrelatedChangedObservation();
    VerifyExternalTimelineAndLifetime();
    std::cout << "SceneRuntimeMigrationSmoke passed\n";
}
