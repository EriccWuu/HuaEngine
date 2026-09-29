#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedEcs.h>
#include <FixtureEnums/GeneratedEcs.h>
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
		const auto* plain = context.Types().Find<PlainComponent>();
		Require(plain && plain->Descriptor.Reflection &&
			plain->Descriptor.Guid == plain->Descriptor.Reflection->Guid &&
			HE::Refl::HasRuntimeFlag(plain->Descriptor.Reflection->Flags, "Component"),
			"Generated component metadata must share the reflection Guid and flags");
		const auto* label = HE::Refl::FindRuntimeAttribute(plain->Descriptor.Reflection->Fields[0].Attributes, "Inspector.Label");
		Require(label && label->Value == "Text" &&
			HE::Refl::HasRuntimeFlag(plain->Descriptor.Reflection->Fields[1].MetadataFlags, "ScriptVisible"),
			"Generated fields must retain extensible attributes and flags");
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

    void CrossModuleEnums() {
        Require(HE::Generated::Test::RuntimeEnums().empty() &&
            HE::Generated::FixtureEnums::RuntimeTypes().empty() &&
            HE::Generated::FixtureEnums::RuntimeEnums().size() == 1,
            "Each generated module must export only declarations in its entry header");
        EcsContext first({.WorkerCount = 1});
        EcsContext second({.WorkerCount = 1});
        const auto linkedId = Take(first.Types().Register<LinkedComponent>());
        const auto* firstEnum = first.Types().FindEnum("P6Fixture::SharedMode");
        const auto* firstType = first.Types().Find(linkedId);
        Require(firstEnum && firstType && firstType == first.Types().FindByQualifiedName("P6Fixture::LinkedComponent") &&
            firstType->Descriptor.Reflection && firstType->Descriptor.Reflection->Fields.size() == 1 &&
            firstType->Descriptor.Reflection->Fields[0].EnumType == firstEnum,
            "Typed component registration must resolve an enum declared in another module");
        const auto& field = firstType->Descriptor.Reflection->Fields[0];
        LinkedComponent edited;
        int64_t value = -1;
        Require(HE::Refl::SetRuntimeEnumFieldValueByName(field, &edited, "Active") &&
            HE::Refl::GetRuntimeEnumFieldValue(field, &edited, value) &&
            edited.Mode == SharedMode::Active && value == static_cast<int64_t>(SharedMode::Active),
            "Context-bound enum fields must support reflected name editing and value reads");
        Require(!HE::Refl::SetRuntimeEnumFieldValueByName(field, &edited, "Unknown") &&
            edited.Mode == SharedMode::Active &&
            !HE::Refl::SetRuntimeEnumFieldValue(field, &edited, 99) && edited.Mode == SharedMode::Active,
            "Invalid enum names and values must leave the component unchanged");
        Require(HE::Refl::FindRuntimeEnumValueByName(*firstEnum, "Active") != nullptr &&
            HE::Refl::FindRuntimeEnumValueByValue(*firstEnum, static_cast<int64_t>(SharedMode::Active)) != nullptr,
            "Context enum metadata must resolve names and values");
        const auto* firstReflection = firstType->Descriptor.Reflection;
        Check(HE::Generated::Test::RegisterComponents(first.Types()));
        Check(HE::Generated::FixtureEnums::RegisterComponents(first.Types()));
        Check(HE::Generated::FixtureEnums::RegisterComponents(first.Types()));
        Require(first.Types().Find(linkedId) == firstType && firstType->Descriptor.Reflection == firstReflection &&
            first.Types().FindEnum("P6Fixture::SharedMode") == firstEnum && first.Types().AllEnums().size() == 1,
            "Typed and module registration must keep one component and enum identity");
        Check(HE::Generated::FixtureEnums::RegisterComponents(second.Types()));
        Check(HE::Generated::Test::RegisterComponents(second.Types()));
        const auto* secondType = second.Types().Find<LinkedComponent>();
        const auto* secondEnum = second.Types().FindEnum("P6Fixture::SharedMode");
        Require(secondType && secondEnum && secondType->Descriptor.Guid == firstType->Descriptor.Guid &&
            secondType->Descriptor.Reflection->Fields[0].EnumType == secondEnum &&
            secondType->Descriptor.Reflection != firstReflection && secondEnum != firstEnum,
            "Separate Contexts must own distinct reflection and enum descriptors across modules");
    }

#if defined(HUA_META_ENGINE_INTEGRATION)
    void GeneratedSerialization() {
        HE::Serialization::InitializeSerialization();
        HE::Refl::Registry reflection;
        Require(!HE::Generated::Test::RegisterReflection(reflection),
            "Expected generated reflection registration without an ECS Context");
        const PlainReflected plain{"independent reflection", SharedMode::Active};
        const auto plainJson = HE::Serialization::ToJson(plain, reflection);
        const auto plainYaml = HE::Serialization::ToYaml(plain, reflection);
        Require(plainJson.find("independent reflection") != std::string::npos &&
            plainJson.find("Active") != std::string::npos &&
            plainYaml.find("independent reflection") != std::string::npos &&
            plainYaml.find("Active") != std::string::npos,
            "Ordinary generated reflection must serialize without ECS registration");
        PlainReflected plainFromJson, plainFromYaml;
        Require(HE::Serialization::FromJson(plainJson, plainFromJson, reflection) &&
            HE::Serialization::FromYaml(plainYaml, plainFromYaml, reflection) &&
            plainFromJson.Label == plain.Label && plainFromJson.Mode == plain.Mode &&
            plainFromYaml.Label == plain.Label && plainFromYaml.Mode == plain.Mode,
            "Ordinary generated reflection must deserialize JSON and YAML without ECS");
        EcsContext context({.WorkerCount = 1});
        Take(context.Types().Register<LinkedComponent>());
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        const PlainComponent input{"generated plain struct", 73};
        const auto json = HE::Serialization::ToJson(input, context.Reflection());
        const auto yaml = HE::Serialization::ToYaml(input, context.Reflection());
        Require(!json.empty() && !yaml.empty(), "Generated serialization must produce real JSON and YAML documents");
        PlainComponent jsonOutput, yamlOutput;
        const auto jsonRead = HE::Serialization::FromJson(json, jsonOutput, context.Reflection());
        if (!jsonRead) { std::cerr << "Generated JSON input: " << json << '\n'; }
        Require(jsonRead, "Generated plain components must deserialize JSON through their owning Context");
        Require(HE::Serialization::FromYaml(yaml, yamlOutput, context.Reflection()), "Generated plain components must deserialize YAML through their owning Context");
        Require(jsonOutput.Text == input.Text && jsonOutput.Value == input.Value && yamlOutput.Text == input.Text && yamlOutput.Value == input.Value,
            "Generated plain struct runtime descriptors must round-trip non-POD fields");
        const LinkedComponent linked{SharedMode::Active};
        const auto linkedJson = HE::Serialization::ToJson(linked, context.Reflection());
        const auto linkedYaml = HE::Serialization::ToYaml(linked, context.Reflection());
        Require(linkedJson.find("Active") != std::string::npos &&
            linkedYaml.find("Active") != std::string::npos,
            "An enum declared in another module must serialize by name in JSON and YAML");
        LinkedComponent loadedJson, loadedYaml;
        Require(HE::Serialization::FromJson(linkedJson, loadedJson, context.Reflection()) &&
            HE::Serialization::FromYaml(linkedYaml, loadedYaml, context.Reflection()) &&
            loadedJson.Mode == SharedMode::Active && loadedYaml.Mode == SharedMode::Active,
            "A cross-module enum must deserialize through Context metadata in JSON and YAML");
        auto invalidJson = linkedJson;
        auto invalidYaml = linkedYaml;
        invalidJson.replace(invalidJson.find("Active"), 6, "Unknown");
        invalidYaml.replace(invalidYaml.find("Active"), 6, "Unknown");
        LinkedComponent preservedJson{SharedMode::Active}, preservedYaml{SharedMode::Active};
        Require(!HE::Serialization::FromJson(invalidJson, preservedJson, context.Reflection()) &&
            !HE::Serialization::FromYaml(invalidYaml, preservedYaml, context.Reflection()) &&
            preservedJson.Mode == SharedMode::Active && preservedYaml.Mode == SharedMode::Active,
            "Invalid cross-module enum names must not overwrite either serialized component");
    }
#endif

}

void RunMetaGeneratedContracts() {
#if defined(HUA_META_ENGINE_INTEGRATION)
    GeneratedSerialization();
#endif
    Capabilities();
    CacheBindings();
    CrossModuleEnums();
}
