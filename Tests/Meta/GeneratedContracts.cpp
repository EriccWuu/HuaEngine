#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedReflection.h>
#include <Test/Queries.h>
#include "HuaEngine/ECS/Runtime/Timeline.h"
#if defined(HUA_META_ENGINE_INTEGRATION)
#include "HuaEngine/Serialization/Serialization.h"
#endif

#include <cstdlib>
#include <iostream>

namespace {
    using namespace HE::Ecs;
    using namespace P6Fixture;
    void Require(bool condition, const char* message) {
        if (!condition) { std::cerr << "[GeneratedContracts] " << message << '\n'; std::exit(1); }
    }
    template<typename T> T Take(Result<T> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
        return std::move(result).Value();
    }
    void Check(Result<void> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
    }
    template<typename T> TypeId Id(EcsContext& context) { return context.Types().Find<T>()->Id; }

    void Capabilities() {
        EcsContext context({.WorkerCount = 1});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        const auto& noDefault = *context.Types().Find<NoDefault>();
        const auto& moveOnly = *context.Types().Find<MoveOnly>();
        const auto& immovable = *context.Types().Find<Immovable>();
        Require(!noDefault.Descriptor.CanDefaultConstruct() && noDefault.Descriptor.CanCopy(), "Generated no-default capability must remain explicit");
        Require(moveOnly.Descriptor.CanDefaultConstruct() && !moveOnly.Descriptor.CanCopy(), "Generated move-only component must not require copying");
        Require(immovable.Descriptor.Storage == StorageKind::Indirect && !immovable.Descriptor.CanDefaultConstruct() && !immovable.Descriptor.CanCopy(),
            "Generated immovable component must use the indirect policy");
        Require(!OwnedValue::Default(noDefault), "Unavailable generated default construction must fail");
        auto value = Take(OwnedValue::Default(moveOnly));
        Require(!value.Clone(), "Unavailable generated copy must fail");
        World world(context);
        const auto entity = Take(world.CreateEmpty());
        Check(world.Emplace<NoDefault>(entity, 7));
        Check(world.Emplace<MoveOnly>(entity));
        Check(world.Emplace<Immovable>(entity, 19));
        Require(world.TryGet<NoDefault>(entity)->Value == 7 && *world.TryGet<MoveOnly>(entity)->Owner == 42 &&
            world.TryGet<Immovable>(entity)->Value == 19, "Generated lifecycle operations must construct actual C++ values");
    }

    void CacheBindings() {
        EcsContext context({.WorkerCount = 1});
        EcsContext other({.WorkerCount = 1});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        Check(HE::Generated::Test::RegisterComponents(other.Types()));
        World first(context), second(context);
        QuerySpec spec;
        spec.Columns.push_back({Id<PlainComponent>(context), Presence::Required, AccessMode::Read});
        auto* original = Take(context.FindOrCreateGeneratedQuery("binding", spec));
        Require(Take(context.FindOrCreateGeneratedQuery("binding", spec)) == original, "Identical generated cache bindings must reuse the entry");
        auto different = spec;
        different.Columns[0].Access = AccessMode::Write;
        Require(Take(context.FindOrCreateGeneratedQuery("binding", different)) != original, "Access permissions must be part of the cache key");
        Require(Take(context.FindOrCreateGeneratedQuery(std::string_view("binding\0tail", 12), spec)) != original,
            "Declaration keys must compare their entire length");
        spec.RandomAccesses.push_back({&first, Id<ReadComponent>(context), AccessMode::Read});
        auto* boundFirst = Take(context.FindOrCreateGeneratedQuery("random", spec));
        spec.RandomAccesses[0].Target = &second;
        Require(Take(context.FindOrCreateGeneratedQuery("random", spec)) != boundFirst, "Target World identity must be part of the cache key");
        QuerySpec otherSpec;
        otherSpec.Columns.push_back({Id<PlainComponent>(other), Presence::Required, AccessMode::Read});
        Require(Take(other.FindOrCreateGeneratedQuery("binding", otherSpec)) != original, "Generated cache entries must remain Context-local");
    }

#if defined(HUA_META_ENGINE_INTEGRATION)
    void GeneratedSerialization() {
        HE::Serialization::InitializeSerialization();
        EcsContext context({.WorkerCount = 1});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        const PlainComponent input{"generated plain struct", 73};
        const auto json = HE::Serialization::ToJson(input, context.Types());
        const auto yaml = HE::Serialization::ToYaml(input, context.Types());
        Require(!json.empty() && !yaml.empty(), "Generated serialization must produce real JSON and YAML documents");
        PlainComponent jsonOutput, yamlOutput;
        const auto jsonRead = HE::Serialization::FromJson(json, jsonOutput, context.Types());
        if (!jsonRead) { std::cerr << "Generated JSON input: " << json << '\n'; }
        Require(jsonRead, "Generated plain components must deserialize JSON through their owning Context");
        Require(HE::Serialization::FromYaml(yaml, yamlOutput, context.Types()), "Generated plain components must deserialize YAML through their owning Context");
        Require(jsonOutput.Text == input.Text && jsonOutput.Value == input.Value && yamlOutput.Text == input.Text && yamlOutput.Value == input.Value,
            "Generated plain struct runtime descriptors must round-trip non-POD fields");
    }
#endif

    void AllParameters(TimelineMode mode) {
        EcsContext context({.WorkerCount = 2});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        World world(context), randomWorld(context);
        const auto target = Take(randomWorld.CreateEmpty());
        Check(randomWorld.Emplace<ReadComponent>(target, ReadComponent{11}));
        Check(randomWorld.Emplace<OptionalWrite>(target, OptionalWrite{20}));
        const auto shared = Take(context.Resources().Register(std::make_shared<SharedComponent>(SharedComponent{8})));
        const auto amount = Take(context.Resources().Register(std::make_shared<int>(13)));
        auto totalValue = std::make_shared<int>(0);
        const auto total = Take(context.Resources().Register(totalValue));
        const auto make = [&](int initial) {
            const auto entity = Take(world.CreateEmpty());
            Check(world.Emplace<PlainComponent>(entity, PlainComponent{"fixture", initial}));
            Check(world.Emplace<ReadComponent>(entity, ReadComponent{2}));
            Check(world.SetTag(entity, Id<Active>(context)));
            Check(world.SetShared(entity, {Id<SharedComponent>(context), shared}));
            return entity;
        };
        const auto normal = make(0), optionalMissing = make(10), excluded = make(20), disabled = make(30);
        Check(world.Emplace<OptionalRead>(normal, OptionalRead{3}));
        Check(world.Emplace<OptionalWrite>(normal, OptionalWrite{4}));
        Check(world.SetTag(excluded, Id<Excluded>(context)));
        Check(world.SetEnabled(disabled, false));
        ChangedState changed;
        Timeline timeline(context, {.Mode = mode});
        const auto submit = [&] {
            return P6Fixture::Queries::AllArguments(timeline, world, target,
                randomWorld, randomWorld, amount, total, shared, changed);
        };
        Take(submit());
        Check(timeline.Finish());
        Require(*totalValue == 2 && world.TryGet<PlainComponent>(normal)->Value == 29 &&
            world.TryGet<PlainComponent>(optionalMissing)->Value == 36, "Required, optional, random and resource parameters must use their declared bindings");
        Require(world.TryGet<OptionalWrite>(normal)->Value == 5 && randomWorld.TryGet<OptionalWrite>(target)->Value == 24,
            "Optional and random writable views must refer to the correct Worlds");
        Require(world.TryGet<PlainComponent>(excluded)->Value == 20 && world.TryGet<PlainComponent>(disabled)->Value == 30,
            "Tag, exclusion and enabled filters must restrict generated queries");
        Take(submit());
        Check(timeline.Finish());
        Require(*totalValue == 2, "Changed observers must acknowledge the successful generated round");
        Check(timeline.CommitCommands());
        Require(world.EntityCount() == 6, "Generated Commands must remain deferred until commit");
        Check(world.SetComponentEnabled(normal, Id<PlainComponent>(context), false));
        Take(P6Fixture::Queries::IncludeAll(timeline, world));
        Check(timeline.Finish());
        Require(world.TryGet<PlainComponent>(normal)->Value == 129 && world.TryGet<PlainComponent>(disabled)->Value == 130,
            "Generated explicit enabled-mask overrides must apply to entity and component masks");
        EcsContext other({.WorkerCount = 1});
        Check(HE::Generated::Test::RegisterComponents(other.Types()));
        World foreign(other);
        Require(!P6Fixture::Queries::AllArguments(timeline, world, target,
            foreign, randomWorld, amount, total, shared, changed), "Generated random bindings must reject a foreign Context");
        Require(!P6Fixture::Queries::AllArguments(timeline, world, target,
            randomWorld, randomWorld, shared, total, shared, changed), "Generated resource bindings must reject the wrong native type");
    }
}

void RunGeneratedContracts() {
#if defined(HUA_META_ENGINE_INTEGRATION)
    GeneratedSerialization();
#endif
    Capabilities();
    CacheBindings();
    AllParameters(TimelineMode::Serial);
    AllParameters(TimelineMode::Parallel);
}
