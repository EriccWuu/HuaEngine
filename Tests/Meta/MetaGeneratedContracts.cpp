#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedReflection.h>
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
        auto* original = Take(context.FindOrCreateQuery("binding", spec));
        Require(Take(context.FindOrCreateQuery("binding", spec)) == original, "Identical Job cache bindings must reuse the entry");
        auto different = spec;
        different.Columns[0].Access = AccessMode::Write;
        Require(Take(context.FindOrCreateQuery("binding", different)) != original, "Access permissions must be part of the cache key");
        Require(Take(context.FindOrCreateQuery(std::string_view("binding\0tail", 12), spec)) != original,
            "Declaration keys must compare their entire length");
        spec.RandomAccesses.push_back({&first, Id<ReadComponent>(context), AccessMode::Read});
        auto* boundFirst = Take(context.FindOrCreateQuery("random", spec));
        spec.RandomAccesses[0].Target = &second;
        Require(Take(context.FindOrCreateQuery("random", spec)) != boundFirst, "Target World identity must be part of the cache key");
        QuerySpec otherSpec;
        otherSpec.Columns.push_back({Id<PlainComponent>(other), Presence::Required, AccessMode::Read});
        Require(Take(other.FindOrCreateQuery("binding", otherSpec)) != original, "Job cache entries must remain Context-local");
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

}

void RunMetaGeneratedContracts() {
#if defined(HUA_META_ENGINE_INTEGRATION)
    GeneratedSerialization();
#endif
    Capabilities();
    CacheBindings();
}
